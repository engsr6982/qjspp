#pragma once
#include "ValueBase.hpp"

namespace qjspp {

class Null final {
public:
    QJSPP_DEFINE_VALUE_COMMON(Null);

    Null();
};

} // namespace qjspp