#pragma once
#include "Function.hpp"
#include "Value.hpp"
#include "qjspp/bind/adapter/FunctionAdapter.hpp"

namespace qjspp {

template <typename... Args>
    requires(concepts::JsValueType<std::remove_cvref_t<Args>> && ...)
Value Function::call(Value const& thiz, Args&&... args) const {
    const Value argsArr[] = {std::forward<Args>(args)...};
    return call(thiz, std::span<Value const>(argsArr));
}

template <typename T>
    requires(!concepts::JsFunctionCallback<T>)
Function Function::newFunction(T&& func) {
    if constexpr (concepts::JsFunctionCallback<T>) {
        return Function{std::forward<T>(func)};
    }
    return Function{bind::adapter::bindStaticFunction<T>(std::forward<T>(func))};
}

template <typename... Overloads>
    requires(sizeof...(Overloads) > 1)
Function Function::newFunction(Overloads&&... overloads) {
    return Function{bind::adapter::bindStaticOverloadedFunction(std::forward<Overloads>(overloads)...)};
}


} // namespace qjspp