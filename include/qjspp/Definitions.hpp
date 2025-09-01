#pragma once
#include "Global.hpp"
#include "Types.hpp"
#include <memory>
#include <string>


namespace qjspp {


struct StaticDefine {
    struct Property {
        std::string const    name_;
        GetterCallback const getter_;
        SetterCallback const setter_;

        explicit Property(std::string name, GetterCallback getter, SetterCallback setter)
        : name_(std::move(name)),
          getter_(std::move(getter)),
          setter_(std::move(setter)) {}
    };
    struct Function {
        std::string const      name_;
        FunctionCallback const callback_;

        explicit Function(std::string name, FunctionCallback callback)
        : name_(std::move(name)),
          callback_(std::move(callback)) {}
    };

    std::vector<Property> const property_;
    std::vector<Function> const functions_;

    explicit StaticDefine(std::vector<Property> property, std::vector<Function> functions)
    : property_(std::move(property)),
      functions_(std::move(functions)) {}
};

struct InstanceDefine {
    struct Property {
        std::string const            name_;
        InstanceGetterCallback const getter_;
        InstanceSetterCallback const setter_;

        explicit Property(std::string name, InstanceGetterCallback getter, InstanceSetterCallback setter)
        : name_(std::move(name)),
          getter_(std::move(getter)),
          setter_(std::move(setter)) {}
    };
    struct Method {
        std::string const            name_;
        InstanceMethodCallback const callback_;

        explicit Method(std::string name, InstanceMethodCallback callback)
        : name_(std::move(name)),
          callback_(std::move(callback)) {}
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
    )
    : constructor_(std::move(constructor)),
      property_(std::move(property)),
      methods_(std::move(functions)) {}
};


class ClassDefine {
public:
    std::string const    name_;
    StaticDefine const   staticDefine_;
    InstanceDefine const instanceDefine_;
    ClassDefine const*   extends_{nullptr};

    [[nodiscard]] inline bool hasInstanceConstructor() const { return instanceDefine_.constructor_ != nullptr; }

    // 由于采用 void* 提升了运行时的灵活性，但缺少了类型信息。
    // delete void* 是不安全的，所以需要此辅助方法，以生成合理的 deleter。
    // 此回调仅在 JavaScript new 时调用，用于包装 InstanceConstructor 返回的实例 (T*)
    // The use of void* enhances runtime flexibility but lacks type information.
    // Deleting a void* is unsafe, so this helper method is needed to generate a reasonable deleter.
    // This callback is only invoked when using JavaScript's `new` operator, and it is used to wrap the instance (T*)
    //  returned by InstanceConstructor.
    using TypedWrappedResourceFactory = std::unique_ptr<struct WrappedResource> (*)(void* instance);
    TypedWrappedResourceFactory const mJsNewInstanceWrapFactory{nullptr};

    [[nodiscard]] inline auto wrap(void* instance) const { return mJsNewInstanceWrapFactory(instance); }

private:
    explicit ClassDefine(
        std::string                 name,
        StaticDefine                static_,
        InstanceDefine              instance,
        ClassDefine const*          parent,
        TypedWrappedResourceFactory factory
    )
    : name_(std::move(name)),
      staticDefine_(std::move(static_)),
      instanceDefine_(std::move(instance)),
      extends_(parent),
      mJsNewInstanceWrapFactory(factory) {}

    template <typename>
    friend struct ClassDefineBuilder;
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

    explicit WrappedResource(void* resource, ResGetter getter, ResDeleter deleter)
    : resource_(resource),
      getter_(getter),
      deleter_(deleter) {}

    ~WrappedResource() {
        if (deleter_ != nullptr) {
            deleter_(resource_);
        }
    }

    template <typename... Args>
        requires std::constructible_from<WrappedResource, Args...>
    static inline std::unique_ptr<WrappedResource> make(Args&&... args) {
        return std::make_unique<WrappedResource>(std::forward<Args>(args)...);
    }
};


} // namespace qjspp