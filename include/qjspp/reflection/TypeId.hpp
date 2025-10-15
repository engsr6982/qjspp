#pragma once
#include <cstdint>
#include <string_view>

namespace qjspp::reflection {

namespace internal {

constexpr uint64_t fnv1a(std::string_view s) noexcept {
    uint64_t h = 1469598103934665603ull;
    for (char c : s) h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
    return h;
}

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

struct TypeId {
    std::string_view const s;
    uint64_t const         h;

    constexpr explicit TypeId(std::string_view s, uint64_t h) noexcept : s(s), h(h) {}
    constexpr explicit TypeId(std::string_view s) noexcept : s(s), h(internal::fnv1a(s)) {}

    [[nodiscard]] constexpr std::string_view str() const noexcept { return s; }
    [[nodiscard]] constexpr uint64_t         hash() const noexcept { return h; }

    constexpr bool operator==(const TypeId& other) const noexcept { return other.h == h; }
    constexpr bool operator!=(const TypeId& other) const noexcept { return other.h != h; }

    template <typename T>
    [[nodiscard]] constexpr bool isSameOf() const noexcept {
        return TypeId{TypeUnPrefixId_v<T>} == *this;
    }
};

template <typename T>
[[nodiscard]] constexpr TypeId getTypeId() noexcept {
    return TypeId{TypeUnPrefixId_v<T>};
}

} // namespace qjspp::reflection