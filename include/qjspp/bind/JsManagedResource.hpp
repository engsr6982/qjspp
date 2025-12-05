#pragma once
#include "qjspp/Global.hpp"

#include <concepts>
#include <memory>

namespace qjspp {

class JsEngine;
namespace detail {
struct BindRegistry;
}

namespace bind {
namespace meta {
class ClassDefine;
}

struct JsManagedResource final {
    using Accessor  = void* (*)(void* resource); // return instance (T* -> void*)
    using Finalizer = void (*)(void* resource);

private:
    void*           resource_{nullptr};
    Accessor const  accessor_{nullptr};
    Finalizer const finalizer_{nullptr};

    // internal use only
    meta::ClassDefine const* define_{nullptr};
    JsEngine const*          engine_{nullptr};
    bool const               constructFromJs_{false};

    friend detail::BindRegistry;

public:
    [[nodiscard]] inline void* get() const { return resource_ ? accessor_(resource_) : nullptr; }
    [[nodiscard]] inline void* operator()() const { return get(); }

    inline void finalize() {
        if (finalizer_ != nullptr && resource_ != nullptr) {
            finalizer_(resource_);
            resource_ = nullptr;
        }
    }

    QJSPP_DISABLE_COPY(JsManagedResource);
    explicit JsManagedResource() = delete;
    explicit JsManagedResource(void* resource, Accessor const accessor, Finalizer const finalizer)
    : resource_(resource),
      accessor_(accessor),
      finalizer_(finalizer) {}

    ~JsManagedResource() { finalize(); }

    template <typename... Args>
        requires std::constructible_from<JsManagedResource, Args...>
    static inline std::unique_ptr<JsManagedResource> make(Args&&... args) {
        return std::make_unique<JsManagedResource>(std::forward<Args>(args)...);
    }
};

} // namespace bind

} // namespace qjspp
