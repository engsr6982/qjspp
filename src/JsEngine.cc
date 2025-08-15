#include "qjspp/JsEngine.hpp"
#include "qjspp/Binding.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/JsScope.hpp"
#include "qjspp/Module.hpp"
#include "qjspp/TaskQueue.hpp"
#include "qjspp/Types.hpp"
#include "qjspp/Values.hpp"
#include "quickjs.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>


namespace qjspp {

JsEngine::PauseGc::PauseGc(JsEngine* engine) : engine_(engine) { engine_->pauseGcCount_++; }
JsEngine::PauseGc::~PauseGc() { engine_->pauseGcCount_--; }

void JsEngine::kTemplateClassFinalizer(JSRuntime*, JSValue val) {
    auto classID = JS_GetClassID(val);
    assert(classID != JS_INVALID_CLASS_ID); // ID 必须有效

    auto opaque = JS_GetOpaque(val, classID);
    if (opaque) {
        auto wrapped = static_cast<WrappedResource*>(opaque);
        // 校验类ID是否匹配
        assert(wrapped->define_->instanceDefine_.classId_ == classID);

        PauseGc pauseGc(const_cast<JsEngine*>(wrapped->engine_));
        delete wrapped;
    }
}
bool JsEngine::kUpdateModuleMainFlag(JSContext* ctx, JSModuleDef* module, bool isMain) {
    auto meta = JS_GetImportMeta(ctx, module);
    if (JS_IsException(meta)) {
        return false;
    }
    JS_DefinePropertyValueStr(ctx, meta, "main", JS_NewBool(ctx, isMain), JS_PROP_C_W_E);
    JS_FreeValue(ctx, meta);
    return true;
}
bool JsEngine::kUpdateModuleUrl(JSContext* ctx, JSModuleDef* module, std::string_view url) {
    auto meta = JS_GetImportMeta(ctx, module);
    if (JS_IsException(meta)) {
        return false;
    }
    JS_DefinePropertyValueStr(ctx, meta, "url", JS_NewString(ctx, url.data()), JS_PROP_C_W_E);
    JS_FreeValue(ctx, meta);
    return true;
}
bool JsEngine::kUpdateModuleMeta(JSContext* ctx, JSModuleDef* module, std::string_view url, bool isMain) {
    return kUpdateModuleUrl(ctx, module, url) && kUpdateModuleMainFlag(ctx, module, isMain);
}


/* ModuleLoader impl */
constexpr std::string_view FilePrefix = "file://";
char* JsEngine::ModuleLoader::normalize(JSContext* ctx, const char* base, const char* name, void* opaque) {
    auto* engine = static_cast<JsEngine*>(opaque);
    // std::cout << "[normalize] base: " << base << ", name: " << name << std::endl;

    std::string_view baseView{base};
    std::string_view nameView{name};

    // 1) 各种奇奇怪怪的 <eval>
    if (baseView.starts_with('<') && baseView.ends_with('>')) {
        return js_strdup(ctx, name);
    }

    // 2) 检查是否是原生模块
    if (engine->nativeModules_.contains(name)) {
        return js_strdup(ctx, name);
    }

    // 3) 检查是否是标准文件协议开头
    if (std::strncmp(name, FilePrefix.data(), FilePrefix.size()) == 0) {
        return js_strdup(ctx, name);
    }

    // 处理相对路径（./ 或 ../）
    std::string baseStr(base);
    if (baseStr.rfind(FilePrefix, 0) == 0) {
        baseStr.erase(0, FilePrefix.size()); // 去掉 file://
    }

    // 获取 base 所在目录
    std::filesystem::path basePath(baseStr);
    basePath = basePath.parent_path();

    // 拼接相对路径
    std::filesystem::path targetPath = basePath / name;

    // 规范化（处理 .. 和 .）
    targetPath = std::filesystem::weakly_canonical(targetPath);

    // 重新加上 file:// 前缀
    std::string fullUrl = std::string(FilePrefix) + targetPath.generic_string();

    return js_strdup(ctx, fullUrl.c_str());
}
JSModuleDef* JsEngine::ModuleLoader::loader(JSContext* ctx, const char* canonical, void* opaque) {
    auto* engine = static_cast<JsEngine*>(opaque);
    // std::cout << "[loader] canonical: " << canonical << std::endl;

    // 1) 检查是否是原生模块
    auto iter = engine->nativeModules_.find(canonical);
    if (iter != engine->nativeModules_.end()) {
        auto module = iter->second;
        return module->init(engine);
    }

    // 2) file:// 协议 => 读取文件并编译
    if (std::strncmp(canonical, FilePrefix.data(), FilePrefix.size()) == 0) {
        std::string   path = canonical + FilePrefix.size();
        std::ifstream ifs(path);
        if (!ifs) {
            JS_ThrowReferenceError(ctx, "Module file not found: %s", path.c_str());
            return nullptr;
        }
        std::string source((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        // 编译模块
        JSValue result =
            JS_Eval(ctx, source.c_str(), source.size(), canonical, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        if (JS_IsException(result)) return nullptr;

        // 更新 import meta
        auto* m = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(result));
        if (!kUpdateModuleMeta(ctx, m, canonical, false)) {
            JS_FreeValue(ctx, result);
            return nullptr;
        }
        JS_FreeValue(ctx, result);
        return m;
    }

    return nullptr;
}

/* JsEngine impl */
JsEngine::JsEngine() : runtime_(JS_NewRuntime()), queue_(std::make_unique<TaskQueue>()) {
    if (runtime_) {
        context_ = JS_NewContext(runtime_);
    }

    if (!runtime_ || !context_) {
        throw std::logic_error("Failed to create JS runtime or context");
    }

#ifdef QJSPP_DEBUG
    JS_SetDumpFlags(runtime_, JS_DUMP_LEAKS | JS_DUMP_ATOM_LEAKS);
#endif

    // 指针数据
    JSClassDef pointer{};
    pointer.class_name = "RawPointer";
    JS_NewClassID(runtime_, &kPointerClassId);
    JS_NewClass(runtime_, kPointerClassId, &pointer);

    // 函数数据
    JSClassDef function{};
    function.class_name = "RawFunction";
    function.finalizer  = [](JSRuntime*, JSValue val) {
        auto id  = JS_GetClassID(val);
        auto ptr = JS_GetOpaque(val, id);
        if (ptr) {
            delete static_cast<FunctionCallback*>(ptr);
        }
    };
    JS_NewClassID(runtime_, &kFunctionDataClassId);
    JS_NewClass(runtime_, kFunctionDataClassId, &function);

    lengthAtom_ = JS_NewAtom(context_, "length");

    JS_SetRuntimeOpaque(runtime_, this);
    JS_SetContextOpaque(context_, this);
    JS_SetModuleLoaderFunc(runtime_, &ModuleLoader::normalize, &ModuleLoader::loader, this);
}

JsEngine::~JsEngine() {
    isDestroying_ = true;
    userData_.reset();
    queue_.reset();

    JS_FreeAtom(context_, lengthAtom_);

    {
        JsScope scope{this};
        for (auto&& [def, obj] : nativeStaticClasses_) {
            obj.reset();
        }
        for (auto&& [def, data] : nativeInstanceClasses_) {
            JS_FreeValue(context_, data.first);
            JS_FreeValue(context_, data.second);
        }
    }

    JS_RunGC(runtime_);
    JS_FreeContext(context_);
    JS_FreeRuntime(runtime_);
}

::JSRuntime* JsEngine::runtime() const { return runtime_; }

::JSContext* JsEngine::context() const { return context_; }

bool JsEngine::isJobPending() const { return JS_IsJobPending(runtime_); }

void JsEngine::pumpJobs() {
    if (isDestroying()) return;

    bool no = false;
    if (JS_IsJobPending(runtime_) && pumpScheduled_.compare_exchange_strong(no, true)) {
        queue_->postTask(
            [](void* data) {
                auto       engine = static_cast<JsEngine*>(data);
                JSContext* ctx    = nullptr;
                JsScope    lock(engine);
                while (JS_ExecutePendingJob(engine->runtime_, &ctx) > 0) {}
                engine->pumpScheduled_ = false;
            },
            this
        );
    }
}

Value JsEngine::eval(String const& code, EvalType type) { return eval(code.value(), "<eval>", type); }
Value JsEngine::eval(String const& code, String const& source, EvalType type) {
    return eval(code.value(), source ? source.value() : "<eval>", type);
}
Value JsEngine::eval(std::string const& code, std::string const& source, EvalType type) {
    auto result = JS_Eval(
        context_,
        code.c_str(),
        code.size(),
        source.c_str(),
        type == EvalType::kGlobal ? JS_EVAL_TYPE_GLOBAL : JS_EVAL_TYPE_MODULE
    );
    JsException::check(result);
    pumpJobs();
    return Value::move<Value>(result);
}

Value JsEngine::loadScript(std::filesystem::path const& path, bool main) {
    if (!std::filesystem::exists(path)) {
        throw JsException{"File not found: " + path.string()};
    }

    std::ifstream ifs(path);
    if (!ifs) {
        throw JsException{"Failed to open file: " + path.string()};
    }
    std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    auto        pathStr = path.string();
#ifdef _WIN32
    std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
#endif

    auto result = JS_Eval(context_, code.c_str(), code.size(), pathStr.c_str(), JS_EVAL_TYPE_MODULE);
    JsException::check(result); // check SyntaxError
    if (main) {
        assert(JS_VALUE_GET_TAG(result) == JS_TAG_MODULE);
        auto module = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(result));
        kUpdateModuleMainFlag(context_, module, main);
    }

    // module return rejected promise
    JSPromiseStateEnum state = JS_PromiseState(context_, result);
    if (state == JSPromiseStateEnum::JS_PROMISE_REJECTED) {
        JSValue msg = JS_PromiseResult(context_, result);
        JsException::check(msg);
        JS_FreeValue(context_, msg);
    }
    pumpJobs();

    return Value::move<Value>(result);
}

void JsEngine::loadByteCode(std::filesystem::path const& path, bool main) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw JsException{"Failed to open binary file: " + path.string()};
    }
    std::vector<uint8_t> bytecode((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    JSValue result = JS_ReadObject(context_, bytecode.data(), bytecode.size(), JS_READ_OBJ_BYTECODE);
    JsException::check(result);

    if (JS_VALUE_GET_TAG(result) == JS_TAG_MODULE) {
        if (JS_ResolveModule(context_, result) < 0) {
            JS_FreeValue(context_, result);
            JsException::check(-1, "Failed to resolve module");
        }
        auto url = path.string();
#ifdef _WIN32
        std::replace(url.begin(), url.end(), '\\', '/');
#endif
        url = std::string{FilePrefix} + url;
        assert(JS_VALUE_GET_TAG(result) == JS_TAG_MODULE);
        auto module = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(result));
        kUpdateModuleMeta(context_, module, url, main);
    }

    JSValue ret = JS_EvalFunction(context_, result);
    // module return rejected promise
    JSPromiseStateEnum state = JS_PromiseState(context_, ret);
    if (state == JSPromiseStateEnum::JS_PROMISE_REJECTED) {
        JSValue msg = JS_PromiseResult(context_, ret);
        JsException::check(msg);
        JS_FreeValue(context_, msg);
    }

    JsException::check(ret);
    JS_FreeValue(context_, ret);

    pumpJobs();
}

Object JsEngine::globalThis() const {
    auto global = JS_GetGlobalObject(context_);
    JsException::check(global);
    return Value::move<Object>(global);
}

bool JsEngine::isDestroying() const { return isDestroying_; }

void JsEngine::gc() {
    JsScope lock(this);
    if (isDestroying() || pauseGcCount_ != 0) return;
    JS_RunGC(runtime_);
}

size_t JsEngine::getMemoryUsage() {
    JsScope       lock(this);
    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(runtime_, &usage);
    return usage.memory_used_size;
}

TaskQueue* JsEngine::getTaskQueue() const { return queue_.get(); }

void JsEngine::setData(std::shared_ptr<void> data) { userData_ = std::move(data); }

Object JsEngine::registerNativeClass(ClassDefine const& def) {
    auto ctor = createJavaScriptClassOf(def);
    globalThis().set(def.name_, ctor);
    return ctor;
}
void JsEngine::registerNativeModule(ModuleDefine const& module) {
    if (nativeModules_.contains(module.name_)) {
        throw std::logic_error("ES module " + module.name_ + " already registered");
    }
    nativeModules_.emplace(module.name_, &module); // 懒加载
}
Object JsEngine::createJavaScriptClassOf(ClassDefine const& def) {
    if (nativeInstanceClasses_.contains(&def)) {
        throw std::logic_error("Native class " + def.name_ + " already registered");
    }

    bool const instance = def.hasInstanceConstructor();
    if (!instance) {
        // 非实例类，挂载为 Object 的静态属性
        auto object = Object{};
        implStaticRegister(object, def.staticDefine_);
        nativeStaticClasses_.emplace(&def, object);
        return object;
    }

    if (def.instanceDefine_.classId_ == JS_INVALID_CLASS_ID) {
        auto id = const_cast<JSClassID*>(&def.instanceDefine_.classId_);
        JS_NewClassID(runtime_, id);
    }

    JSClassDef jsDef{};
    jsDef.class_name = def.name_.c_str();
    jsDef.finalizer  = &kTemplateClassFinalizer;

    JS_NewClass(runtime_, def.instanceDefine_.classId_, &jsDef);

    auto ctor  = createConstructor(def);
    auto proto = createPrototype(def);
    implStaticRegister(ctor, def.staticDefine_);

    JS_SetConstructor(context_, Value::extract(ctor), Value::extract(proto));
    JS_SetClassProto(context_, def.instanceDefine_.classId_, JS_DupValue(context_, Value::extract(proto)));

    nativeInstanceClasses_.emplace(
        &def,
        std::pair{
            JS_DupValue(context_, Value::extract(ctor)),
            JS_DupValue(context_, Value::extract(proto)),
        }
    );

    if (def.extends_ != nullptr) {
        if (!def.extends_->hasInstanceConstructor()) {
            throw std::logic_error("Native class " + def.name_ + " extends non-instance class " + def.extends_->name_);
        }
        auto iter = nativeInstanceClasses_.find(def.extends_);
        if (iter == nativeInstanceClasses_.end()) {
            throw std::logic_error(
                def.name_ + " cannot inherit from " + def.extends_->name_
                + " because the parent class is not registered."
            );
        }
        // Child.prototype.__proto__ = Parent.prototype;
        assert(def.extends_->instanceDefine_.classId_ != JS_INVALID_CLASS_ID);
        auto base = JS_GetClassProto(context_, def.extends_->instanceDefine_.classId_);
        auto code = JS_SetPrototype(context_, Value::extract(proto), base);
        JS_FreeValue(context_, base);
        JsException::check(code);

        // Child.__proto__ = Parent;
        JS_SetPrototype(context_, Value::extract(ctor), iter->second.first);
    }

    return ctor;
}
Function JsEngine::createQuickJsCFunction(void* data1, void* data2, RawFunctionCallback cb) {
    auto newOpaque = [this](JSContext* context, void* data) -> JSValue {
        if (!data) return JS_UNDEFINED;
        auto dt = JS_NewObjectClass(context, static_cast<int>(kPointerClassId));
        JsException::check(dt);
        JS_SetOpaque(dt, data);
        return dt;
    };

    auto op1   = newOpaque(context_, data1);
    auto op2   = newOpaque(context_, data2);
    auto anyCb = newOpaque(context_, reinterpret_cast<void*>(cb));

    std::array<JSValue, 3> dataArray{op1, op2, anyCb};

    auto fn = JS_NewCFunctionData(
        context_,
        [](JSContext* ctx, JSValueConst thiz, int argc, JSValueConst* argv, int magic, JSValue* data) -> JSValue {
            auto engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));

            auto data1    = JS_GetOpaque(data[0], engine->kPointerClassId);
            auto data2    = JS_GetOpaque(data[1], engine->kPointerClassId);
            auto callback = reinterpret_cast<RawFunctionCallback>(JS_GetOpaque(data[2], engine->kPointerClassId));

            try {
                auto arguments = Arguments{engine, thiz, argc, argv};
                auto ret       = callback(arguments, data1, data2, (magic & JS_CALL_FLAG_CONSTRUCTOR) != 0);
                return JS_DupValue(ctx, Value::extract(ret));
            } catch (JsException const& e) {
                return e.rethrowToEngine();
            }
        },
        0,
        0,
        dataArray.size(),
        dataArray.data()
    );

    JS_FreeValue(context_, op1);
    JS_FreeValue(context_, op2);
    JS_FreeValue(context_, anyCb);

    JsException::check(fn);
    return Value::move<Function>(fn);
}
Object JsEngine::createConstructor(ClassDefine const& def) {
    auto ctor = createQuickJsCFunction(
        const_cast<ClassDefine*>(&def),
        nullptr,
        [](Arguments const& args, void* data1, void*, bool) -> Value {
            auto def    = static_cast<ClassDefine*>(data1);
            auto engine = args.engine();

            if (!JS_IsConstructor(engine->context_, args.thiz_)) {
                throw JsException{
                    JsException::Type::SyntaxError,
                    "Native class constructor cannot be called as a function"
                };
            }

            JSValue proto = JS_GetPropertyStr(engine->context_, args.thiz_, "prototype");
            JsException::check(proto);

            auto obj = JS_NewObjectProtoClass(engine->context_, proto, static_cast<int>(def->instanceDefine_.classId_));
            JS_FreeValue(engine->context_, proto);
            JsException::check(obj);

            void* instance        = nullptr;
            bool  constructFromJs = true;
            if (args.length_ == 1) {
                auto rawArg0 = Value::extract(args[0]);
                auto id      = JS_GetClassID(rawArg0);
                if (auto ptr = JS_GetOpaque(rawArg0, id)) {
                    assert(id != JS_INVALID_CLASS_ID);
                    assert(id == engine->kPointerClassId);

                    instance        = ptr; // construct from c++
                    constructFromJs = false;
                }
            }

            if (!instance) { // construct from js
                const_cast<Arguments&>(args).thiz_ = obj;
                instance                           = (def->instanceDefine_.constructor_)(args);
                if (!instance) {
                    throw JsException{"This native class cannot be constructed."};
                }
            }

            void* wrapped = constructFromJs ? def->wrap(instance).release() : instance;
            {
                auto typed     = static_cast<WrappedResource*>(wrapped);
                typed->define_ = def;
                typed->engine_ = engine;

                (*const_cast<bool*>(&typed->constructFromJs_)) = constructFromJs;
            }

            JS_SetOpaque(obj, wrapped);
            return Value::move<Value>(obj);
        }
    );

    auto obj = JS_DupValue(context_, Value::extract(ctor));
    JsException::check(JS_SetConstructorBit(context_, obj, true));
    return Value::move<Object>(obj);
}
Object JsEngine::createPrototype(ClassDefine const& def) {
    auto prototype = Object{};
    auto defPtr    = const_cast<ClassDefine*>(&def);
    for (auto&& method : def.instanceDefine_.methods_) {
        auto fn = createQuickJsCFunction(
            const_cast<InstanceDefine::Method*>(&method),
            defPtr,
            [](Arguments const& args, void* data1, void* data2, bool) -> Value {
                auto classID = JS_GetClassID(args.thiz_);
                assert(classID != JS_INVALID_CLASS_ID);
                auto typed = static_cast<WrappedResource*>(JS_GetOpaque(args.thiz_, classID));
                auto thiz  = (*typed)();
                if (thiz == nullptr) {
                    return Null{};
                }
                if (kInstanceCallCheckClassDefine && typed->define_ != data2 && typed->define_->extends_ != data2) {
                    throw JsException{"This object is not a valid instance of this class."};
                }
                auto def = static_cast<InstanceDefine::Method*>(data1);
                return (def->callback_)(thiz, args);
            }
        );
        prototype.set(method.name_, fn);
    }

    for (auto&& prop : def.instanceDefine_.property_) {
        Value getter;
        Value setter;

        getter = createQuickJsCFunction(
            const_cast<InstanceDefine::Property*>(&prop),
            defPtr,
            [](Arguments const& args, void* data1, void* data2, bool) -> Value {
                auto classID = JS_GetClassID(args.thiz_);
                assert(classID != JS_INVALID_CLASS_ID);
                auto typed = static_cast<WrappedResource*>(JS_GetOpaque(args.thiz_, classID));
                auto thiz  = (*typed)();
                if (thiz == nullptr) {
                    return Null{};
                }
                if (kInstanceCallCheckClassDefine && typed->define_ != data2 && typed->define_->extends_ != data2) {
                    throw JsException{"This object is not a valid instance of this class."};
                }
                auto def = static_cast<InstanceDefine::Property*>(data1);
                return (def->getter_)(thiz, args);
            }
        );

        if (prop.setter_) {
            setter = createQuickJsCFunction(
                const_cast<InstanceDefine::Property*>(&prop),
                defPtr,
                [](Arguments const& args, void* data1, void* data2, bool) -> Value {
                    auto classID = JS_GetClassID(args.thiz_);
                    assert(classID != JS_INVALID_CLASS_ID);
                    auto typed = static_cast<WrappedResource*>(JS_GetOpaque(args.thiz_, classID));
                    auto thiz  = (*typed)();
                    if (thiz == nullptr) {
                        return Null{};
                    }
                    if (kInstanceCallCheckClassDefine && typed->define_ != data2 && typed->define_->extends_ != data2) {
                        throw JsException{"This object is not a valid instance of this class."};
                    }
                    auto def = static_cast<InstanceDefine::Property*>(data1);
                    (def->setter_)(thiz, args);
                    return {}; // undefined
                }
            );
        }

        auto atom = JS_NewAtomLen(context_, prop.name_.data(), prop.name_.size());
        auto ret  = JS_DefinePropertyGetSet(
            context_,
            Value::extract(prototype),
            atom,
            JS_DupValue(context_, Value::extract(getter)),
            JS_DupValue(context_, Value::extract(setter)),
            toQuickJSFlags(PropertyAttributes::DontDelete)
        );
        JS_FreeAtom(context_, atom);
        JsException::check(ret);
    }
    return prototype;
}
void JsEngine::implStaticRegister(Object& ctor, StaticDefine const& def) {
    for (auto&& fnDef : def.functions_) {
        auto fn = createQuickJsCFunction(
            const_cast<StaticDefine::Function*>(&fnDef),
            nullptr,
            [](Arguments const& args, void* data1, void*, bool) -> Value {
                auto def = static_cast<StaticDefine::Function*>(data1);
                return (def->callback_)(args);
            }
        );
        ctor.set(fnDef.name_, fn);
    }

    for (auto&& propDef : def.property_) {
        Value getter;
        Value setter;

        getter = createQuickJsCFunction(
            const_cast<StaticDefine::Property*>(&propDef),
            nullptr,
            [](Arguments const&, void* data1, void*, bool) -> Value {
                auto def = static_cast<StaticDefine::Property*>(data1);
                return (def->getter_)();
            }
        );
        if (propDef.setter_) {
            setter = createQuickJsCFunction(
                const_cast<StaticDefine::Property*>(&propDef),
                nullptr,
                [](Arguments const& args, void* data1, void*, bool) -> Value {
                    auto def = static_cast<StaticDefine::Property*>(data1);
                    (def->setter_)(args[0]);
                    return {};
                }
            );
        }

        auto atom = JS_NewAtomLen(context_, propDef.name_.data(), propDef.name_.size());
        auto ret  = JS_DefinePropertyGetSet(
            context_,
            Value::extract(ctor),
            atom,
            JS_DupValue(context_, Value::extract(getter)),
            JS_DupValue(context_, Value::extract(setter)),
            toQuickJSFlags(PropertyAttributes::DontDelete)
        );
        JS_FreeAtom(context_, atom);
        JsException::check(ret);
    }
}


Object JsEngine::newInstance(ClassDefine const& def, std::unique_ptr<WrappedResource>&& wrappedResource) {
    auto iter = nativeInstanceClasses_.find(&def);
    if (iter == nativeInstanceClasses_.end()) {
        throw JsException{
            "The native class " + def.name_ + " is not registered, so an instance cannot be constructed."
        };
    }
    auto instance = JS_NewObjectClass(context_, static_cast<int>(kPointerClassId));
    JsException::check(instance);
    JS_SetOpaque(instance, wrappedResource.release());

    std::array<JSValue, 1> args = {instance};

    auto& ctor   = iter->second.first;
    auto  result = JS_CallConstructor(context_, ctor, args.size(), args.data());

    JS_FreeValue(context_, instance);
    JsException::check(result);
    pumpJobs();

    return Value::move<Object>(result);
}

bool JsEngine::isInstanceOf(Object const& thiz, ClassDefine const& def) const {
    auto iter = nativeInstanceClasses_.find(&def);
    if (iter != nativeInstanceClasses_.end()) {
        return thiz.instanceOf(Value::wrap<Value>(iter->second.first));
    }
    return false;
}

void* JsEngine::getNativeInstanceOf(Object const& thiz, ClassDefine const& def) const {
    if (!isInstanceOf(thiz, def)) {
        return nullptr;
    }
    auto wrapped = static_cast<WrappedResource*>(JS_GetOpaque(Value::extract(thiz), def.instanceDefine_.classId_));
    return (*wrapped)();
}


} // namespace qjspp