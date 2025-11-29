#pragma once
#include "ValueBase.hpp"

namespace qjspp {

class Boolean final {
public:
    QJSPP_DEFINE_VALUE_COMMON(Boolean);

    Boolean(bool value);

    [[nodiscard]] bool value() const;

    operator bool() const;
};

} // namespace qjspp