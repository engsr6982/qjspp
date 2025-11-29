#include "qjspp/types/ScopedJsValue.hpp"

#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/Value.hpp"

namespace qjspp {

ScopedJsValue::ScopedJsValue(Value value) {
    val_    = std::move(value);
    engine_ = &Locker::currentEngineChecked();
}
ScopedJsValue::ScopedJsValue(JsEngine* engine, Value val) : engine_(engine), val_(std::move(val)) {}
ScopedJsValue::ScopedJsValue(ScopedJsValue&& other) noexcept {
    engine_ = other.engine_;
    val_    = std::move(other.val_);
}
ScopedJsValue& ScopedJsValue::operator=(ScopedJsValue&& other) noexcept {
    engine_ = other.engine_;
    val_    = std::move(other.val_);
    return *this;
}
ScopedJsValue::ScopedJsValue(ScopedJsValue const& copy) {
    engine_ = copy.engine_;
    val_    = copy.val_;
}
ScopedJsValue& ScopedJsValue::operator=(ScopedJsValue const& copy) = default;
ScopedJsValue::~ScopedJsValue() { reset(); }
void ScopedJsValue::reset() {
    if (val_.isValid()) {
        Locker lock{engine_};
        val_.reset();
    }
}
JsEngine* ScopedJsValue::engine() const { return engine_; }
Value     ScopedJsValue::value() const { return val_; }
ScopedJsValue::operator Value() const {
    Locker lock{engine_};
    return val_;
}

} // namespace qjspp