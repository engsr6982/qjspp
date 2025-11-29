#pragma once
#include "ValueBase.hpp"

namespace qjspp {

class Undefined final {
public:
    QJSPP_DEFINE_VALUE_COMMON(Undefined);

    Undefined();
};

} // namespace qjspp