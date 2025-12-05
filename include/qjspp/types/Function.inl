#pragma once
#include "Function.hpp"
#include "Value.hpp"

namespace qjspp {

template <typename... Args>
    requires(concepts::JsValueType<std::remove_cvref_t<Args>> && ...)
Value Function::call(Value const& thiz, Args&&... args) const {
    const Value argsArr[] = {std::forward<Args>(args)...};
    return call(thiz, std::span<Value const>(argsArr));
}


} // namespace qjspp