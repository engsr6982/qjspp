#pragma once
#include <concepts>
#include <string>
#include <type_traits>


namespace qjspp ::concepts {


template <typename T>
concept NumberLike = std::is_arithmetic_v<T>;

template <typename T>
concept StringLike = std::convertible_to<T, std::string_view> || std::same_as<std::remove_cvref_t<T>, std::string>;

template <typename T>
concept HasDefaultConstructor = requires { T{}; };

template <typename T>
concept HasEquality = requires(T const& lhs, T const& rhs) {
    { lhs == rhs } -> std::convertible_to<bool>;
};


} // namespace qjspp::concepts