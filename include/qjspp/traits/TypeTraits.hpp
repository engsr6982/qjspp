#pragma once
#include <type_traits>

#include <string_view>

namespace qjspp::traits {


namespace internal {

template <typename T>
consteval std::string_view getRawTypeId() noexcept {
#if defined(_MSC_VER)
    constexpr std::string_view s{__FUNCSIG__};
    constexpr std::string_view f{"getRawTypeId<"};
    constexpr std::string_view e{">(void) noexcept"};
#else
    constexpr std::string_view s{__PRETTY_FUNCTION__};
    constexpr std::string_view f{"[T = "};
    constexpr std::string_view e{"]"};
#endif
    constexpr auto p = s.find(f) + f.size();
    return s.substr(p, s.size() - p - e.size());
}

template <auto T>
consteval std::string_view getRawTypeId() noexcept {
#if defined(_MSC_VER)
    constexpr std::string_view s{__FUNCSIG__};
    constexpr std::string_view f{"getRawTypeId<"};
    constexpr std::string_view e{">(void) noexcept"};
#else
    constexpr std::string_view s{__PRETTY_FUNCTION__};
    constexpr std::string_view f{"[T = "};
    constexpr std::string_view e{"]"};
#endif
    constexpr auto p = s.find(f) + f.size();
    return s.substr(p, s.size() - p - e.size());
}

constexpr std::string_view removeTypePrefix(std::string_view s) noexcept {
    auto trim = [&](std::string_view prefix) {
        if (s.starts_with(prefix)) s.remove_prefix(prefix.size());
    };
    trim("enum ");
    trim("class ");
    trim("struct ");
    trim("union ");
    return s;
}

} // namespace internal

template <typename T>
constexpr std::string_view TypeRawId_v = internal::getRawTypeId<T>();

template <typename T>
constexpr std::string_view TypeUnPrefixId_v = internal::removeTypePrefix(TypeRawId_v<T>);

template <typename T>
using RawType_t = std::remove_pointer_t<std::decay_t<T>>;


} // namespace qjspp::traits