#pragma once
#include "ValueBase.hpp"

#include <string>

namespace qjspp {

class Object final {
public:
    QJSPP_DEFINE_VALUE_COMMON(Object);

    [[nodiscard]] static Object newObject();

    [[nodiscard]] bool has(String const& key) const;
    [[nodiscard]] bool has(std::string const& key) const;

    [[nodiscard]] Value get(String const& key) const;
    [[nodiscard]] Value get(std::string const& key) const;

    void set(String const& key, Value const& value);
    void set(std::string const& key, Value const& value);

    void remove(String const& key);
    void remove(std::string const& key);

    [[nodiscard]] std::vector<String> getOwnPropertyNames() const;

    [[nodiscard]] std::vector<std::string> getOwnPropertyNamesAsString() const;

    [[nodiscard]] bool instanceOf(Value const& value) const;

    bool defineOwnProperty(String const& key, Value const& value, PropertyAttributes attr = PropertyAttributes::None);
    bool
    defineOwnProperty(std::string const& key, Value const& value, PropertyAttributes attr = PropertyAttributes::None);
};

} // namespace qjspp