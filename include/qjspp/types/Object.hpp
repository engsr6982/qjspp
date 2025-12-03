#pragma once
#include "ValueBase.hpp"

#include "qjspp/concepts/BasicConcepts.hpp"

#include <string>
#include <string_view>

namespace qjspp {

class Object final {
public:
    QJSPP_DEFINE_VALUE_COMMON(Object);

    [[nodiscard]] static Object newObject();

    [[nodiscard]] bool has(String const& key) const;
    [[nodiscard]] bool has(std::string_view key) const;

    [[nodiscard]] Value get(String const& key) const;
    [[nodiscard]] Value get(std::string_view key) const;

    void set(String const& key, Value const& value);
    void set(std::string_view key, Value const& value);

    void remove(String const& key);
    void remove(std::string_view key);

    template <concepts::StringLike T>
    [[nodiscard]] bool has(T const& key) const;

    template <concepts::StringLike T>
    [[nodiscard]] Value get(T const& key) const;

    template <concepts::StringLike T>
    void set(T const& key, Value const& value);

    template <concepts::StringLike T>
    void remove(T const& key);

    [[nodiscard]] std::vector<String> getOwnPropertyNames() const;

    [[nodiscard]] std::vector<std::string> getOwnPropertyNamesAsString() const;

    [[nodiscard]] bool instanceOf(Value const& value) const;

    bool defineOwnProperty(String const& key, Value const& value, PropertyAttributes attr = PropertyAttributes::None);
    bool
    defineOwnProperty(std::string_view key, Value const& value, PropertyAttributes attr = PropertyAttributes::None);
};

} // namespace qjspp

#include "Object.inl"