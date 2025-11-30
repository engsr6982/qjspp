#include "qjspp/types/Object.hpp"

#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"

namespace qjspp {

IMPL_QJSPP_DEFINE_VALUE_COMMON(Object);

Object Object::newObject() {
    auto obj = JS_NewObject(Locker::currentContextChecked());
    JsException::check(obj);
    return Value::move<Object>(obj);
}

bool Object::has(String const& key) const { return has(key.value()); }
bool Object::has(std::string const& key) const {
    auto ctx  = Locker::currentContextChecked();
    auto atom = JS_NewAtomLen(ctx, key.c_str(), key.size());

    auto ret = JS_HasProperty(ctx, val_, atom);
    JS_FreeAtom(ctx, atom);

    JsException::check(ret);
    return ret != 0;
}

Value Object::get(String const& key) const { return get(key.value()); }
Value Object::get(std::string const& key) const {
    auto ret = JS_GetPropertyStr(Locker::currentContextChecked(), val_, key.c_str());
    JsException::check(ret);
    return Value::move<Value>(ret);
}

void Object::set(String const& key, Value const& value) { set(key.value(), value); }
void Object::set(std::string const& key, Value const& value) {
    auto ctx = Locker::currentContextChecked();
    auto ret = JS_SetPropertyStr(ctx, val_, key.c_str(), JS_DupValue(ctx, Value::extract(value)));
    JsException::check(ret);
}

void Object::remove(String const& key) { remove(key.value()); }
void Object::remove(std::string const& key) {
    auto ctx  = Locker::currentContextChecked();
    auto atom = JS_NewAtomLen(ctx, key.c_str(), key.size());
    auto ret  = JS_DeleteProperty(ctx, val_, atom, 0);
    JS_FreeAtom(ctx, atom);
    JsException::check(ret);
}

std::vector<String> Object::getOwnPropertyNames() const {
    auto            ctx  = Locker::currentContextChecked();
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
    auto ret = JS_IsInstanceOf(Locker::currentContextChecked(), val_, Value::extract(value));
    JsException::check(ret);
    return ret != 0;
}

bool Object::defineOwnProperty(String const& key, Value const& value, PropertyAttributes attr) {
    return defineOwnProperty(key.value(), value, attr);
}
bool Object::defineOwnProperty(std::string const& key, Value const& value, PropertyAttributes attr) {
    auto ctx = Locker::currentContextChecked();

    JSAtom atom  = JS_NewAtom(ctx, key.c_str());
    int    flags = toQuickJSFlags(attr);

    int ret = JS_DefinePropertyValue(ctx, val_, atom, JS_DupValue(ctx, Value::extract(value)), flags);
    JS_FreeAtom(ctx, atom);
    JsException::check(ret);
    return ret != 0;
}


} // namespace qjspp