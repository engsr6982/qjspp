#pragma once
#include "qjspp/Types.hpp"
#include <concepts>
#include <string>
#include <type_traits>


namespace qjspp {


template <typename T>
concept IsInt64_v = std::is_same_v<T, int64_t>;

template <typename T>
concept IsUint64_v = std::is_same_v<T, uint64_t>;

template <typename T>
concept IsInt64OrUint64_v = IsInt64_v<T> || IsUint64_v<T>;

template <typename T>
concept NumberLike_v = std::is_arithmetic_v<T> && !IsInt64OrUint64_v<T>;

template <typename T>
concept StringLike_v = std::convertible_to<T, std::string_view> || std::same_as<std::remove_cvref_t<T>, std::string>;

template <typename T>
concept HasDefaultConstructor_v = requires {
    { T{} } -> std::same_as<T>;
    requires std::is_default_constructible_v<T>;
};

template <typename T>
concept HasUserDeclaredDestructor_v = !std::is_trivially_destructible_v<T>;

template <typename T>
concept HasEquality = requires(T const& lhs, T const& rhs) {
    { lhs == rhs } -> std::convertible_to<bool>;
};


//  helper
template <typename T>
concept IsWrappedType = std::same_as<T, Value> || std::same_as<T, Undefined> || std::same_as<T, Null>
                     || std::same_as<T, Boolean> || std::same_as<T, Number> || std::same_as<T, String>
                     || std::same_as<T, Object> || std::same_as<T, Array> || std::same_as<T, Function>;


template <typename T>
concept IsFunctionCallback = std::is_invocable_r_v<Value, T, Arguments const&>;

template <typename T>
concept IsInstanceMethodCallback = std::is_invocable_r_v<Value, T, void*, Arguments const&>;


// JavaScript value types: undefined、null、boolean、number、string、symbol
template <typename T>
constexpr bool IsJsValueLike_v =
    std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<std::decay_t<T>, std::string>;


} // namespace qjspp