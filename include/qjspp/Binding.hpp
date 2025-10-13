#pragma once
#include "qjspp/Concepts.hpp"
#include "qjspp/Definitions.hpp"
#include "qjspp/JsManagedResource.hpp"
#include "qjspp/Types.hpp"
#include "qjspp/Values.hpp"
#include <stdexcept>
#include <string>
#include <type_traits>


namespace qjspp {

namespace internal {

// Forward statement

template <typename Func>
FunctionCallback bindStaticFunction(Func&& func);

template <typename... Func>
FunctionCallback bindStaticOverloadedFunction(Func&&... funcs);

template <typename Fn>
GetterCallback bindStaticGetter(Fn&& fn);

template <typename Fn>
SetterCallback bindStaticSetter(Fn&& fn);

template <typename Ty>
std::pair<GetterCallback, SetterCallback> bindStaticProperty(Ty* ptr);


template <typename C, typename... Args>
InstanceConstructor bindInstanceConstructor();

template <typename C, typename Func>
InstanceMethodCallback bindInstanceMethod(Func&& fn);

template <typename C, typename... Func>
InstanceMethodCallback bindInstanceOverloadedMethod(Func&&... funcs);

template <typename C, typename Fn>
InstanceGetterCallback bindInstanceGetter(Fn&& fn);

template <typename C, typename Fn>
InstanceSetterCallback bindInstanceSetter(Fn&& fn);

template <typename C, typename Ty>
std::pair<InstanceGetterCallback, InstanceSetterCallback> bindInstanceProperty(Ty C::* member);

template <typename C, typename Ty>
std::pair<InstanceGetterCallback, InstanceSetterCallback>
bindInstancePropertyRef(Ty C::* member, ClassDefine const* def);

template <typename C>
InstanceMemberDefine::InstanceEqualsCallback bindInstanceEquals();

} // namespace internal


template <typename Class>
struct ClassDefineBuilder {
private:
    std::string                                 className_;
    std::vector<StaticMemberDefine::Property>   staticProperty_;
    std::vector<StaticMemberDefine::Function>   staticFunctions_;
    std::vector<InstanceMemberDefine::Property> instanceProperty_;
    std::vector<InstanceMemberDefine::Method>   instanceFunctions_;
    ClassDefine const*                          base_ = nullptr;

    enum class ConstructorMode { None, Normal, Custom, Disabled };
    ConstructorMode                                              constructorMode_        = ConstructorMode::None;
    InstanceConstructor                                          userDefinedConstructor_ = nullptr;
    std::unordered_map<size_t, std::vector<InstanceConstructor>> constructors_           = {};


public:
    explicit ClassDefineBuilder(std::string className) : className_(std::move(className)) {}

    // 注册静态方法（已包装的 JsFunctionCallback） / Register static function (already wrapped)
    template <typename Fn>
        requires(IsFunctionCallback<Fn>)
    ClassDefineBuilder<Class>& function(std::string name, Fn&& fn) {
        staticFunctions_.emplace_back(std::move(name), std::forward<Fn>(fn));
        return *this;
    }

    // 注册静态方法（自动包装） / Register static function (wrap C++ callable)
    template <typename Fn>
        requires(!IsFunctionCallback<Fn>)
    ClassDefineBuilder<Class>& function(std::string name, Fn&& fn) {
        staticFunctions_.emplace_back(std::move(name), internal::bindStaticFunction(std::forward<Fn>(fn)));
        return *this;
    }

    // 注册重载静态方法 / Register overloaded static functions
    template <typename... Fn>
        requires(sizeof...(Fn) > 1 && (!IsFunctionCallback<Fn> && ...))
    ClassDefineBuilder<Class>& function(std::string name, Fn&&... fn) {
        staticFunctions_.emplace_back(std::move(name), internal::bindStaticOverloadedFunction(std::forward<Fn>(fn)...));
        return *this;
    }

    // 注册静态属性（回调形式）/ Static property with raw callback
    ClassDefineBuilder<Class>& property(std::string name, GetterCallback getter, SetterCallback setter = nullptr) {
        staticProperty_.emplace_back(std::move(name), std::move(getter), std::move(setter));
        return *this;
    }

    // 静态属性（变量指针）/ Static property from global/static variable pointer
    // 注意：绑定生成的属性默认采用值传递（即：toJs、toCpp 均拷贝实例）
    // note: the default binding of the property uses value passing (i.e. toJs, toCpp all copy instances)
    template <typename Ty>
    ClassDefineBuilder<Class>& property(std::string name, Ty* member) {
        auto gs = internal::bindStaticProperty<Ty>(member);
        staticProperty_.emplace_back(std::move(name), std::move(gs.first), std::move(gs.second));
        return *this;
    }


    /* Instance Interface */
    /**
     * 绑定默认构造函数。必须可被指定参数调用。
     * Bind a default constructor. C must be constructible with specified arguments.
     */
    template <typename... Args>
        requires(!std::is_void_v<Class>)
    ClassDefineBuilder<Class>& constructor() {
        static_assert(
            !std::is_aggregate_v<Class> && std::is_constructible_v<Class, Args...>,
            "Constructor must be callable with the specified arguments"
        );
        if (constructorMode_ == ConstructorMode::Custom)
            throw std::logic_error("Cannot mix constructor() with customConstructor()");
        if (constructorMode_ == ConstructorMode::Disabled)
            throw std::logic_error("Cannot mix constructor() with disableConstructor()");

        constexpr size_t N = sizeof...(Args);
        constructorMode_   = ConstructorMode::Normal;
        constructors_[N].emplace_back(internal::bindInstanceConstructor<Class, Args...>());
        return *this;
    }

    /**
     * 自定义构造逻辑。返回对象指针。
     * Register a custom constructor. Should return a pointer to the instance.
     */
    template <typename T = Class>
        requires(!std::is_void_v<T>)
    ClassDefineBuilder<Class>& customConstructor(InstanceConstructor ctor) {
        if (constructorMode_ == ConstructorMode::Normal)
            throw std::logic_error("Cannot mix customConstructor() with constructor()");
        if (constructorMode_ == ConstructorMode::Disabled)
            throw std::logic_error("Cannot mix customConstructor() with disableConstructor()");
        constructorMode_        = ConstructorMode::Custom;
        userDefinedConstructor_ = std::move(ctor);
        return *this;
    }

    /**
     * 禁用构造函数，使 JavaScript 无法构造此类。
     * Disable constructor from being called in JavaScript.
     */
    template <typename T = Class>
        requires(!std::is_void_v<T>)
    ClassDefineBuilder<Class>& disableConstructor() {
        if (constructorMode_ == ConstructorMode::Normal)
            throw std::logic_error("Cannot mix disableConstructor() with constructor()");
        if (constructorMode_ == ConstructorMode::Custom)
            throw std::logic_error("Cannot mix disableConstructor() with customConstructor()");
        constructorMode_        = ConstructorMode::Disabled;
        userDefinedConstructor_ = [](Arguments const&) { return nullptr; };
        return *this;
    }

    // 注册实例方法（已包装）/ Instance method with JsInstanceMethodCallback
    template <typename Fn>
        requires(!std::is_void_v<Class> && IsInstanceMethodCallback<Fn>)
    ClassDefineBuilder<Class>& instanceMethod(std::string name, Fn&& fn) {
        instanceFunctions_.emplace_back(std::move(name), std::forward<Fn>(fn));
        return *this;
    }

    // 实例方法（自动包装）/ Instance method with automatic binding
    template <typename Fn>
        requires(!std::is_void_v<Class> && !IsInstanceMethodCallback<Fn> && std::is_member_function_pointer_v<Fn>)
    ClassDefineBuilder<Class>& instanceMethod(std::string name, Fn&& fn) {
        instanceFunctions_.emplace_back(std::move(name), internal::bindInstanceMethod<Class>(std::forward<Fn>(fn)));
        return *this;
    }

    // 实例重载方法 / Overloaded instance methods
    template <typename... Fn>
        requires(
            !std::is_void_v<Class>
            && (sizeof...(Fn) > 1 && (!IsInstanceMethodCallback<Fn> && ...)
                && (std::is_member_function_pointer_v<Fn> && ...))
        )
    ClassDefineBuilder<Class>& instanceMethod(std::string name, Fn&&... fn) {
        instanceFunctions_.emplace_back(
            std::move(name),
            internal::bindInstanceOverloadedMethod<Class>(std::forward<Fn>(fn)...)
        );
        return *this;
    }

    // 实例属性（回调）/ Instance property with callbacks
    ClassDefineBuilder<Class>&
    instanceProperty(std::string name, InstanceGetterCallback getter, InstanceSetterCallback setter = nullptr) {
        static_assert(!std::is_void_v<Class>, "Only instance class can have instanceProperty");
        instanceProperty_.emplace_back(std::move(name), std::move(getter), std::move(setter));
        return *this;
    }

    // 实例属性（成员变量）/ Instance property from T C::* member
    // 注意：此回调适用于值类型成员，qjspp 会进行拷贝传递
    template <typename Member>
        requires(!std::is_void_v<Class> && std::is_member_object_pointer_v<Member>)
    ClassDefineBuilder<Class>& instanceProperty(std::string name, Member member) {
        auto gs = internal::bindInstanceProperty<Class>(std::forward<Member>(member));
        instanceProperty_.emplace_back(std::move(name), std::move(gs.first), std::move(gs.second));
        return *this;
    }

    // 实例属性（成员变量，对象引用）/ Instance property from T C::* member with reference
    template <typename Member>
        requires(!std::is_void_v<Class> && std::is_member_object_pointer_v<Member>)
    auto& instancePropertyRef(std::string name, Member member, ClassDefine const& def) {
        auto gs = internal::bindInstancePropertyRef<Class>(std::forward<Member>(member), &def);
        instanceProperty_.emplace_back(std::move(name), std::move(gs.first), std::move(gs.second));
        return *this;
    }

    /**
     * 设置继承关系 / Set base class
     * @note 基类必须为一个实例类
     * @note 由于 QuickJs C API 限制，目前只能做到 ES5 继承(无法继承静态属性、方法)
     */
    ClassDefineBuilder<Class>& extends(ClassDefine const& parent) {
        static_assert(!std::is_void_v<Class>, "Only instance classes can set up inheritance.");
        base_ = &parent;
        return *this;
    }

    [[nodiscard]] ClassDefine build() {
        bool const isInstanceClass = !std::is_void_v<Class>;

        InstanceConstructor ctor = nullptr;
        if (isInstanceClass) {
            if (constructorMode_ == ConstructorMode::Custom || constructorMode_ == ConstructorMode::Disabled) {
                if (userDefinedConstructor_ == nullptr) {
                    throw std::logic_error("No constructor provided");
                }
                ctor = std::move(userDefinedConstructor_);
            } else {
                ctor = [fn = std::move(constructors_)](Arguments const& arguments) -> void* {
                    auto argc = arguments.length();
                    auto iter = fn.find(argc);
                    if (iter == fn.end()) {
                        return nullptr;
                    }
                    for (auto const& f : iter->second) {
                        try {
                            if (void* ptr = std::invoke(f, arguments)) {
                                return ptr;
                            }
                        } catch (...) {}
                    }
                    return nullptr;
                };
            }
        }

        // generate class wrapped resource factory
        ClassDefine::ManagedResourceFactory factory = nullptr;
        if constexpr (isInstanceClass) {
            factory = [](void* instance) -> std::unique_ptr<JsManagedResource> {
                return JsManagedResource::make(
                    instance,
                    [](void* res) -> void* { return res; },
                    [](void* res) -> void { delete static_cast<Class*>(res); }
                );
            };
        }

        // generate script helper function
        InstanceMemberDefine::InstanceEqualsCallback equals = nullptr;
        if constexpr (isInstanceClass) {
            equals = internal::bindInstanceEquals<Class>();
        }

        return ClassDefine{
            std::move(className_),
            StaticMemberDefine{std::move(staticProperty_), std::move(staticFunctions_)},
            InstanceMemberDefine{std::move(ctor), std::move(instanceProperty_), std::move(instanceFunctions_), equals},
            base_,
            factory
        };
    }
};


template <typename C>
inline ClassDefineBuilder<C> defineClass(std::string className) {
    return ClassDefineBuilder<C>(std::move(className));
}


template <typename E>
struct EnumDefineBuilder {
    static_assert(std::is_enum_v<E>, "EnumDefineBuilder only accept enum type!");

    std::string                    name_;
    std::vector<EnumDefine::Entry> entries_;

    explicit EnumDefineBuilder(std::string name) : name_(std::move(name)) {}

    EnumDefineBuilder& value(std::string name, E e) {
        entries_.emplace_back(std::move(name), static_cast<int64_t>(e));
        return *this;
    }

    [[nodiscard]] EnumDefine build() { return EnumDefine{std::move(name_), std::move(entries_)}; }
};


template <typename E>
inline EnumDefineBuilder<E> defineEnum(std::string name) {
    return EnumDefineBuilder<E>(std::move(name));
}


} // namespace qjspp

#include "Binding.inl"
