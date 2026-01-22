#pragma once
#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/Function.hpp"
#include "qjspp/types/ScopedJsValue.hpp"
#include "qjspp/types/Value.hpp"

#include <array>

namespace qjspp::bind::adapter {

// Function -> std::function
template <typename R, typename... Args>
inline decltype(auto) bindScriptCallback(Value const& value) {
    if (!value.isFunction()) [[unlikely]] {
        throw JsException(JsException::Type::TypeError, "expected function");
    }
    auto engine = Locker::currentEngine();
    auto scoped = ScopedJsValue{engine, value};
    return [sc = std::move(scoped)](Args&&... args) -> R {
        auto   engine = sc.engine();
        Locker lock{engine};

        auto cb = sc.value().asFunction();

        std::array<Value, sizeof...(Args)> argv{ConvertToJs(std::forward<Args>(args))...};
        if constexpr (std::is_void_v<R>) {
            cb.call({}, argv);
        } else {
            return ConvertToCpp<R>(cb.call({}, argv));
        }
    };
}


} // namespace qjspp::bind::adapter