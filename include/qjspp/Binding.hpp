#pragma once
#include "qjspp/Concepts.hpp"
#include "qjspp/Global.hpp"
#include "qjspp/Types.hpp"
#include <memory>
#include <stdexcept>
#include <string>


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

} // namespace internal

struct StaticDefine {
    struct Property {
        std::string const    name_;
        GetterCallback const getter_;
        SetterCallback const setter_;

        explicit Property(std::string name, GetterCallback getter, SetterCallback setter);
    };
    struct Function {
        std::string const      name_;
        FunctionCallback const callback_;

        explicit Function(std::string name, FunctionCallback callback);
    };

    std::vector<Property> const property_;
    std::vector<Function> const functions_;

    explicit StaticDefine(std::vector<Property> property, std::vector<Function> functions);
};

struct InstanceDefine {
    struct Property {
        std::string const            name_;
        InstanceGetterCallback const getter_;
        InstanceSetterCallback const setter_;

        explicit Property(std::string name, InstanceGetterCallback getter, InstanceSetterCallback setter);
    };
    struct Method {
        std::string const            name_;
        InstanceMethodCallback const callback_;

        explicit Method(std::string name, InstanceMethodCallback callback);
    };

    InstanceConstructor const   constructor_;
    std::vector<Property> const property_;
    std::vector<Method> const   methods_;

    // internal use only
    JSClassID const classId_{JS_INVALID_CLASS_ID};

    explicit InstanceDefine(
        InstanceConstructor   constructor,
        std::vector<Property> property,
        std::vector<Method>   functions
    );
};

struct WrappedResource final {
    using ResGetter  = void* (*)(void* resource); // return instance (T* -> void*)
    using ResDeleter = void (*)(void* resource);

private:
    void*            resource_{nullptr};
    ResGetter const  getter_{nullptr};
    ResDeleter const deleter_{nullptr};

    // internal use only
    ClassDefine const* define_{nullptr};
    JsEngine const*    engine_{nullptr};
    bool const         constructFromJs_{false};

    friend class JsEngine;

public:
    inline void* operator()() const { return getter_(resource_); }

    QJSPP_DISALLOW_COPY(WrappedResource);
    explicit WrappedResource() = delete;
    explicit WrappedResource(void* resource, ResGetter getter, ResDeleter deleter);
    ~WrappedResource(); // Call the deleter and pass in the resource.

    template <typename... Args>
        requires std::constructible_from<WrappedResource, Args...>
    static inline std::unique_ptr<WrappedResource> make(Args&&... args) {
        return std::make_unique<WrappedResource>(std::forward<Args>(args)...);
    }
};

class ClassDefine {
public:
    std::string const    name_;
    StaticDefine const   staticDefine_;
    InstanceDefine const instanceDefine_;
    ClassDefine const*   extends_{nullptr};

    [[nodiscard]] bool hasInstanceConstructor() const;

    // 由于采用 void* 提升了运行时的灵活性，但缺少了类型信息。
    // delete void* 是不安全的，所以需要此辅助方法，以生成合理的 deleter。
    // 此回调仅在 JavaScript new 时调用，用于包装 InstanceConstructor 返回的实例 (T*)
    // The use of void* enhances runtime flexibility but lacks type information.
    // Deleting a void* is unsafe, so this helper method is needed to generate a reasonable deleter.
    // This callback is only invoked when using JavaScript's `new` operator, and it is used to wrap the instance (T*)
    //  returned by InstanceConstructor.
    using TypedWrappedResourceFactory = std::unique_ptr<WrappedResource> (*)(void* instance);
    TypedWrappedResourceFactory const mJsNewInstanceWrapFactory{nullptr};

    [[nodiscard]] inline auto wrap(void* instance) const { return mJsNewInstanceWrapFactory(instance); }

private:
    explicit ClassDefine(
        std::string                 name,
        StaticDefine                static_,
        InstanceDefine              instance,
        ClassDefine const*          parent,
        TypedWrappedResourceFactory factory
    );

    template <typename>
    friend struct ClassDefineBuilder;
};


template <typename Class>
struct ClassDefineBuilder {
private:
    std::string                           mClassName;
    std::vector<StaticDefine::Property>   mStaticProperty;
    std::vector<StaticDefine::Function>   mStaticFunctions;
    InstanceConstructor                   mInstanceConstructor;
    std::vector<InstanceDefine::Property> mInstanceProperty;
    std::vector<InstanceDefine::Method>   mInstanceFunctions;
    ClassDefine const*                    mExtends         = nullptr;
    bool const                            mIsInstanceClass = !std::is_void_v<Class>;

public:
    explicit ClassDefineBuilder(std::string className) : mClassName(std::move(className)) {}

    // 注册静态方法（已包装的 JsFunctionCallback） / Register static function (already wrapped)
    template <typename Fn>
        requires(IsFunctionCallback<Fn>)
    ClassDefineBuilder<Class>& function(std::string name, Fn&& fn) {
        mStaticFunctions.emplace_back(std::move(name), std::forward<Fn>(fn));
        return *this;
    }

    // 注册静态方法（自动包装） / Register static function (wrap C++ callable)
    template <typename Fn>
        requires(!IsFunctionCallback<Fn>)
    ClassDefineBuilder<Class>& function(std::string name, Fn&& fn) {
        mStaticFunctions.emplace_back(std::move(name), internal::bindStaticFunction(std::forward<Fn>(fn)));
        return *this;
    }

    // 注册重载静态方法 / Register overloaded static functions
    template <typename... Fn>
        requires(sizeof...(Fn) > 1 && (!IsFunctionCallback<Fn> && ...))
    ClassDefineBuilder<Class>& function(std::string name, Fn&&... fn) {
        mStaticFunctions.emplace_back(std::move(name), internal::bindStaticOverloadedFunction(std::forward<Fn>(fn)...));
        return *this;
    }

    // 注册静态属性（回调形式）/ Static property with raw callback
    ClassDefineBuilder<Class>& property(std::string name, GetterCallback getter, SetterCallback setter = nullptr) {
        mStaticProperty.emplace_back(std::move(name), std::move(getter), std::move(setter));
        return *this;
    }

    // 静态属性（变量指针）/ Static property from global/static variable pointer
    template <typename Ty>
    ClassDefineBuilder<Class>& property(std::string name, Ty* member) {
        auto gs = internal::bindStaticProperty<Ty>(member);
        mStaticProperty.emplace_back(std::move(name), std::move(gs.first), std::move(gs.second));
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
        if (mInstanceConstructor) throw std::logic_error("Constructor has already been registered!");
        mInstanceConstructor = internal::bindInstanceConstructor<Class, Args...>();
        return *this;
    }

    /**
     * 自定义构造逻辑。返回对象指针。
     * Register a custom constructor. Should return a pointer to the instance.
     */
    template <typename T = Class>
        requires(!std::is_void_v<T>)
    ClassDefineBuilder<Class>& customConstructor(InstanceConstructor ctor) {
        if (mInstanceConstructor) throw std::logic_error("Constructor has already been registered!");
        mInstanceConstructor = std::move(ctor);
        return *this;
    }

    /**
     * 禁用构造函数，使 JavaScript 无法构造此类。
     * Disable constructor from being called in JavaScript.
     */
    template <typename T = Class>
        requires(!std::is_void_v<T>)
    ClassDefineBuilder<Class>& disableConstructor() {
        if (mInstanceConstructor) throw std::logic_error("Constructor has already been registered!");
        mInstanceConstructor = [](Arguments const&) { return nullptr; };
        return *this;
    }

    // 注册实例方法（已包装）/ Instance method with JsInstanceMethodCallback
    template <typename Fn>
        requires(!std::is_void_v<Class> && IsInstanceMethodCallback<Fn>)
    ClassDefineBuilder<Class>& instanceMethod(std::string name, Fn&& fn) {
        mInstanceFunctions.emplace_back(std::move(name), std::forward<Fn>(fn));
        return *this;
    }

    // 实例方法（自动包装）/ Instance method with automatic binding
    template <typename Fn>
        requires(!std::is_void_v<Class> && !IsInstanceMethodCallback<Fn> && std::is_member_function_pointer_v<Fn>)
    ClassDefineBuilder<Class>& instanceMethod(std::string name, Fn&& fn) {
        mInstanceFunctions.emplace_back(std::move(name), internal::bindInstanceMethod<Class>(std::forward<Fn>(fn)));
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
        mInstanceFunctions.emplace_back(
            std::move(name),
            internal::bindInstanceOverloadedMethod<Class>(std::forward<Fn>(fn)...)
        );
        return *this;
    }

    // 实例属性（回调）/ Instance property with callbacks
    ClassDefineBuilder<Class>&
    instanceProperty(std::string name, InstanceGetterCallback getter, InstanceSetterCallback setter = nullptr) {
        static_assert(!std::is_void_v<Class>, "Only instance class can have instanceProperty");
        mInstanceProperty.emplace_back(std::move(name), std::move(getter), std::move(setter));
        return *this;
    }

    // 实例属性（成员变量）/ Instance property from T C::* member
    template <typename Member>
        requires(!std::is_void_v<Class> && std::is_member_object_pointer_v<Member>)
    ClassDefineBuilder<Class>& instanceProperty(std::string name, Member member) {
        auto gs = internal::bindInstanceProperty<Class>(std::forward<Member>(member));
        mInstanceProperty.emplace_back(std::move(name), std::move(gs.first), std::move(gs.second));
        return *this;
    }

    /**
     * 设置继承关系 / Set base class
     * @note 基类必须为一个实例类
     * @note 由于 QuickJs C API 限制，目前只能做到 ES5 继承(无法继承静态属性、方法)
     */
    ClassDefineBuilder<Class>& extends(ClassDefine const& parent) {
        static_assert(!std::is_void_v<Class>, "Only instance classes can set up inheritance.");
        mExtends = &parent;
        return *this;
    }

    ClassDefine build() {
        if (mIsInstanceClass && !mInstanceConstructor) {
            throw std::logic_error("Instance class must have a constructor!");
        }

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

        return ClassDefine{
            std::move(mClassName),
            StaticDefine{std::move(mStaticProperty), std::move(mStaticFunctions)},
            InstanceDefine{
                         std::move(mInstanceConstructor),
                         std::move(mInstanceProperty),
                         std::move(mInstanceFunctions)
            },
            mExtends,
            factory
        };
    }
};


template <typename C>
inline ClassDefineBuilder<C> defineClass(std::string className) {
    return ClassDefineBuilder<C>(std::move(className));
}


} // namespace qjspp

#include "Binding.inl"
