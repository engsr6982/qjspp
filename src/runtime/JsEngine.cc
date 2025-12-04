#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include "qjspp/Forward.hpp"
#include "qjspp/bind/meta/ClassDefine.hpp"
#include "qjspp/bind/meta/EnumDefine.hpp"
#include "qjspp/bind/meta/ModuleDefine.hpp"
#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/runtime/TaskQueue.hpp"
#include "qjspp/runtime/detail/ModuleLoader.hpp"
#include "qjspp/types/Arguments.hpp"
#include "qjspp/types/Function.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"


namespace qjspp {

JsEngine::PauseGc::PauseGc(JsEngine* engine) : engine_(engine) { engine_->pauseGcCount_++; }
JsEngine::PauseGc::~PauseGc() { engine_->pauseGcCount_--; }

void JsEngine::kTemplateClassFinalizer(JSRuntime*, JSValue val) {
    auto classID = JS_GetClassID(val);
    assert(classID != JS_INVALID_CLASS_ID); // ID 必须有效

    auto opaque = JS_GetOpaque(val, classID);
    if (opaque) {
        auto managed_resource = static_cast<bind::JsManagedResource*>(opaque);
        assert(managed_resource->define_->instanceMemberDef_.classId_ == classID); // 校验类ID是否匹配
        auto engine = const_cast<JsEngine*>(managed_resource->engine_);

        PauseGc pauseGc(engine); // 暂停GC
        Locker  lock(engine);    // 同步线程析构
        delete managed_resource;
    }
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

#ifndef QJSPP_DONT_PATCH_CLASS_TO_STRING_TAG
    {
        Locker scope{this};
        auto   sym = eval("(Symbol.toStringTag)"); // 获取 Symbol.toStringTag
        if (!JS_IsSymbol(Value::extract(sym))) {
            throw std::logic_error("Failed to get Symbol.toStringTag");
        }
        toStringTagSymbol_ = JS_ValueToAtom(context_, Value::extract(sym));
    }
#endif

    JS_SetRuntimeOpaque(runtime_, this);
    JS_SetContextOpaque(context_, this);
    JS_SetModuleLoaderFunc(runtime_, &detail::ModuleLoader::normalize, &detail::ModuleLoader::loader, this);
}

JsEngine::~JsEngine() {
    isDestroying_ = true;
    userData_.reset();
    queue_.reset();

    JS_FreeAtom(context_, lengthAtom_);
#ifndef QJSPP_DONT_PATCH_CLASS_TO_STRING_TAG
    JS_FreeAtom(context_, toStringTagSymbol_);
#endif
    {
        Locker scope{this};
        for (auto&& [def, obj] : nativeEnums_) obj.reset();
        for (auto&& [def, obj] : nativeStaticClasses_) obj.reset();
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
                Locker     lock(engine);
                while (JS_ExecutePendingJob(engine->runtime_, &ctx) > 0) {}
                engine->pumpScheduled_ = false;
            },
            this
        );
    }
}

Value JsEngine::eval(String const& code, EvalType type) { return eval(code.value(), "<eval>", type); }
Value JsEngine::eval(String const& code, String const& source, EvalType type) {
    return eval(code.value(), source.isValid() ? source.value() : "<eval>", type);
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
        throw std::runtime_error{std::format("File not found: {}", path.string())};
    }

    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error{std::format("Failed to open file: {}", path.string())};
    }
    std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    auto url = path.is_absolute() ? path.string() : std::filesystem::absolute(path).string();
#ifdef _WIN32
    std::replace(url.begin(), url.end(), '\\', '/');
#endif

    // 1) 编译模块
    auto result =
        JS_Eval(context_, code.c_str(), code.size(), url.c_str(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    JsException::check(result); // SyntaxError

    // 2) 设置模块元数据
    assert(JS_VALUE_GET_TAG(result) == JS_TAG_MODULE);
    auto module = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(result));
    detail::ModuleLoader::setModuleMainFlag(context_, module, main);

    // 3) 执行模块
    result = JS_EvalFunction(context_, result);
    JsException::check(result); // SyntaxError

    // 4) 检查模块是否返回 rejected promise
    JSPromiseStateEnum state = JS_PromiseState(context_, result);
    if (state == JSPromiseStateEnum::JS_PROMISE_REJECTED) {
        JSValue msg = JS_PromiseResult(context_, result);
        JS_Throw(context_, msg);
        JsException::check(-1);
    }

    pumpJobs();
    return Value::move<Value>(result);
}

void JsEngine::loadByteCode(std::filesystem::path const& path, bool main) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error{std::format("Failed to open binary file: {}", path.string())};
    }
    std::vector<uint8_t> bytecode((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    // 1) 读取字节码
    JSValue result = JS_ReadObject(context_, bytecode.data(), bytecode.size(), JS_READ_OBJ_BYTECODE);
    JsException::check(result); // SyntaxError

    // 2) 设置模块元数据
    if (JS_VALUE_GET_TAG(result) == JS_TAG_MODULE) {
        if (JS_ResolveModule(context_, result) < 0) {
            JS_FreeValue(context_, result);
            JsException::check(-1, "Failed to resolve module");
        }
        auto url = path.is_absolute() ? path.string() : std::filesystem::absolute(path).string();
#ifdef _WIN32
        std::replace(url.begin(), url.end(), '\\', '/');
#endif
        url         = std::string{detail::ModuleLoader::kFilePrefix} + url;
        auto module = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(result));
        detail::ModuleLoader::setModuleMeta(context_, module, url, main);
    }

    // 3) 执行模块
    result = JS_EvalFunction(context_, result);
    JsException::check(result); // SyntaxError

    // 4) 检查模块是否返回 rejected promise
    JSPromiseStateEnum state = JS_PromiseState(context_, result);
    if (state == JSPromiseStateEnum::JS_PROMISE_REJECTED) {
        JSValue msg = JS_PromiseResult(context_, result);
        JS_Throw(context_, msg);
        JsException::check(-1);
    }

    JS_FreeValue(context_, result);
    pumpJobs();
}

Object JsEngine::globalThis() const {
    auto global = JS_GetGlobalObject(context_);
    JsException::check(global);
    return Value::move<Object>(global);
}

bool JsEngine::isDestroying() const { return isDestroying_; }

void JsEngine::gc() {
    Locker lock(this);
    if (isDestroying() || pauseGcCount_ != 0) return;
    JS_RunGC(runtime_);
}

size_t JsEngine::getMemoryUsage() {
    Locker        lock(this);
    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(runtime_, &usage);
    return usage.memory_used_size;
}

TaskQueue* JsEngine::getTaskQueue() const { return queue_.get(); }

void JsEngine::setData(std::shared_ptr<void> data) { userData_ = std::move(data); }

Object JsEngine::registerClass(bind::meta::ClassDefine const& def) {
    auto ctor = newJsClass(def);
    globalThis().set(def.name_, ctor);
    return ctor;
}
Object JsEngine::registerEnum(bind::meta::EnumDefine const& def) {
    auto obj = implRegisterEnum(def);
    globalThis().set(def.name_, obj);
    return obj;
}
void JsEngine::registerModule(bind::meta::ModuleDefine const& module) {
    if (nativeModules_.contains(module.name_)) {
        throw std::logic_error(std::format("ES module {} already registered", module.name_));
    }
    nativeModules_.emplace(module.name_, &module); // 懒加载
}
Object JsEngine::newJsClass(bind::meta::ClassDefine const& def) {
    if (nativeInstanceClasses_.contains(&def)) {
        throw std::logic_error(std::format("Native class {} already registered", def.name_));
    }

    bool const instance = def.hasConstructor();
    if (!instance) {
        // 非实例类，挂载为 Object 的静态属性
        auto object = Object::newObject();
        implStaticRegister(object, def.staticMemberDef_);
#ifndef QJSPP_DONT_PATCH_CLASS_TO_STRING_TAG
        updateToStringTag(object, def.name_);
#endif
        nativeStaticClasses_.emplace(&def, object);
        return object;
    }


    if (def.instanceMemberDef_.classId_ == JS_INVALID_CLASS_ID) {
        auto id = const_cast<JSClassID*>(&def.instanceMemberDef_.classId_);
        JS_NewClassID(runtime_, id);
    }

    JSClassDef jsDef{};
    jsDef.class_name = def.name_.c_str();
    jsDef.finalizer  = &kTemplateClassFinalizer;

    JS_NewClass(runtime_, def.instanceMemberDef_.classId_, &jsDef);

    auto ctor  = newJsConstructor(def);
    auto proto = newJsPrototype(def);
    implStaticRegister(ctor, def.staticMemberDef_);

    JS_SetConstructor(context_, Value::extract(ctor), Value::extract(proto));
    JS_SetClassProto(context_, def.instanceMemberDef_.classId_, JS_DupValue(context_, Value::extract(proto)));

#ifndef QJSPP_DONT_PATCH_CLASS_TO_STRING_TAG
    updateToStringTag(ctor, def.name_);
    updateToStringTag(proto, def.name_);
#endif

    nativeInstanceClasses_.emplace(
        &def,
        std::pair{
            JS_DupValue(context_, Value::extract(ctor)),
            JS_DupValue(context_, Value::extract(proto)),
        }
    );

    if (def.base_ != nullptr) {
        if (!def.base_->hasConstructor()) {
            throw std::logic_error(
                std::format("Native class {} extends non-instance class {}", def.name_, def.base_->name_)
            );
        }
        auto iter = nativeInstanceClasses_.find(def.base_);
        if (iter == nativeInstanceClasses_.end()) {
            throw std::logic_error(
                std::format(
                    "{} cannot inherit from {} because the parent class is not registered.",
                    def.name_,
                    def.base_->name_
                )
            );
        }
        // Child.prototype.__proto__ = Parent.prototype;
        assert(def.base_->instanceMemberDef_.classId_ != JS_INVALID_CLASS_ID);
        auto base = JS_GetClassProto(context_, def.base_->instanceMemberDef_.classId_);
        auto code = JS_SetPrototype(context_, Value::extract(proto), base);
        JS_FreeValue(context_, base);
        JsException::check(code);

        // Child.__proto__ = Parent;
        JS_SetPrototype(context_, Value::extract(ctor), iter->second.first);
    }

    return ctor;
}
Function JsEngine::newManagedRawFunction(void* data1, void* data2, RawFunctionCallback cb) const {
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
                auto ret       = callback(arguments, data1, data2);
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
Object JsEngine::newJsConstructor(bind::meta::ClassDefine const& def) const {
    auto ctor = newManagedRawFunction(
        const_cast<bind::meta::ClassDefine*>(&def),
        nullptr,
        [](Arguments const& args, void* data1, void*) -> Value {
            auto def    = static_cast<bind::meta::ClassDefine*>(data1);
            auto engine = args.engine();

            if (!JS_IsConstructor(engine->context_, args.thiz_)) [[unlikely]] {
                throw JsException{
                    JsException::Type::TypeError,
                    "Native class constructor cannot be called as a function"
                };
            }

            JSValue proto = JS_GetPropertyStr(engine->context_, args.thiz_, "prototype");
            JsException::check(proto);

            auto obj =
                JS_NewObjectProtoClass(engine->context_, proto, static_cast<int>(def->instanceMemberDef_.classId_));
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
                auto& unConst = const_cast<Arguments&>(args);
                unConst.thiz_ = obj;
                instance      = (def->instanceMemberDef_.constructor_)(args);
                if (!instance) [[unlikely]] {
                    throw JsException{JsException::Type::TypeError, "This native class cannot be constructed."};
                }
            }

            // 对于从 C++ 构造的脚本对象，instance 已是 JsManagedResource 无需进行托管
            // 对于从 脚本 构造的对象，instance 从绑定构造函数里获得，其为原始指针，需要进行托管
            // 对于禁止脚本构造的类，绑定构造函数应该返回 nullptr 并在上方步骤抛出异常拦截
            // 注意：脚本禁止构造的类，默认不会生成托管工厂方法，如果调用会抛出 logic_error
            void* managed = constructFromJs ? def->manage(instance).release() : instance;
            {
                auto typed     = static_cast<bind::JsManagedResource*>(managed);
                typed->define_ = def;
                typed->engine_ = engine;

                (*const_cast<bool*>(&typed->constructFromJs_)) = constructFromJs;
            }

            JS_SetOpaque(obj, managed);
            return Value::move<Value>(obj);
        }
    );

    auto obj = JS_DupValue(context_, Value::extract(ctor));
    JsException::check(JS_SetConstructorBit(context_, obj, true));
    return Value::move<Object>(obj);
}
bool ClassDefineCheckHelper(bind::meta::ClassDefine const* def, bind::meta::ClassDefine const* target) {
    // TODO(optimization): For each ClassDefine, maintain a cached unordered_set of all ancestor classes.
    // Then implement isFamily(target) to quickly check if a class is derived from target.
    // This avoids repeatedly walking the inheritance chain in high-frequency calls.
    while (def) {
        if (def == target) {
            return true;
        }
        def = def->base_;
    }
    return false;
}
Object JsEngine::newJsPrototype(bind::meta::ClassDefine const& def) const {
    auto prototype = Object::newObject();
    auto definePtr = const_cast<bind::meta::ClassDefine*>(&def);

#ifndef QJSPP_DONT_GENERATE_HELPER_EQLAUS_METHDO
    prototype.set(
        kInstanceClassHelperEqlaus,
        newManagedRawFunction(definePtr, nullptr, [](Arguments const& args, void* data1, void*) -> Value {
            auto const classID = JS_GetClassID(args.thiz_);
            assert(classID != JS_INVALID_CLASS_ID);

            auto managed  = static_cast<bind::JsManagedResource*>(JS_GetOpaque(args.thiz_, classID));
            auto instance = (*managed)();
            if (instance == nullptr) [[unlikely]] {
                throw JsException{JsException::Type::ReferenceError, "object is no longer available"};
            }
            if (kInstanceCallCheckClassDefine
                && !ClassDefineCheckHelper(managed->define_, static_cast<bind::meta::ClassDefine*>(data1)))
                [[unlikely]] {
                throw JsException{JsException::Type::TypeError, "This object is not a valid instance of this class."};
            }
            const_cast<Arguments&>(args).managed_ = managed; // for Arguments::getJsManagedResource

            // lhs.$equals(rhs): boolean; lhs == rhs
            if (args.length_ != 1) [[unlikely]] {
                throw JsException{JsException::Type::TypeError, "$equals() takes exactly one argument."};
            }

            auto rhs = args[0];
            if (!rhs.isObject() || !args.engine_->isInstanceOf(rhs.asObject(), *managed->define_)) {
                return Boolean{false};
            }

            auto rhsInstance = args.engine_->getNativeInstanceOf(rhs.asObject(), *managed->define_);

            bool const val = (*managed->define_->instanceMemberDef_.equals_)(instance, rhsInstance);
            return Boolean{val};
        })
    );
#endif // QJSPP_DONT_GENERATE_HELPER_EQLAUS_METHDO

    for (auto&& method : def.instanceMemberDef_.methods_) {
        auto fn = newManagedRawFunction(
            const_cast<bind::meta::InstanceMemberDefine::Method*>(&method),
            definePtr,
            [](Arguments const& args, void* data1, void* data2) -> Value {
                auto const classID = JS_GetClassID(args.thiz_);
                assert(classID != JS_INVALID_CLASS_ID);

                auto managed  = static_cast<bind::JsManagedResource*>(JS_GetOpaque(args.thiz_, classID));
                auto instance = (*managed)();
                if (instance == nullptr) [[unlikely]] {
                    throw JsException{JsException::Type::ReferenceError, "object is no longer available"};
                }
                if (kInstanceCallCheckClassDefine
                    && !ClassDefineCheckHelper(managed->define_, static_cast<bind::meta::ClassDefine*>(data2)))
                    [[unlikely]] {
                    throw JsException{
                        JsException::Type::TypeError,
                        "This object is not a valid instance of this class."
                    };
                }
                const_cast<Arguments&>(args).managed_ = managed; // for Arguments::getJsManagedResource

                auto method = static_cast<bind::meta::InstanceMemberDefine::Method*>(data1);
                return (method->callback_)(instance, args);
            }
        );
        prototype.set(method.name_, fn);
    }

    for (auto&& prop : def.instanceMemberDef_.property_) {
        Value getter;
        Value setter;

        getter = newManagedRawFunction(
            const_cast<bind::meta::InstanceMemberDefine::Property*>(&prop),
            definePtr,
            [](Arguments const& args, void* data1, void* data2) -> Value {
                auto const classID = JS_GetClassID(args.thiz_);
                assert(classID != JS_INVALID_CLASS_ID);

                auto managed  = static_cast<bind::JsManagedResource*>(JS_GetOpaque(args.thiz_, classID));
                auto instance = (*managed)();
                if (instance == nullptr) [[unlikely]] {
                    throw JsException{JsException::Type::ReferenceError, "object is no longer available"};
                }
                if (kInstanceCallCheckClassDefine
                    && !ClassDefineCheckHelper(managed->define_, static_cast<bind::meta::ClassDefine*>(data2)))
                    [[unlikely]] {
                    throw JsException{
                        JsException::Type::TypeError,
                        "This object is not a valid instance of this class."
                    };
                }
                const_cast<Arguments&>(args).managed_ = managed; // for Arguments::getJsManagedResource

                auto property = static_cast<bind::meta::InstanceMemberDefine::Property*>(data1);
                return (property->getter_)(instance, args);
            }
        );

        if (prop.setter_) {
            setter = newManagedRawFunction(
                const_cast<bind::meta::InstanceMemberDefine::Property*>(&prop),
                definePtr,
                [](Arguments const& args, void* data1, void* data2) -> Value {
                    auto const classID = JS_GetClassID(args.thiz_);
                    assert(classID != JS_INVALID_CLASS_ID);

                    auto managed  = static_cast<bind::JsManagedResource*>(JS_GetOpaque(args.thiz_, classID));
                    auto instance = (*managed)();
                    if (instance == nullptr) [[unlikely]] {
                        throw JsException{JsException::Type::ReferenceError, "object is no longer available"};
                    }
                    if (kInstanceCallCheckClassDefine
                        && !ClassDefineCheckHelper(managed->define_, static_cast<bind::meta::ClassDefine*>(data2)))
                        [[unlikely]] {
                        throw JsException{
                            JsException::Type::TypeError,
                            "This object is not a valid instance of this class."
                        };
                    }
                    const_cast<Arguments&>(args).managed_ = managed; // for Arguments::getJsManagedResource

                    auto property = static_cast<bind::meta::InstanceMemberDefine::Property*>(data1);
                    (property->setter_)(instance, args);
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
void JsEngine::implStaticRegister(Object& ctor, bind::meta::StaticMemberDefine const& def) const {
    for (auto&& fnDef : def.functions_) {
        auto fn = newManagedRawFunction(
            const_cast<bind::meta::StaticMemberDefine::Function*>(&fnDef),
            nullptr,
            [](Arguments const& args, void* data1, void*) -> Value {
                auto function = static_cast<bind::meta::StaticMemberDefine::Function*>(data1);
                return (function->callback_)(args);
            }
        );
        ctor.set(fnDef.name_, fn);
    }

    for (auto&& propDef : def.property_) {
        Value getter;
        Value setter;

        getter = newManagedRawFunction(
            const_cast<bind::meta::StaticMemberDefine::Property*>(&propDef),
            nullptr,
            [](Arguments const&, void* data1, void*) -> Value {
                auto property = static_cast<bind::meta::StaticMemberDefine::Property*>(data1);
                return (property->getter_)();
            }
        );
        if (propDef.setter_) {
            setter = newManagedRawFunction(
                const_cast<bind::meta::StaticMemberDefine::Property*>(&propDef),
                nullptr,
                [](Arguments const& args, void* data1, void*) -> Value {
                    auto property = static_cast<bind::meta::StaticMemberDefine::Property*>(data1);
                    (property->setter_)(args[0]);
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
Object JsEngine::implRegisterEnum(bind::meta::EnumDefine const& def) {
    auto obj = Object::newObject();
    obj.defineOwnProperty(
        kEnumNameHelperProperty,
        String{def.name_},
        PropertyAttributes::DontDelete | PropertyAttributes::ReadOnly
    );

    for (auto const& [name, value] : def.entries_) {
        obj.defineOwnProperty(name, Number{value}, PropertyAttributes::DontDelete | PropertyAttributes::ReadOnly);
    }

#ifndef QJSPP_DONT_PATCH_CLASS_TO_STRING_TAG
    updateToStringTag(obj, def.name_);
#endif

    nativeEnums_.emplace(&def, obj);
    return obj;
}

#ifndef QJSPP_DONT_PATCH_CLASS_TO_STRING_TAG
void JsEngine::updateToStringTag(Object& obj, std::string_view tag) const {
    JS_DefinePropertyValue(
        context_,
        Value::extract(obj),
        toStringTagSymbol_,
        JS_NewString(context_, tag.data()),
        JS_PROP_CONFIGURABLE
    );
}
#endif


Object
JsEngine::newInstance(bind::meta::ClassDefine const& def, std::unique_ptr<bind::JsManagedResource>&& managedResource) {
    auto iter = nativeInstanceClasses_.find(&def);
    if (iter == nativeInstanceClasses_.end()) {
        throw std::logic_error{
            std::format("The native class {} is not registered, so an instance cannot be constructed.", def.name_)
        };
    }
    auto instance = JS_NewObjectClass(context_, static_cast<int>(kPointerClassId));
    JsException::check(instance);
    JS_SetOpaque(instance, managedResource.release());

    std::array<JSValue, 1> args = {instance};

    auto& ctor   = iter->second.first;
    auto  result = JS_CallConstructor(context_, ctor, args.size(), args.data());

    JS_FreeValue(context_, instance);
    JsException::check(result);
    pumpJobs();

    return Value::move<Object>(result);
}

bool JsEngine::isInstanceOf(Object const& thiz, bind::meta::ClassDefine const& def) const {
    auto iter = nativeInstanceClasses_.find(&def);
    if (iter != nativeInstanceClasses_.end()) {
        return thiz.instanceOf(Value::wrap<Value>(iter->second.first));
    }
    return false;
}

void* JsEngine::getNativeInstanceOf(Object const& thiz, bind::meta::ClassDefine const& def) const {
    if (!isInstanceOf(thiz, def)) {
        return nullptr;
    }
    auto managed_resource =
        static_cast<bind::JsManagedResource*>(JS_GetOpaque(Value::extract(thiz), def.instanceMemberDef_.classId_));
    return (*managed_resource)();
}

void JsEngine::setUnhandledJsExceptionCallback(UnhandledJsExceptionCallback cb) { unhandledJsExceptionCallback_ = cb; }

void JsEngine::invokeUnhandledJsException(JsException const& exception, ExceptionDispatchOrigin origin) {
    if (unhandledJsExceptionCallback_) {
        unhandledJsExceptionCallback_(this, exception, origin);
    }
}


} // namespace qjspp