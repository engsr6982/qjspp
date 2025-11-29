#include "qjspp/types/Number.hpp"

#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"

namespace qjspp {

IMPL_QJSPP_DEFINE_VALUE_COMMON(Number);
Number::Number(double d) : val_(JS_NewFloat64(Locker::currentContextChecked(), d)) {}
Number::Number(float f) : Number{static_cast<double>(f)} {}
Number::Number(int i32) : val_(JS_NewInt32(Locker::currentContextChecked(), i32)) {}
Number::Number(int64_t i64) : val_(JS_NewInt64(Locker::currentContextChecked(), i64)) {}

float  Number::getFloat() const { return static_cast<float>(getDouble()); }
double Number::getDouble() const {
    double ret;
    JsException::check(JS_ToFloat64(Locker::currentContextChecked(), &ret, val_));
    return ret;
}
int Number::getInt32() const {
    int ret;
    JsException::check(JS_ToInt32(Locker::currentContextChecked(), &ret, val_));
    return ret;
}
int64_t Number::getInt64() const {
    int64_t ret;
    JsException::check(JS_ToInt64(Locker::currentContextChecked(), &ret, val_));
    return ret;
}


} // namespace qjspp