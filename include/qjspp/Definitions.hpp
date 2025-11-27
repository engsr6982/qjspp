#pragma once
#include "Global.hpp"
#include "Types.hpp"
#include <memory>
#include <string>
#include <utility>

#include "JsManagedResource.hpp"
#include "reflection/TypeId.hpp"

#include <stdexcept>

namespace qjspp {


struct StaticMemberDefine {
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

    explicit StaticMemberDefine(std::vector<Property> property, std::vector<Function> functions)
    : property_(std::move(property)),
      functions_(std::move(functions)) {}
};

struct InstanceMemberDefine {
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

    // script helper
    using InstanceEqualsCallback = bool (*)(void* lhs, void* rhs);
    InstanceEqualsCallback const equals_{nullptr};

    // internal use only
    JSClassID const classId_{JS_INVALID_CLASS_ID};

    explicit InstanceMemberDefine(
        InstanceConstructor    constructor,
        std::vector<Property>  property,
        std::vector<Method>    functions,
        InstanceEqualsCallback equals
    )
    : constructor_(std::move(constructor)),
      property_(std::move(property)),
      methods_(std::move(functions)),
      equals_(equals) {}
};


class ClassDefine {
public:
    std::string const          name_;
    StaticMemberDefine const   staticMemberDef_;
    InstanceMemberDefine const instanceMemberDef_;
    ClassDefine const*         base_{nullptr};
    reflection::TypeId const   typeId_;

    [[nodiscard]] inline bool hasConstructor() const { return instanceMemberDef_.constructor_ != nullptr; }

    // 由于采用 void* 提升了运行时的灵活性，但缺少了类型信息。
    // delete void* 是不安全的，因此需要此辅助回调生成合理的 finalizer。
    // 但是因为 finalizer 是和资源相关联的，故提供一个工厂方法创建托管资源并设置 getter & finalizer
    // 此回调仅在 JavaScript 使用 `new` 构造时调用，用于包装 InstanceConstructor 返回的实例 (T*)
    // 注意：当 defineClass<T> 时如果设置构造为 Disabled，那么 factory 为空
    using ManagedResourceFactory = std::unique_ptr<struct JsManagedResource> (*)(void* instance);
    ManagedResourceFactory const factory_{nullptr};

    [[nodiscard]] inline auto manage(void* instance) const {
        if (!factory_) [[unlikely]] {
            throw std::logic_error(
                "ClassDefine::manage called but factory_ is null — class is not constructible from JS"
            );
        }
        return factory_(instance);
    }

    explicit ClassDefine(
        std::string            name,
        StaticMemberDefine     staticDef,
        InstanceMemberDefine   instanceDef,
        ClassDefine const*     base,
        reflection::TypeId     typeId,
        ManagedResourceFactory factory
    )
    : name_(std::move(name)),
      staticMemberDef_(std::move(staticDef)),
      instanceMemberDef_(std::move(instanceDef)),
      base_(base),
      typeId_(std::move(typeId)),
      factory_(factory) {}
};


class EnumDefine {
public:
    struct Entry {
        std::string const name_;
        int64_t const     value_;

        explicit Entry(std::string name, int64_t value) : name_(std::move(name)), value_(value) {}
    };

    std::string const        name_;
    std::vector<Entry> const entries_;

    explicit EnumDefine(std::string name, std::vector<Entry> entries)
    : name_(std::move(name)),
      entries_(std::move(entries)) {}
};


} // namespace qjspp