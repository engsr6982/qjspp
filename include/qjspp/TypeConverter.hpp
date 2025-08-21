#pragma once
#include "qjspp/Concepts.hpp"
#include "qjspp/Values.hpp"
#include <stdexcept>
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


// int/uint/float/double → Number
template <typename T>
    requires(NumberLike_v<T> || IsInt64OrUint64_v<T>)
struct TypeConverter<T> {
    static Number toJs(T value) { return Number(value); }

    static T toCpp(Value const& value) { return static_cast<T>(value.asNumber().getDouble()); }
};

// int64/uint64 → BigInt
template <typename T>
    requires IsInt64OrUint64_v<T>
struct TypeConverter<T> {
    static BigInt toJs(T value) { return BigInt(value); }

    static T toCpp(Value const& value) {
        if constexpr (IsInt64_v<T>) {
            return value.asBigInt().getInt64();
        } else {
            return value.asBigInt().getUInt64();
        }
    }
};


template <typename T>
    requires(StringLike_v<T> && std::constructible_from<String, T>)
struct TypeConverter<T> {
    static String toJs(T const& value) { return String{value}; }

    static std::string toCpp(Value const& value) { return value.asString().value(); } // always UTF-8
};


// enum -> Number (enum value)
template <typename T>
    requires std::is_enum_v<T>
struct TypeConverter<T> {
    static Number toJs(T value) { return Number(static_cast<int>(value)); }

    static T toCpp(Value const& value) { return static_cast<T>(value.asNumber().getInt32()); }
};

template <typename R, typename... Args>
struct TypeConverter<std::function<R(Args...)>> {
    static Value toJs(std::function<R(Args...)> const& /* value */) {
        // ! UnSupported: cannot convert function to Value
        throw std::logic_error("Cannot convert std::function to Value.");
    }

    static std::function<R(Args...)> toCpp(Value const& value); // impl in Binding.inl
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


namespace detail {

template <typename T, typename = void>
constexpr bool has_toJs_Tref = false;

template <typename T>
constexpr bool has_toJs_Tref<T, std::void_t<decltype(TypedConverter<T>::toJs(std::declval<T&>()))>> = true;

template <typename T, typename = void>
constexpr bool has_toJs_Tval = false;

template <typename T>
constexpr bool has_toJs_Tval<T, std::void_t<decltype(TypedConverter<T>::toJs(std::declval<T>()))>> = true;

template <typename T, typename = void>
constexpr bool has_toJs_Tptr = false;

template <typename T>
constexpr bool has_toJs_Tptr<T, std::void_t<decltype(TypedConverter<T>::toJs(std::declval<std::add_pointer_t<T>>()))>> =
    true;

} // namespace detail

template <typename T, typename = void>
struct IsTypeConverterAvailable : std::false_type {};

template <typename T>
struct IsTypeConverterAvailable<T, std::void_t<decltype(TypedConverter<T>::toCpp(std::declval<Value>()))>>
: std::bool_constant<detail::has_toJs_Tref<T> || detail::has_toJs_Tval<T> || detail::has_toJs_Tptr<T>> {};


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


namespace detail_conv {

// 获取 TypedConverter 的 toCpp 返回类型（去掉引用/const）
template <typename T>
using TC = internal::TypedConverter<std::remove_cv_t<std::remove_reference_t<T>>>;

// 返回类型
template <typename T>
using TypedToCppRet = decltype(TC<T>::toCpp(std::declval<Value>()));

// helper: 是否是 U*
template <typename X>
inline constexpr bool is_pointer_v = std::is_pointer_v<std::remove_cv_t<std::remove_reference_t<X>>>;

// helper: 是否是 U&
template <typename X>
inline constexpr bool is_lvalue_ref_v = std::is_lvalue_reference_v<X>;

} // namespace detail_conv


template <typename T>
[[nodiscard]] inline decltype(auto) ConvertToCpp(Value const& value) {
    static_assert(
        internal::IsTypeConverterAvailable_v<T>,
        "Cannot convert Js to T; there is no available TypeConverter."
    );

    using RequestedT = T;                                                     // 可能为 U, U&, U*
    using BareT      = std::remove_cv_t<std::remove_reference_t<RequestedT>>; // U

    using Conv    = detail_conv::TC<RequestedT>;            // TypedConverter<U>
    using ConvRet = detail_conv::TypedToCppRet<RequestedT>; // decltype(Conv::toCpp(Value))

    if constexpr (std::is_lvalue_reference_v<RequestedT>) { // 需要 U&
        if constexpr (std::is_pointer_v<std::remove_reference_t<ConvRet>>) {
            auto p = Conv::toCpp(value); // 返回 U*
            if (!p) throw std::runtime_error("TypeConverter::toCpp returned a null pointer.");
            return static_cast<RequestedT&>(*p); // 返回 U&
        } else if constexpr (std::is_lvalue_reference_v<ConvRet>
                             || std::is_const_v<std::remove_reference_t<RequestedT>>) {
            return Conv::toCpp(value); // 已返回 U&，直接转发 或者 const T& 可以绑定临时
        } else {
            static_assert(
                std::is_pointer_v<std::remove_reference_t<ConvRet>> || std::is_lvalue_reference_v<ConvRet>,
                "TypeConverter::toCpp must return either U* or U& when ConvertToCpp<T&> is required. Returning U (by "
                "value) cannot bind to a non-const lvalue reference; change TypeConverter or request a value type."
            );
        }
    } else if constexpr (std::is_pointer_v<RequestedT>) { // 需要 U*
        if constexpr (std::is_pointer_v<std::remove_reference_t<ConvRet>>) {
            return Conv::toCpp(value); // 直接返回
        } else if constexpr (std::is_lvalue_reference_v<ConvRet>) {
            return std::addressof(Conv::toCpp(value)); // 返回 U& -> 可以取地址
        } else {
            static_assert(
                std::is_pointer_v<std::remove_reference_t<ConvRet>> || std::is_lvalue_reference_v<ConvRet>,
                "TypeConverter::toCpp must return U* or U& when ConvertToCpp<U*> is required. "
                "Returning U (by value) would produce pointer to temporary (unsafe)."
            );
        }
    } else {
        // 值类型 U
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<ConvRet>>, BareT>
                      && !std::is_pointer_v<std::remove_reference_t<ConvRet>> && !std::is_lvalue_reference_v<ConvRet>) {
            return Conv::toCpp(value); // 按值返回 / 直接返回 (可能 NRVO)
        } else {
            static_assert(
                std::is_same_v<std::remove_cv_t<std::remove_reference_t<ConvRet>>, BareT>
                    && !std::is_pointer_v<std::remove_reference_t<ConvRet>> && !std::is_lvalue_reference_v<ConvRet>,
                "TypeConverter::toCpp must return U (by value) for ConvertToCpp<U>. "
                "Other return forms (U* or U&) are not supported for value request."
            );
        }
    }
}


} // namespace qjspp