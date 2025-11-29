#include "qjspp/types/Undefined.hpp"

#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"

namespace qjspp {

IMPL_QJSPP_DEFINE_VALUE_COMMON(Undefined);
Undefined::Undefined() = default;

} // namespace qjspp