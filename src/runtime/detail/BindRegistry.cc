#include "qjspp/runtime/detail/BindRegistry.hpp"
#include "qjspp/bind/meta/ModuleDefine.hpp"
#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/detail/FunctionFactory.hpp"
#include "qjspp/types/Arguments.hpp"
#include "qjspp/types/Function.hpp"
#include "qjspp/types/Value.hpp"


#include <algorithm>
#include <cassert>

namespace qjspp::detail {

BindRegistry::BindRegistry(JsEngine& engine) : engine_(engine) {}
BindRegistry::~BindRegistry() {
    Locker scope{engine_};
    enums_.clear();
    staticClasses_.clear();
    moduleExports_.clear();
    for (auto&& [def, data] : instanceClasses_) {
        JS_FreeValue(engine_.context_, data.first);
        JS_FreeValue(engine_.context_, data.second);
    }
}

bool BindRegistry::tryRegister(bind::meta::EnumDefine const& enumDef) {
    if (enums_.contains(&enumDef)) {
        return false;
    }
    auto obj = _buildEnum(enumDef);
    engine_.globalThis().set(enumDef.name_, obj);
    enums_.emplace(&enumDef, std::move(obj));
    return true;
}

bool BindRegistry::tryRegister(bind::meta::ClassDefine const& classDef) {
    if (instanceClasses_.contains(&classDef)) {
        return false;
    }
    auto v = _registerClass(classDef);
    engine_.globalThis().set(classDef.name_, v);
    return true;
}

bool BindRegistry::tryRegister(bind::meta::ModuleDefine const& moduleDef) {
    if (lazyModules_.contains(moduleDef.name_)) {
        return false;
    }
    lazyModules_.emplace(moduleDef.name_, &moduleDef);
    return true;
}


Object BindRegistry::_buildEnum(bind::meta::EnumDefine const& enumDef) const {
    auto obj = Object::newObject();
    obj.defineOwnProperty(
        "$name",
        String{enumDef.name_},
        PropertyAttributes::DontDelete | PropertyAttributes::ReadOnly
    );
    for (auto const& [name, value] : enumDef.entries_) {
        obj.defineOwnProperty(name, Number{value}, PropertyAttributes::DontDelete | PropertyAttributes::ReadOnly);
    }
    engine_.setObjectToStringTag(obj, enumDef.name_);
    return obj;
}

Value BindRegistry::_registerClass(bind::meta::ClassDefine const& def) {
    bool const isInstance = def.hasConstructor();
    if (!isInstance) {
        auto object = Object::newObject();
        _buildClassStatic(def.staticMemberDef_, object);
        engine_.setObjectToStringTag(object, def.name_);
        staticClasses_.emplace(&def, object);
        return object;
    }

    // Instance class
    if (def.instanceMemberDef_.classId_ == JS_INVALID_CLASS_ID) {
        auto id = const_cast<JSClassID*>(&def.instanceMemberDef_.classId_);
        JS_NewClassID(engine_.runtime_, id);
    }

    JSClassDef jsDef{};
    jsDef.class_name = def.name_.c_str();
    jsDef.finalizer  = &kInstanceClassFinalizer;

    JS_NewClass(engine_.runtime_, def.instanceMemberDef_.classId_, &jsDef);

    auto ctor  = _buildClassConstructor(def);
    auto proto = _buildClassPrototype(def);

    engine_.setObjectToStringTag(proto, def.name_);
    {
        auto asObject = Value::wrap<Object>(Value::extract(ctor));
        engine_.setObjectToStringTag(asObject, def.name_);
        _buildClassStatic(def.staticMemberDef_, asObject);
    }

    JS_SetConstructor(engine_.context_, Value::extract(ctor), Value::extract(proto));
    JS_SetClassProto(
        engine_.context_,
        def.instanceMemberDef_.classId_,
        JS_DupValue(engine_.context_, Value::extract(proto))
    );

    if (def.base_ != nullptr) {
        if (!def.base_->hasConstructor()) {
            throw std::logic_error(
                std::format("Native class {} extends non-instance class {}", def.name_, def.base_->name_)
            );
        }
        auto iter = instanceClasses_.find(def.base_);
        if (iter == instanceClasses_.end()) {
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
        auto base = JS_GetClassProto(engine_.context_, def.base_->instanceMemberDef_.classId_);
        auto code = JS_SetPrototype(engine_.context_, Value::extract(proto), base);
        JS_FreeValue(engine_.context_, base);
        JsException::check(code);

        // Child.__proto__ = Parent;
        JS_SetPrototype(engine_.context_, Value::extract(ctor), iter->second.first);
    }

    instanceClasses_.emplace(
        &def,
        std::pair{
            JS_DupValue(engine_.context_, Value::extract(ctor)),
            JS_DupValue(engine_.context_, Value::extract(proto)),
        }
    );
    return ctor;
}

Function BindRegistry::_buildClassConstructor(bind::meta::ClassDefine const& def) const {
    auto ctor = FunctionFactory::create(
        engine_,
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

    auto obj = JS_DupValue(engine_.context_, Value::extract(ctor));
    JsException::check(JS_SetConstructorBit(engine_.context_, obj, true));
    return Value::move<Function>(obj);
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

#ifndef QJSPP_SKIP_INSTANCE_CALL_CHECK_CLASS_DEFINE
constexpr bool kInstanceCallCheckClassDefine = true;
#else
constexpr bool kInstanceCallCheckClassDefine = false; // 跳过实例调用时检查类定义
#endif

Object BindRegistry::_buildClassPrototype(bind::meta::ClassDefine const& def) const {
    auto prototype = Object::newObject();
    auto definePtr = const_cast<bind::meta::ClassDefine*>(&def);

#ifndef QJSPP_DONT_GENERATE_HELPER_EQLAUS_METHDO
    prototype.set(
        "$equals",
        FunctionFactory::create(engine_, definePtr, nullptr, [](Arguments const& args, void* data1, void*) -> Value {
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
        auto fn = FunctionFactory::create(
            engine_,
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

        getter = FunctionFactory::create(
            engine_,
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
            setter = FunctionFactory::create(
                engine_,
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

        auto atom = JS_NewAtomLen(engine_.context_, prop.name_.data(), prop.name_.size());
        auto ret  = JS_DefinePropertyGetSet(
            engine_.context_,
            Value::extract(prototype),
            atom,
            JS_DupValue(engine_.context_, Value::extract(getter)),
            JS_DupValue(engine_.context_, Value::extract(setter)),
            toQuickJSFlags(PropertyAttributes::DontDelete)
        );
        JS_FreeAtom(engine_.context_, atom);
        JsException::check(ret);
    }
    return prototype;
}

void BindRegistry::_buildClassStatic(bind::meta::StaticMemberDefine const& def, Object& ctor) const {
    for (auto&& fnDef : def.functions_) {
        auto fn = FunctionFactory::create(
            engine_,
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

        getter = FunctionFactory::create(
            engine_,
            const_cast<bind::meta::StaticMemberDefine::Property*>(&propDef),
            nullptr,
            [](Arguments const&, void* data1, void*) -> Value {
                auto property = static_cast<bind::meta::StaticMemberDefine::Property*>(data1);
                return (property->getter_)();
            }
        );
        if (propDef.setter_) {
            setter = FunctionFactory::create(
                engine_,
                const_cast<bind::meta::StaticMemberDefine::Property*>(&propDef),
                nullptr,
                [](Arguments const& args, void* data1, void*) -> Value {
                    auto property = static_cast<bind::meta::StaticMemberDefine::Property*>(data1);
                    (property->setter_)(args[0]);
                    return {};
                }
            );
        }

        auto atom = JS_NewAtomLen(engine_.context_, propDef.name_.data(), propDef.name_.size());
        auto ret  = JS_DefinePropertyGetSet(
            engine_.context_,
            Value::extract(ctor),
            atom,
            JS_DupValue(engine_.context_, Value::extract(getter)),
            JS_DupValue(engine_.context_, Value::extract(setter)),
            toQuickJSFlags(PropertyAttributes::DontDelete)
        );
        JS_FreeAtom(engine_.context_, atom);
        JsException::check(ret);
    }
}

void BindRegistry::_buildModuleExports(bind::meta::ModuleDefine const& def, JSModuleDef* m) {
    auto cache = ModuleExportCache{};
    cache.constants_.reserve(def.variables_.size());
    for (auto& var : def.variables_) {
        auto value = var.getter_();
        cache.constants_.emplace(&var, std::move(value));
    }

    cache.functions_.rehash(def.functions_.size());
    for (auto& fn : def.functions_) {
        auto fnVal = FunctionFactory::create(
            engine_,
            const_cast<bind::meta::ModuleDefine::FunctionExport*>(&fn),
            nullptr,
            [](Arguments const& args, void* data1, void*) -> Value {
                auto function = static_cast<bind::meta::ModuleDefine::FunctionExport*>(data1);
                return (function->callback_)(args);
            }
        );
        cache.functions_.emplace(&fn, std::move(fnVal));
    }

    moduleExports_.emplace(m, std::move(cache));
}

void BindRegistry::kInstanceClassFinalizer(JSRuntime*, JSValue val) {
    auto classID = JS_GetClassID(val);
    assert(classID != JS_INVALID_CLASS_ID); // ID 必须有效

    auto opaque = JS_GetOpaque(val, classID);
    if (opaque) {
        auto managed = static_cast<bind::JsManagedResource*>(opaque);
        assert(managed->define_->instanceMemberDef_.classId_ == classID); // 校验类ID是否匹配
        auto engine = const_cast<JsEngine*>(managed->engine_);

        JsEngine::PauseGc pauseGc(engine); // 暂停GC
        Locker            lock(engine);    // 同步线程析构
        delete managed;
    }
}

} // namespace qjspp::detail