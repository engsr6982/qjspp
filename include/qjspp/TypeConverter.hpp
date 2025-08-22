#pragma once
#include "qjspp/Concepts.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/Values.hpp"
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>


namespace qjspp {


template <typename T>
struct TypeConverter {
    static_assert(sizeof(T) == 0, "Cannot convert Js to this type T; no TypeConverter defined.");
};

template <typename T>
concept HasTypeConverter = requires { typename TypeConverter<T>; };


// bool <-> Boolean
template <>
struct TypeConverter<bool> {
    static Boolean toJs(bool value) { return Boolean{value}; }

    static bool toCpp(Value const& value) { return value.asBoolean().value(); }
};

// int/uint/float/double <-> Number
template <typename T>
    requires NumberLike_v<T>
struct TypeConverter<T> {
    static Number toJs(T value) { return Number(value); }

    static T toCpp(Value const& value) { return static_cast<T>(value.asNumber().getDouble()); }
};

// int64/uint64 <-> BigInt
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

// std::string <-> String
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

// std::function -> Function
template <typename R, typename... Args>
struct TypeConverter<std::function<R(Args...)>> {
    static_assert(
        (HasTypeConverter<Args> && ...),
        "Cannot convert std::function to Function; all parameter types must have a TypeConverter"
    );

    static Value toJs(std::function<R(Args...)> const& /* value */) {
        // ! UnSupported: cannot convert function to Value
        throw std::logic_error("UnSupported: cannot convert std::function to Value");
    }

    static std::function<R(Args...)> toCpp(Value const& value); // impl in Binding.inl
};

// std::optional <-> null/undefined
template <typename T>
struct TypeConverter<std::optional<T>> {
    static Value toJs(std::optional<T> const& value) {
        if (value) {
            return ConvertToJs(value.value());
        }
        return Null{}; // default to null
    }

    static std::optional<T> toCpp(Value const& value) {
        if (value.isUndefined() || value.isNull()) {
            return std::nullopt;
        }
        return std::optional<T>{ConvertToCpp<T>(value)};
    }
};

// std::vector <-> Array
template <typename T>
struct TypeConverter<std::vector<T>> {
    static Value toJs(std::vector<T> const& value) {
        auto array = Array{value.size()};
        for (std::size_t i = 0; i < value.size(); ++i) {
            array.set(i, ConvertToJs(value[i]));
        }
        return array;
    }

    static std::vector<T> toCpp(Value const& value) {
        auto array = value.asArray();

        std::vector<T> result;
        result.reserve(array.length());
        for (std::size_t i = 0; i < array.length(); ++i) {
            result.push_back(ConvertToCpp<T>(array[i]));
        }
        return result;
    }
};

// std::unordered_map <-> Object
template <typename K, typename V>
    requires StringLike_v<K> // JavaScript only supports string keys
struct TypeConverter<std::unordered_map<K, V>> {
    static_assert(HasTypeConverter<V>, "Cannot convert std::unordered_map to Object; type V has no TypeConverter");

    static Value toJs(std::unordered_map<K, V> const& value) {
        auto object = Object{};
        for (auto const& [key, val] : value) {
            object.set(key, ConvertToJs(val));
        }
        return object;
    }

    static std::unordered_map<K, V> toCpp(Value const& value) {
        auto object = value.asObject();
        auto keys   = object.getOwnPropertyNamesAsString();

        std::unordered_map<K, V> result;
        for (auto const& key : keys) {
            result[key] = ConvertToCpp<V>(object.get(key));
        }
        return result;
    }
};

// std::variant <-> Type
template <typename... Is>
struct TypeConverter<std::variant<Is...>> {
    static_assert(
        (HasTypeConverter<Is> && ...),
        "Cannot convert std::variant to Object; all types must have a TypeConverter"
    );
    using TypedVariant = std::variant<Is...>;

    static Value toJs(TypedVariant const& value) {
        if (value.valueless_by_exception()) {
            return Null{};
        }
        return std::visit([&](auto const& v) -> Value { return ConvertToJs(v); }, value);
    }

    static TypedVariant toCpp(Value const& value) { return tryToCpp(value); }

    template <size_t I = 0>
    static TypedVariant tryToCpp(Value const& value) {
        if constexpr (I >= sizeof...(Is)) {
            throw JsException{
                JsException::Type::RangeError,
                "Cannot convert Value to std::variant; no matching type found."
            };
        } else {
            using Type = std::variant_alternative_t<I, TypedVariant>;
            try {
                return ConvertToCpp<Type>(value);
            } catch (JsException const&) {
                return tryToCpp<I + 1>(value);
            }
        }
    }
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