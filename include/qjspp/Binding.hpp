#pragma once
#include "qjspp/Concepts.hpp"
#include "qjspp/Definitions.hpp"
#include "qjspp/Types.hpp"
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
std::pair<GetterCallback, SetterCallback> bindStaticProperty(Ty* p);


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
std::pair<InstanceGetterCallback, InstanceSetterCallback> bindInstanceProperty(Ty C::* prop);


template <typename C>
InstanceDefine::InstanceEqualsCallback bindInstanceEquals();

} // namespace internal


template <typename Class>
struct ClassDefineBuilder {
private:
    std::string                           className_;
    std::vector<StaticDefine::Property>   staticProperty_;
    std::vector<StaticDefine::Function>   staticFunctions_;
    InstanceConstructor                   instanceConstructor_;
    std::vector<InstanceDefine::Property> instanceProperty_;
    std::vector<InstanceDefine::Method>   instanceFunctions_;
    ClassDefine const*                    extend_          = nullptr;
    bool const                            isInstanceClass_ = !std::is_void_v<Class>;

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
        if (instanceConstructor_) throw std::logic_error("Constructor has already been registered!");
        instanceConstructor_ = internal::bindInstanceConstructor<Class, Args...>();
        return *this;
    }

    /**
     * 自定义构造逻辑。返回对象指针。
     * Register a custom constructor. Should return a pointer to the instance.
     */
    template <typename T = Class>
        requires(!std::is_void_v<T>)
    ClassDefineBuilder<Class>& customConstructor(InstanceConstructor ctor) {
        if (instanceConstructor_) throw std::logic_error("Constructor has already been registered!");
        instanceConstructor_ = std::move(ctor);
        return *this;
    }

    /**
     * 禁用构造函数，使 JavaScript 无法构造此类。
     * Disable constructor from being called in JavaScript.
     */
    template <typename T = Class>
        requires(!std::is_void_v<T>)
    ClassDefineBuilder<Class>& disableConstructor() {
        if (instanceConstructor_) throw std::logic_error("Constructor has already been registered!");
        instanceConstructor_ = [](Arguments const&) { return nullptr; };
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
    template <typename Member>
        requires(!std::is_void_v<Class> && std::is_member_object_pointer_v<Member>)
    ClassDefineBuilder<Class>& instanceProperty(std::string name, Member member) {
        auto gs = internal::bindInstanceProperty<Class>(std::forward<Member>(member));
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
        extend_ = &parent;
        return *this;
    }

    ClassDefine build() {
        if (isInstanceClass_ && !instanceConstructor_) {
            throw std::logic_error("Instance class must have a constructor!");
        }

        // generate class wrapped resource factory
        ClassDefine::TypedWrappedResourceFactory factory = nullptr;
        if constexpr (!std::is_void_v<Class>) {
            factory = [](void* instance) -> std::unique_ptr<WrappedResource> {
                return WrappedResource::make(
                    instance,
                    [](void* res) -> void* { return res; },
                    [](void* res) -> void { delete static_cast<Class*>(res); }
                );
            };
        }

        // generate script helper function
        InstanceDefine::InstanceEqualsCallback equals = nullptr;
        if constexpr (!std::is_void_v<Class>) {
            equals = internal::bindInstanceEquals<Class>();
        }

        return ClassDefine{
            std::move(className_),
            StaticDefine{std::move(staticProperty_), std::move(staticFunctions_)},
            InstanceDefine{
                         std::move(instanceConstructor_),
                         std::move(instanceProperty_),
                         std::move(instanceFunctions_),
                         equals
            },
            extend_,
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

    EnumDefine build() { return EnumDefine{std::move(name_), std::move(entries_)}; }
};


template <typename E>
inline EnumDefineBuilder<E> defineEnum(std::string name) {
    return EnumDefineBuilder<E>(std::move(name));
}


} // namespace qjspp

#include "Binding.inl"
