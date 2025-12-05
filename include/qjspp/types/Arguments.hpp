#pragma once
#include "qjspp/Forward.hpp"
#include "qjspp/Global.hpp"

namespace qjspp {

class JsEngine;
namespace bind {
struct JsManagedResource;
}
namespace detail {
struct FunctionFactory;
struct BindRegistry;
} // namespace detail


class Arguments final {
    JsEngine*                engine_;
    JSValueConst             thiz_;
    int                      length_;
    JSValueConst*            args_;
    bind::JsManagedResource* managed_{nullptr};

    friend detail::FunctionFactory;
    friend detail::BindRegistry;
    friend class Function;

    explicit Arguments(JsEngine* engine, JSValueConst thiz, int length, JSValueConst* args);

public:
    QJSPP_DISABLE_COPY_MOVE(Arguments);

    [[nodiscard]] JsEngine* engine() const;

    [[nodiscard]] bool hasThiz() const;

    [[nodiscard]] Object thiz() const; // this

    [[nodiscard]] size_t length() const;

    // Obtain internal resource management wrapper.
    // This function is only used internally.
    // note: This resource is only valid during instance class calls (method, property).
    [[nodiscard]] bool                     hasJsManagedResource() const;
    [[nodiscard]] bind::JsManagedResource* getJsManagedResource() const;

    Value operator[](size_t index) const;
};

} // namespace qjspp