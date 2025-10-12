#pragma once
#include "qjspp/Binding.hpp"
#include "qjspp/Concepts.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/Values.hpp"
#include <type_traits>


namespace qjspp {


template <IsWrappedType T>
T Value::as() const {
    if constexpr (std::is_same_v<T, Value>) {
        return asValue();
    } else if constexpr (std::is_same_v<T, Undefined>) {
        return asUndefined();
    } else if constexpr (std::is_same_v<T, Null>) {
        return asNull();
    } else if constexpr (std::is_same_v<T, Boolean>) {
        return asBoolean();
    } else if constexpr (std::is_same_v<T, Number>) {
        return asNumber();
    } else if constexpr (std::is_same_v<T, BigInt>) {
        return asBigInt();
    } else if constexpr (std::is_same_v<T, String>) {
        return asString();
    } else if constexpr (std::is_same_v<T, Object>) {
        return asObject();
    } else if constexpr (std::is_same_v<T, Array>) {
        return asArray();
    } else if constexpr (std::is_same_v<T, Function>) {
        return asFunction();
    }
    throw JsException("Unable to convert Value to T, forgot to add if branch?");
}


} // namespace qjspp