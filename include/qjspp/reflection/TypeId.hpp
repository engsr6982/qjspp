#pragma once
#include <cstdint>
#include <string_view>

#include "qjspp/traits/TypeTraits.hpp"

namespace qjspp::reflection {


struct TypeId {
    std::string_view const s;
    uint64_t const         h;

    constexpr explicit TypeId(std::string_view s, uint64_t h) noexcept : s(s), h(h) {}
    constexpr explicit TypeId(std::string_view s) noexcept : s(s), h(fnv1a(s)) {}

    [[nodiscard]] constexpr std::string_view str() const noexcept { return s; }
    [[nodiscard]] constexpr uint64_t         hash() const noexcept { return h; }

    constexpr bool operator==(const TypeId& other) const noexcept { return other.h == h; }

    template <typename T>
    [[nodiscard]] constexpr bool isSameOf() const noexcept {
        return TypeId{traits::TypeUnPrefixId_v<T>} == *this;
    }

    static constexpr uint64_t fnv1a(std::string_view s) noexcept {
        uint64_t h = 1469598103934665603ull;
        for (char c : s) h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
        return h;
    }
};

template <typename T>
[[nodiscard]] constexpr TypeId getTypeId() noexcept {
    return TypeId{traits::TypeUnPrefixId_v<T>};
}

} // namespace qjspp::reflection