#include "qjspp/types/BigInt.hpp"

#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"

namespace qjspp {

IMPL_QJSPP_DEFINE_VALUE_COMMON(BigInt);
BigInt::BigInt(int64_t i64) : val_(JS_NewBigInt64(Locker::currentContextChecked(), i64)) {}
BigInt::BigInt(uint64_t u64) : val_(JS_NewBigUint64(Locker::currentContextChecked(), u64)) {}

int64_t BigInt::getInt64() const {
    int64_t ret;
    JsException::check(JS_ToBigInt64(Locker::currentContextChecked(), &ret, val_));
    return ret;
}
uint64_t BigInt::getUInt64() const {
    uint64_t ret;
    JsException::check(JS_ToBigUint64(Locker::currentContextChecked(), &ret, val_));
    return ret;
}

BigInt BigInt::newBigInt(int64_t i64) { return BigInt(i64); }
BigInt BigInt::newBigInt(uint64_t u64) { return BigInt(u64); }


} // namespace qjspp