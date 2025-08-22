#pragma once
#include "TypeConverter.hpp"
#include "qjspp/Concepts.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/Values.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <variant>

namespace qjspp {


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


template <typename R, typename... Args>
Value TypeConverter<std::function<R(Args...)>>::toJs(std::function<R(Args...)> const& /* value */) {
    throw std::logic_error("UnSupported: cannot convert std::function to Value");
}


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

template <>
struct TypeConverter<std::monostate> {
    static Value toJs(std::monostate) { return Null{}; }

    static std::monostate toCpp(Value const& value) {
        if (value.isUndefined() || value.isNull()) {
            return std::monostate{};
        }
        throw JsException{"Expected null/undefined for std::monostate"};
    }
};


// internal type
template <typename T>
    requires IsWrappedType<T>
struct TypeConverter<T> {
    static Value toJs(T const& value) { return value.asValue(); }
    static T     toCpp(Value const& value) { return value.as<T>(); }
};


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
