#include "qjspp/Values.hpp"
#include "qjspp/JsEngine.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/JsScope.hpp"
#include "qjspp/Types.hpp"
#include "quickjs.h"
#include <cstddef>
#include <cstdint>
#include <string_view>


namespace qjspp {


Value::Value() : val_(JS_UNDEFINED) {}

ValueType Value::type() const {
    if (isUndefined()) {
        return ValueType::Undefined;
    } else if (isNull()) {
        return ValueType::Null;
    } else if (isBoolean()) {
        return ValueType::Boolean;
    } else if (isNumber()) {
        return ValueType::Number;
    } else if (isString()) {
        return ValueType::String;
    } else if (isObject()) {
        return ValueType::Object;
    } else if (isArray()) {
        return ValueType::Array;
    } else if (isFunction()) {
        return ValueType::Function;
    }
    throw JsException("Unknown type, did you forget to add if branch?");
}

bool Value::isUninitialized() const { return JS_IsUninitialized(val_); }
bool Value::isUndefined() const { return JS_IsUndefined(val_); }
bool Value::isNull() const { return JS_IsNull(val_); }
bool Value::isBoolean() const { return JS_IsBool(val_); }
bool Value::isNumber() const { return JS_IsNumber(val_); }
bool Value::isString() const { return JS_IsString(val_); }
bool Value::isObject() const { return JS_IsObject(val_); }
bool Value::isArray() const { return JS_IsArray(val_); }
bool Value::isFunction() const { return JS_IsFunction(JsScope::currentContextChecked(), val_); }

Undefined Value::asUndefined() const {
    if (!isUndefined()) throw JsException{"can't convert to Undefined"};
    return Undefined{val_};
}
Null Value::asNull() const {
    if (!isNull()) throw JsException{"can't convert to Null"};
    return Null{val_};
}
Boolean Value::asBoolean() const {
    if (!isBoolean()) throw JsException{"can't convert to Boolean"};
    return Boolean{val_};
}
Number Value::asNumber() const {
    if (!isNumber()) throw JsException{"can't convert to Number"};
    return Number{val_};
}
String Value::asString() const {
    if (!isString()) throw JsException{"can't convert to String"};
    return String{val_};
}
Object Value::asObject() const {
    if (!isObject()) throw JsException{"can't convert to Object"};
    return Object{val_};
}
Array Value::asArray() const {
    if (!isArray()) throw JsException{"can't convert to Array"};
    return Array{val_};
}
Function Value::asFunction() const {
    if (!isFunction()) throw JsException{"can't convert to Function"};
    return Function{val_};
}


#define IMPL_SPECIALIZE_RAII(TYPE)                                                                                     \
    TYPE::TYPE(JSValue val) : val_(JS_DupValue(JsScope::currentContextChecked(), val)) {}                              \
    TYPE::~TYPE() { JS_FreeValue(JsScope::currentContextChecked(), val_); }                                            \
    Value TYPE::asValue() const { return Value(val_); }                                                                \
    void  TYPE::reset() {                                                                                              \
        if (!JS_IsUndefined(val_)) {                                                                                  \
            JS_FreeValue(JsScope::currentContextChecked(), val_);                                                     \
            val_ = JS_UNDEFINED;                                                                                      \
        }                                                                                                             \
    }                                                                                                                  \
    String TYPE::toString() const {                                                                                    \
        auto ret = JS_ToString(JsScope::currentContextChecked(), val_);                                                \
        JsException::check(ret);                                                                                       \
        return String(ret);                                                                                            \
    }                                                                                                                  \
    bool TYPE::operator==(Value const& other) const {                                                                  \
        return JS_IsStrictEqual(JsScope::currentContextChecked(), val_, other.val_);                                   \
    }


#define IMPL_SPECIALIZE_COPY_AND_MOVE(TYPE)                                                                            \
    TYPE::TYPE(TYPE const& copy) : val_(JS_DupValue(JsScope::currentContextChecked(), copy.val_)) {}                   \
    TYPE& TYPE::operator=(TYPE const& copy) {                                                                          \
        if (this != &copy) {                                                                                           \
            JS_FreeValue(JsScope::currentContextChecked(), val_);                                                      \
            val_ = JS_DupValue(JsScope::currentContextChecked(), copy.val_);                                           \
        }                                                                                                              \
        return *this;                                                                                                  \
    }                                                                                                                  \
    TYPE::TYPE(TYPE&& move) noexcept : val_(move.val_) { move.val_ = JS_UNDEFINED; }                                   \
    TYPE& TYPE::operator=(TYPE&& move) noexcept {                                                                      \
        if (this != &move) {                                                                                           \
            if (!JS_IsUndefined(val_)) {                                                                               \
                JS_FreeValue(JsScope::currentContextChecked(), val_);                                                  \
            }                                                                                                          \
            val_      = move.val_;                                                                                     \
            move.val_ = JS_UNDEFINED;                                                                                  \
        }                                                                                                              \
        return *this;                                                                                                  \
    }


IMPL_SPECIALIZE_RAII(Value);
IMPL_SPECIALIZE_COPY_AND_MOVE(Value);


IMPL_SPECIALIZE_RAII(Undefined);
IMPL_SPECIALIZE_COPY_AND_MOVE(Undefined);
Undefined::Undefined() : val_(JS_UNDEFINED) {}


IMPL_SPECIALIZE_RAII(Null);
IMPL_SPECIALIZE_COPY_AND_MOVE(Null);
Null::Null() : val_(JS_NULL) {}


IMPL_SPECIALIZE_RAII(Boolean);
IMPL_SPECIALIZE_COPY_AND_MOVE(Boolean);
Boolean::Boolean(bool value) : val_{JS_NewBool(JsScope::currentContextChecked(), value)} {}
bool Boolean::value() const { return JS_ToBool(JsScope::currentContextChecked(), val_); }


IMPL_SPECIALIZE_RAII(Number);
IMPL_SPECIALIZE_COPY_AND_MOVE(Number);
Number::Number(double d) : val_(JS_NewFloat64(JsScope::currentContextChecked(), d)) {}
Number::Number(float f) : Number{static_cast<double>(f)} {}
Number::Number(int i32) : val_{JS_NewInt32(JsScope::currentContextChecked(), i32)} {}
Number::Number(int64_t i64) : val_{JS_NewInt64(JsScope::currentContextChecked(), i64)} {}

float  Number::getFloat() const { return static_cast<float>(getDouble()); }
double Number::getDouble() const {
    double ret;
    JsException::check(JS_ToFloat64(JsScope::currentContextChecked(), &ret, val_));
    return ret;
}
int Number::getInt32() const {
    int ret;
    JsException::check(JS_ToInt32(JsScope::currentContextChecked(), &ret, val_));
    return ret;
}
int64_t Number::getInt64() const {
    int64_t ret;
    JsException::check(JS_ToInt64(JsScope::currentContextChecked(), &ret, val_));
    return ret;
}


IMPL_SPECIALIZE_RAII(String);
IMPL_SPECIALIZE_COPY_AND_MOVE(String);
String::String(std::string_view utf8) {
    auto cstr = JS_NewStringLen(JsScope::currentContextChecked(), utf8.data(), utf8.size());
    JsException::check(cstr);
    val_ = cstr;
}
String::String(std::string const& value) : String{std::string_view{value}} {}
String::String(char const* value) : String{std::string_view{value}} {}

std::string String::value() const {
    auto   ctx = JsScope::currentContextChecked();
    size_t len;
    auto   cstr = JS_ToCStringLen(ctx, &len, val_);
    if (cstr == nullptr) {
        throw JsException{"failed to convert String to std::string"};
    }
    std::string copy{cstr, len};
    JS_FreeCString(ctx, cstr);
    return copy;
}


IMPL_SPECIALIZE_RAII(Object);
IMPL_SPECIALIZE_COPY_AND_MOVE(Object);
Object::Object() {
    auto obj = JS_NewObject(JsScope::currentContextChecked());
    JsException::check(obj);
    val_ = obj;
}

bool Object::has(String const& key) const { return has(key.value()); }
bool Object::has(std::string const& key) const {
    auto ctx  = JsScope::currentContextChecked();
    auto atom = JS_NewAtomLen(ctx, key.c_str(), key.size());

    auto ret = JS_HasProperty(ctx, val_, atom);
    JS_FreeAtom(ctx, atom);

    JsException::check(ret);
    return ret != 0;
}

Value Object::get(String const& key) const { return get(key.value()); }
Value Object::get(std::string const& key) const {
    auto ret = JS_GetPropertyStr(JsScope::currentContextChecked(), val_, key.c_str());
    JsException::check(ret);
    return Value::move<Value>(ret);
}

void Object::set(String const& key, Value const& value) { set(key.value(), value); }
void Object::set(std::string const& key, Value const& value) {
    auto ctx = JsScope::currentContextChecked();
    auto ret = JS_SetPropertyStr(ctx, val_, key.c_str(), JS_DupValue(ctx, value.val_));
    JsException::check(ret);
}

void Object::remove(String const& key) { remove(key.value()); }
void Object::remove(std::string const& key) {
    auto ctx  = JsScope::currentContextChecked();
    auto atom = JS_NewAtomLen(ctx, key.c_str(), key.size());
    auto ret  = JS_DeleteProperty(ctx, val_, atom, 0);
    JS_FreeAtom(ctx, atom);
    JsException::check(ret);
}

std::vector<String> Object::getOwnPropertyNames() const {
    auto            ctx  = JsScope::currentContextChecked();
    JSPropertyEnum* ptab = nullptr;
    uint32_t        len  = 0;

    JsException::check(
        JS_GetOwnPropertyNames(ctx, &ptab, &len, val_, JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK | JS_GPN_PRIVATE_MASK)
    );

    std::unique_ptr<JSPropertyEnum, std::function<void(JSPropertyEnum*)>> ptr(ptab, [ctx](JSPropertyEnum* list) {
        if (list) js_free(ctx, list);
    });

    std::vector<String> ret;
    ret.reserve(len);
    for (uint32_t i = 0; i < len; ++i) {
        ret.push_back(Value::move<String>(JS_AtomToString(ctx, ptab[i].atom)));
        JS_FreeAtom(ctx, ptab[i].atom);
    }
    return ret;
}

std::vector<std::string> Object::getOwnPropertyNamesAsString() const {
    auto                     names = getOwnPropertyNames();
    std::vector<std::string> ret;
    ret.reserve(names.size());
    for (auto const& name : names) ret.push_back(name.value());
    return ret;
}

bool Object::instanceOf(Value const& value) const {
    if (!value.isObject()) return false;
    auto ret = JS_IsInstanceOf(JsScope::currentContextChecked(), val_, value.val_);
    JsException::check(ret);
    return ret != 0;
}

bool Object::defineOwnProperty(String const& key, Value const& value, PropertyAttributes attr) {
    return defineOwnProperty(key.value(), value, attr);
}
bool Object::defineOwnProperty(std::string const& key, Value const& value, PropertyAttributes attr) {
    auto ctx = JsScope::currentContextChecked();

    JSAtom atom  = JS_NewAtom(ctx, key.c_str());
    int    flags = toQuickJSFlags(attr);

    int ret = JS_DefinePropertyValue(ctx, val_, atom, Value::extract(value), flags);
    JS_FreeAtom(ctx, atom);
    JsException::check(ret);
    return ret != 0;
}


IMPL_SPECIALIZE_RAII(Array);
IMPL_SPECIALIZE_COPY_AND_MOVE(Array);
Array::Array(size_t size) {
    auto& engine = JsScope::currentEngineChecked();
    auto  array  = JS_NewArray(engine.context_);
    JsException::check(array);
    if (size != 0) {
        auto length = JS_NewInt32(engine.context_, static_cast<int32_t>(size));
        JsException::check(JS_SetProperty(engine.context_, array, engine.lengthAtom_, length));
    }
    val_ = array;
}

size_t Array::length() const {
    auto& engine = JsScope::currentEngineChecked();
    auto  ret    = JS_GetProperty(engine.context_, val_, engine.lengthAtom_);
    JsException::check(ret);
    uint32_t length = 0;
    if (JS_IsNumber(ret)) {
        JS_ToUint32(engine.context_, &length, ret);
        JS_FreeValue(engine.context_, ret);
    } else {
        JS_FreeValue(engine.context_, ret);
        throw JsException{"Array::length got not a number"};
    }
    return length;
}

Value Array::operator[](size_t index) const { return get(index); }
Value Array::get(size_t index) const {
    auto ret = JS_GetPropertyUint32(JsScope::currentContextChecked(), val_, static_cast<uint32_t>(index));
    JsException::check(ret);
    return Value::move<Value>(ret);
}

void Array::set(size_t index, Value const& value) {
    auto ctx = JsScope::currentContextChecked();
    JsException::check(JS_SetPropertyInt64(ctx, val_, static_cast<int64_t>(index), JS_DupValue(ctx, value.val_)));
}

void Array::push(Value const& value) { set(length(), value); }

void Array::clear() {
    auto& engine = JsScope::currentEngineChecked();
    auto  length = JS_NewInt32(engine.context_, static_cast<uint32_t>(0));
    JsException::check(JS_SetProperty(engine.context_, val_, engine.lengthAtom_, length));
}


IMPL_SPECIALIZE_RAII(Function);
IMPL_SPECIALIZE_COPY_AND_MOVE(Function);
Function::Function(FunctionCallback callback) {
    auto ptr = std::make_unique<FunctionCallback>(std::move(callback));

    auto& engine = JsScope::currentEngineChecked();

    auto fnData = JS_NewObjectClass(engine.context_, static_cast<int>(JsEngine::kFunctionDataClassId));
    JsException::check(fnData);
    JS_SetOpaque(fnData, ptr.release());

    auto engineData = JS_NewObjectClass(engine.context_, static_cast<int>(JsEngine::kPointerClassId));
    JsException::check(engineData);
    JS_SetOpaque(engineData, &engine);

    JSValue data[2] = {fnData, engineData};

    auto fn = JS_NewCFunctionData(
        engine.context_,
        [](JSContext* ctx, JSValueConst thiz, int argc, JSValueConst* argv, int /*magic*/, JSValue* data) -> JSValue {
            auto cb =
                static_cast<FunctionCallback*>(JS_GetOpaque(data[0], static_cast<int>(JsEngine::kFunctionDataClassId)));
            auto engine = static_cast<JsEngine*>(JS_GetOpaque(data[1], static_cast<int>(JsEngine::kPointerClassId)));
            try {
                auto result = (*cb)(Arguments{engine, thiz, argc, argv});
                return JS_DupValue(ctx, result.val_);
            } catch (JsException const& e) {
                return e.rethrowToEngine();
            }
        },
        0,
        0,
        2,
        data
    );
    JsException::check(fn);
    JS_FreeValue(engine.context_, fnData);
    JS_FreeValue(engine.context_, engineData);
    val_ = fn;
}

Value Function::call(Value const& thiz, std::vector<Value> const& args) const {
    auto& engine = JsScope::currentEngineChecked();

    static_assert(sizeof(Value) == sizeof(JSValue), "Value and JSValue must have the same size");
    int      argc = static_cast<int>(args.size());
    JSValue* argv = reinterpret_cast<JSValue*>(const_cast<Value*>(args.data())); // fast

    auto ret = JS_Call(engine.context_, val_, thiz.isObject() ? thiz.val_ : JS_UNDEFINED, argc, argv);
    JsException::check(ret);
    engine.pumpJobs();

    return Value::move<Value>(ret);
}

Value Function::call() const { return call(Value{}, {}); }

#undef IMPL_SPECIALIZE_RAII
#undef IMPL_SPECIALIZE_COPY_AND_MOVE


Arguments::Arguments(JsEngine* engine, JSValueConst thiz, int length, JSValueConst* args)
: engine_(engine),
  thiz_(thiz),
  length_(length),
  args_(args) {}

JsEngine* Arguments::engine() const { return engine_; }

bool Arguments::hasThiz() const { return JS_IsObject(thiz_); }

Object Arguments::thiz() const { return Value::wrap<Object>(thiz_); }

size_t Arguments::length() const { return length_; }

Value Arguments::operator[](size_t index) const {
    if (index >= length_) {
        return {}; // undefined
    }
    return Value::wrap<Value>(args_[index]);
}


} // namespace qjspp