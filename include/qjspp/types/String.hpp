#pragma once
#include "ValueBase.hpp"

#include <string>
#include <string_view>


namespace qjspp {

class String final {
public:
    QJSPP_DEFINE_VALUE_COMMON(String);
    explicit String(std::string_view utf8);
    explicit String(std::string const& value);
    explicit String(char const* value);

    [[nodiscard]] std::string value() const;
};

} // namespace qjspp