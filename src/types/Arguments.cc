#include "qjspp/types/Arguments.hpp"

#include "qjspp/types/Object.hpp"
#include "qjspp/types/Value.hpp"

namespace qjspp {

Arguments::Arguments(JsEngine* engine, JSValueConst thiz, int length, JSValueConst* args)
: engine_(engine),
  thiz_(thiz),
  length_(length),
  args_(args) {}

JsEngine* Arguments::engine() const { return engine_; }

bool Arguments::hasThiz() const { return JS_IsObject(thiz_); }

Object Arguments::thiz() const { return Value::wrap<Object>(thiz_); }

size_t Arguments::length() const { return length_; }

bool               Arguments::hasJsManagedResource() const { return managed_ != nullptr; }
JsManagedResource* Arguments::getJsManagedResource() const { return managed_; }

Value Arguments::operator[](size_t index) const {
    if (index >= length_) {
        return {}; // undefined
    }
    return Value::wrap<Value>(args_[index]);
}

} // namespace qjspp