#pragma once
#include "qjspp/Concepts.hpp"
#include "qjspp/Values.hpp"
#include <string>
#include <type_traits>


namespace qjspp {


template <typename T>
struct TypeConverter {
    static_assert(sizeof(T) == 0, "Cannot convert Js to this type T; no TypeConverter defined.");
};


template <>
struct TypeConverter<bool> {
    static Boolean toJs(bool value) { return Boolean{value}; }

    static bool toCpp(Value const& value) { return value.asBoolean().value(); }
};


// int/uint/float/double → JsNumber
template <typename T>
    requires(NumberLike_v<T> || IsInt64OrUint64_v<T>)
struct TypeConverter<T> {
    static Number toJs(T value) { return Number(value); }

    static T toCpp(Value const& value) { return static_cast<T>(value.asNumber().getDouble()); }
};


template <typename T>
    requires(StringLike_v<T> && std::constructible_from<String, T>)
struct TypeConverter<T> {
    static String toJs(T const& value) { return String{value}; }

    static std::string toCpp(Value const& value) { return value.asString().value(); } // always UTF-8
};


// enum -> JsNumber (enum value)
template <typename T>
    requires std::is_enum_v<T>
struct TypeConverter<T> {
    static Number toJs(T value) { return Number(static_cast<int>(value)); }

    static T toCpp(Value const& value) { return static_cast<T>(value.asNumber().getInt32()); }
};


// internal type
template <typename T>
    requires IsWrappedType<T>
struct TypeConverter<T> {
    static Value toJs(T const& value) { return value.asValue(); }
    static T     toCpp(Value const& value) { return value.as<T>(); }
};


namespace internal {

template <typename T>
using TypedConverter = TypeConverter<typename std::decay<T>::type>;
// using TypedConverter = TypeConverter<std::remove_cvref_t<T>>;
// using TypedConverter = TypeConverter<T>;


template <typename T, typename = void>
struct IsTypeConverterAvailable : std::false_type {};

template <typename T>
struct IsTypeConverterAvailable<
    T,
    std::void_t<
        decltype(TypedConverter<T>::toJs(std::declval<T>())),
        decltype(TypedConverter<T>::toCpp(std::declval<Value>()))>> : std::true_type {};

template <typename T>
constexpr bool IsTypeConverterAvailable_v = IsTypeConverterAvailable<T>::value;

} // namespace internal


template <typename T>
[[nodiscard]] inline Value ConvertToJs(T&& value) {
    static_assert(
        internal::IsTypeConverterAvailable_v<T>,
        "Cannot convert T to Js; there is no available TypeConverter."
    );
    return internal::TypedConverter<T>::toJs(std::forward<T>(value)).asValue();
}

template <typename T>
[[nodiscard]] inline decltype(auto) ConvertToCpp(Value const& value) {
    static_assert(
        internal::IsTypeConverterAvailable_v<T>,
        "Cannot convert Js to T; there is no available TypeConverter."
    );
    return internal::TypedConverter<T>::toCpp(value);
}


} // namespace qjspp