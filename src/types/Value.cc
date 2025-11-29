#include "qjspp/types/Value.hpp"

#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/Array.hpp"
#include "qjspp/types/BigInt.hpp"
#include "qjspp/types/Boolean.hpp"
#include "qjspp/types/Function.hpp"
#include "qjspp/types/Null.hpp"
#include "qjspp/types/Number.hpp"
#include "qjspp/types/Object.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Undefined.hpp"

namespace qjspp {

IMPL_QJSPP_DEFINE_VALUE_COMMON(Value);
Value::Value() = default;

ValueType Value::type() const {
    if (isUndefined()) {
        return ValueType::Undefined;
    } else if (isNull()) {
        return ValueType::Null;
    } else if (isBoolean()) {
        return ValueType::Boolean;
    } else if (isNumber()) {
        return ValueType::Number;
    } else if (isBigInt()) {
        return ValueType::BigInt;
    } else if (isString()) {
        return ValueType::String;
    } else if (isObject()) {
        return ValueType::Object;
    } else if (isArray()) {
        return ValueType::Array;
    } else if (isFunction()) {
        return ValueType::Function;
    }
    [[unlikely]] throw JsException(JsException::Type::InternalError, "Unknown type, did you forget to add if branch?");
}

bool Value::isUninitialized() const { return JS_IsUninitialized(val_); }
bool Value::isUndefined() const { return JS_IsUndefined(val_); }
bool Value::isNull() const { return JS_IsNull(val_); }
bool Value::isBoolean() const { return JS_IsBool(val_); }
bool Value::isNumber() const { return JS_IsNumber(val_); }
bool Value::isBigInt() const { return JS_IsBigInt(Locker::currentContextChecked(), val_); }
bool Value::isString() const { return JS_IsString(val_); }
bool Value::isObject() const { return JS_IsObject(val_); }
bool Value::isArray() const { return JS_IsArray(val_); }
bool Value::isFunction() const { return JS_IsFunction(Locker::currentContextChecked(), val_); }

Undefined Value::asUndefined() const {
    if (!isUndefined()) throw JsException{JsException::Type::InternalError, "can't convert to Undefined"};
    return Undefined{val_};
}
Null Value::asNull() const {
    if (!isNull()) throw JsException{JsException::Type::InternalError, "can't convert to Null"};
    return Null{val_};
}
Boolean Value::asBoolean() const {
    if (!isBoolean()) throw JsException{JsException::Type::InternalError, "can't convert to Boolean"};
    return Boolean{val_};
}
Number Value::asNumber() const {
    if (!isNumber()) throw JsException{JsException::Type::InternalError, "can't convert to Number"};
    return Number{val_};
}
BigInt Value::asBigInt() const {
    if (!isBigInt()) throw JsException{JsException::Type::InternalError, "can't convert to BigInt"};
    return BigInt{val_};
}
String Value::asString() const {
    if (!isString()) throw JsException{JsException::Type::InternalError, "can't convert to String"};
    return String{val_};
}
Object Value::asObject() const {
    if (!isObject()) throw JsException{JsException::Type::InternalError, "can't convert to Object"};
    return Object{val_};
}
Array Value::asArray() const {
    if (!isArray()) throw JsException{JsException::Type::InternalError, "can't convert to Array"};
    return Array{val_};
}
Function Value::asFunction() const {
    if (!isFunction()) throw JsException{JsException::Type::InternalError, "can't convert to Function"};
    return Function{val_};
}

} // namespace qjspp