#include "qjspp/types/Boolean.hpp"

#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"

namespace qjspp {

IMPL_QJSPP_DEFINE_VALUE_COMMON(Boolean);
Boolean::Boolean(bool value) : val_(JS_NewBool(Locker::currentContextChecked(), value)) {}
bool Boolean::value() const { return JS_ToBool(Locker::currentContextChecked(), val_); }
Boolean::operator bool() const { return value(); }

Boolean Boolean::newBoolean(bool v) { return Boolean{v}; }

} // namespace qjspp