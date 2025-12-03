#pragma once
#include <concepts>
#include <string>
#include <type_traits>


namespace qjspp ::concepts {


template <typename T>
concept NumberLike = std::is_arithmetic_v<T>;

template <typename T>
concept StringLike =
    std::is_same_v<std::remove_cvref_t<T>, std::string> || std::is_same_v<std::remove_cvref_t<T>, std::string_view>
    || std::is_same_v<std::remove_cvref_t<T>, const char*> || std::is_same_v<std::remove_cvref_t<T>, char*>;

template <typename T>
concept HasDefaultConstructor = requires { T{}; };

template <typename T>
concept HasEquality = requires(T const& lhs, T const& rhs) {
    { lhs == rhs } -> std::convertible_to<bool>;
};


} // namespace qjspp::concepts