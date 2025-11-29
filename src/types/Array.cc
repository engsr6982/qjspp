#include "qjspp/types/Array.hpp"

#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"

namespace qjspp {

IMPL_QJSPP_DEFINE_VALUE_COMMON(Array);
Array::Array(size_t size) {
    auto& engine = Locker::currentEngineChecked();
    auto  array  = JS_NewArray(engine.context_);
    JsException::check(array);
    if (size != 0) {
        auto length = JS_NewInt32(engine.context_, static_cast<int32_t>(size));
        JsException::check(JS_SetProperty(engine.context_, array, engine.lengthAtom_, length));
    }
    val_ = array;
}

size_t Array::length() const {
    auto& engine = Locker::currentEngineChecked();
    auto  ret    = JS_GetProperty(engine.context_, val_, engine.lengthAtom_);
    JsException::check(ret);
    uint32_t length = 0;
    if (JS_IsNumber(ret)) {
        JS_ToUint32(engine.context_, &length, ret);
        JS_FreeValue(engine.context_, ret);
    } else [[unlikely]] {
        JS_FreeValue(engine.context_, ret);
        throw JsException{JsException::Type::TypeError, "Array.length got not a number"};
    }
    return length;
}

Value Array::operator[](size_t index) const { return get(index); }
Value Array::get(size_t index) const {
    auto ret = JS_GetPropertyUint32(Locker::currentContextChecked(), val_, static_cast<uint32_t>(index));
    JsException::check(ret);
    return Value::move<Value>(ret);
}

void Array::set(size_t index, Value const& value) {
    auto ctx = Locker::currentContextChecked();
    JsException::check(
        JS_SetPropertyInt64(ctx, val_, static_cast<int64_t>(index), JS_DupValue(ctx, Value::extract(value)))
    );
}

void Array::push(Value const& value) { set(length(), value); }

void Array::clear() {
    auto& engine = Locker::currentEngineChecked();
    auto  length = JS_NewInt32(engine.context_, static_cast<uint32_t>(0));
    JsException::check(JS_SetProperty(engine.context_, val_, engine.lengthAtom_, length));
}


} // namespace qjspp