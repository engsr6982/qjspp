#include "qjspp/JsEngine.hpp"
#include "qjspp/Binding.hpp"
#include "qjspp/ESModule.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/JsScope.hpp"
#include "qjspp/TaskQueue.hpp"
#include "qjspp/Types.hpp"
#include "qjspp/Values.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>


namespace qjspp {

PauseGc::PauseGc(JsEngine* engine) : engine_(engine) { engine_->pauseGcCount_++; }
PauseGc::~PauseGc() { engine_->pauseGcCount_--; }


JSClassID             JsEngine::kPointerClassId      = 0;
JSClassID             JsEngine::kInstanceClassId     = 0;
JSClassID             JsEngine::kFunctionDataClassId = 0;
static std::once_flag kGlobalQjsClass;


JsEngine::JsEngine() : runtime_(JS_NewRuntime()), queue_(std::make_unique<TaskQueue>()) {
    if (runtime_) {
        context_ = JS_NewContext(runtime_);
    }

    if (!runtime_ || !context_) {
        throw std::logic_error("Failed to create JS runtime or context");
    }

#ifdef QJSPP_DEBUG
    JS_SetDumpFlags(
        runtime_,
        JS_DUMP_LEAKS            // 检测内存泄露
            | JS_DUMP_ATOM_LEAKS // 检测Atom泄露
    );
#endif

    std::call_once(kGlobalQjsClass, [this]() {
        JS_NewClassID(runtime_, &kPointerClassId);
        JS_NewClassID(runtime_, &kInstanceClassId);
        JS_NewClassID(runtime_, &kFunctionDataClassId);
    });

    JSClassDef pointer{};
    pointer.class_name = "RawPointer";
    JS_NewClass(runtime_, kPointerClassId, &pointer);

    JSClassDef function{};
    function.class_name = "RawFunction";
    function.finalizer  = [](JSRuntime*, JSValue val) {
        auto ptr = JS_GetOpaque(val, kFunctionDataClassId);
        if (ptr) {
            delete static_cast<FunctionCallback*>(ptr);
        }
    };
    JS_NewClass(runtime_, kFunctionDataClassId, &function);

    JSClassDef instance{};
    instance.class_name = "NativeInstance";
    instance.finalizer  = [](JSRuntime*, JSValue val) {
        auto ptr = JS_GetOpaque(val, kInstanceClassId);
        if (ptr) {
            auto    wrapped = static_cast<WrappedResource*>(ptr);
            PauseGc pauseGc(const_cast<JsEngine*>(wrapped->engine_));
            wrapped->deleter_(wrapped->resource_);
            delete wrapped;
        }
    };
    JS_NewClass(runtime_, kInstanceClassId, &instance);

    lengthAtom_ = JS_NewAtom(context_, "length");

    JS_SetContextOpaque(context_, this);
    JS_SetModuleLoaderFunc(runtime_, NULL, js_module_loader, NULL);
}

JsEngine::~JsEngine() {
    isDestroying_ = true;
    userData_.reset();
    queue_.reset();

    JS_FreeAtom(context_, lengthAtom_);

    for (auto&& [def, data] : nativeClassData_) {
        JS_FreeValue(context_, data.first);
        JS_FreeValue(context_, data.second);
    }

    JS_RunGC(runtime_);
    JS_FreeContext(context_);
    JS_FreeRuntime(runtime_);
}

::JSRuntime* JsEngine::runtime() const { return runtime_; }

::JSContext* JsEngine::context() const { return context_; }

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

Value JsEngine::eval(String const& code) { return eval(code.value()); }
Value JsEngine::eval(String const& code, String const& filename) { return eval(code.value(), filename.value()); }
Value JsEngine::eval(std::string const& code, std::string const& filename) {
    auto result = JS_Eval(context_, code.c_str(), code.size(), filename.c_str(), JS_EVAL_TYPE_GLOBAL);
    JsException::check(result);
    pumpJobs();
    return Value::move<Value>(result);
}

Value JsEngine::loadScript(std::filesystem::path const& path) {
    if (!std::filesystem::exists(path)) {
        throw JsException{"File not found: " + path.string()};
    }

    std::ifstream ifs(path);
    if (!ifs) {
        throw JsException{"Failed to open file: " + path.string()};
    }
    std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    auto        file = path.filename().string();
    std::replace(file.begin(), file.end(), '\\', '/'); // replace \\ to /

    auto result = JS_Eval(context_, code.c_str(), code.size(), file.c_str(), JS_EVAL_TYPE_MODULE);
    JsException::check(result); // check SyntaxError

    // module return rejected promise
    JSPromiseStateEnum state = JS_PromiseState(context_, result);
    if (state == JSPromiseStateEnum::JS_PROMISE_REJECTED) {
        JSValue msg = JS_PromiseResult(context_, result);
        JS_Throw(context_, msg);
        JsException::check(-1);
    }
    pumpJobs();

    return Value::move<Value>(result);
}

void JsEngine::loadByteCode(std::filesystem::path const& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw JsException{"Failed to open binary file: " + path.string()};
    }
    std::vector<uint8_t> bytecode((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    js_std_eval_binary(context_, bytecode.data(), bytecode.size(), JS_EVAL_TYPE_MODULE);

    JsException::check(context_); // 检查是否有挂起的异常

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

void JsEngine::registerNativeClass(ClassDefine const& def) {
    auto ctor = createJavaScriptClassOf(def);
    globalThis().set(def.name_, ctor);
}
void JsEngine::registerNativeESModule(ESModuleDefine const& module) {
    for (auto& c : module.class_) {
        if (nativeClassData_.contains(c)) {
            continue; // 因为 NativeClass 是全局唯一的，如果已经注册了，则跳过
        }
        registerNativeClass(*c);
    }
    auto mdef = module.init(this); // 初始化模块
    nativeESModules_.emplace(mdef, &module);
}
Object JsEngine::createJavaScriptClassOf(ClassDefine const& def) {
    if (nativeClassData_.contains(&def)) {
        throw std::logic_error("Native class " + def.name_ + " already registered");
    }

    bool const instance = def.hasInstanceConstructor();

    auto ctor = instance ? createConstuctor(def) : Object{};
    implStaticRegister(ctor, def.staticDefine_);

    if (instance) {
        auto prototype = createPrototype(def);
        nativeClassData_.emplace(
            &def,
            std::make_pair<JSValue, JSValue>(
                JS_DupValue(context_, Value::extract(ctor)),
                JS_DupValue(context_, Value::extract(prototype))
            )
        );
        ctor.set("prototype", prototype);

        if (def.extends_ != nullptr) {
            if (!def.extends_->hasInstanceConstructor()) {
                throw std::logic_error(
                    "Native class " + def.name_ + " extends non-instance class " + def.extends_->name_
                );
            }
            auto iter = nativeClassData_.find(def.extends_);
            if (iter == nativeClassData_.end()) {
                throw std::logic_error(
                    def.name_ + " cannot inherit from " + def.extends_->name_
                    + " because the parent class is not registered."
                );
            }
            // TODO: impl Inherit
        }
    }
    return ctor;
}
Function JsEngine::createQuickJsCFunction(void* data1, void* data2, RawFunctionCallback cb) {
    auto newOpaque = [](JSContext* context, void* data) -> JSValue {
        if (!data) return JS_UNDEFINED;
        auto dt = JS_NewObjectClass(context, static_cast<int>(kPointerClassId));
        JsException::check(dt);
        JS_SetOpaque(dt, data);
        return dt;
    };

    auto op1    = newOpaque(context_, data1);
    auto op2    = newOpaque(context_, data2);
    auto engine = newOpaque(context_, this);
    auto anyCb  = newOpaque(context_, reinterpret_cast<void*>(cb));

    std::array<JSValue, 4> dataArray{op1, op2, engine, anyCb};

    auto fn = JS_NewCFunctionData(
        context_,
        [](JSContext* ctx, JSValueConst thiz, int argc, JSValueConst* argv, int magic, JSValue* data) -> JSValue {
            auto data1  = JS_GetOpaque(data[0], kPointerClassId);
            auto data2  = JS_GetOpaque(data[1], kPointerClassId);
            auto engine = static_cast<JsEngine*>(JS_GetOpaque(data[2], kPointerClassId));
            auto cb     = reinterpret_cast<RawFunctionCallback>(JS_GetOpaque(data[3], kPointerClassId));

            try {
                auto arguments = Arguments{engine, thiz, argc, argv};
                auto ret       = cb(arguments, data1, data2, (magic & JS_CALL_FLAG_CONSTRUCTOR) != 0);
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
    JS_FreeValue(context_, engine);
    JS_FreeValue(context_, anyCb);

    JsException::check(fn);
    return Value::move<Function>(fn);
}
Object JsEngine::createConstuctor(ClassDefine const& def) {
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

            auto const& data = engine->nativeClassData_.at(def);

            auto obj = JS_NewObjectClass(engine->context_, static_cast<int>(kInstanceClassId));
            JsException::check(JS_SetPrototype(engine->context_, obj, data.second));

            void* instance        = nullptr;
            bool  constructFromJs = true;
            if (args.length_ == 1) {
                if (auto ptr = JS_GetOpaque(Value::extract(args[0]), kPointerClassId)) {
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
                auto typed = static_cast<WrappedResource*>(JS_GetOpaque(args.thiz_, kInstanceClassId));
                auto thiz  = (*typed)();
                if (thiz == nullptr || typed->define_ != data2) {
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
                auto typed = static_cast<WrappedResource*>(JS_GetOpaque(args.thiz_, kInstanceClassId));
                auto thiz  = (*typed)();
                if (thiz == nullptr || typed->define_ != data2) {
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
                    auto typed = static_cast<WrappedResource*>(JS_GetOpaque(args.thiz_, kInstanceClassId));
                    auto thiz  = (*typed)();
                    if (thiz == nullptr || typed->define_ != data2) {
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
    auto iter = nativeClassData_.find(&def);
    if (iter == nativeClassData_.end()) {
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
    JsException::check(result);
    pumpJobs();

    return Value::move<Object>(result);
}

bool JsEngine::isInstanceOf(Object const& thiz, ClassDefine const& def) const {
    auto iter = nativeClassData_.find(&def);
    if (iter != nativeClassData_.end()) {
        return thiz.instanceOf(Value::wrap<Value>(iter->second.first));
    }
    return false;
}

void* JsEngine::getNativeInstanceOf(Object const& thiz, ClassDefine const& def) const {
    if (!isInstanceOf(thiz, def)) {
        return nullptr;
    }
    auto wrapped = static_cast<WrappedResource*>(JS_GetOpaque(Value::extract(thiz), kInstanceClassId));
    return (*wrapped)();
}


} // namespace qjspp