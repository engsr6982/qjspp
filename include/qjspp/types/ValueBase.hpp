#pragma once
#include "qjspp/Forward.hpp"
#include "qjspp/Global.hpp"

namespace qjspp {


#define QJSPP_DEFINE_VALUE_COMMON(ValueType)                                                                           \
private:                                                                                                               \
    JSValue val_{JS_UNDEFINED};                                                                                        \
    friend Value;                                                                                                      \
                                                                                                                       \
public:                                                                                                                \
    QJSPP_DISABLE_NEW();                                                                                               \
                                                                                                                       \
    explicit ValueType(JSValue value);                                                                                 \
                                                                                                                       \
    ValueType(ValueType const& copy);                                                                                  \
    ValueType& operator=(ValueType const& copy);                                                                       \
    ValueType(ValueType&& move) noexcept;                                                                              \
    ValueType& operator=(ValueType&& move) noexcept;                                                                   \
                                                                                                                       \
    ~ValueType();                                                                                                      \
                                                                                                                       \
    bool isValid() const;                                                                                              \
                                                                                                                       \
    void reset();                                                                                                      \
                                                                                                                       \
    [[nodiscard]] String toString() const;                                                                             \
                                                                                                                       \
    [[nodiscard]] Value asValue() const;                                                                               \
                                                                                                                       \
    operator Value() const;                                                                                            \
                                                                                                                       \
    bool operator==(Value const& other) const


#define IMPL_QJSPP_DEFINE_VALUE_COMMON(ValueType)                                                                      \
    ValueType::ValueType(JSValue value) : val_(JS_DupValue(Locker::currentContextChecked(), value)) {}                 \
    ValueType::~ValueType() {                                                                                          \
        if (isValid()) {                                                                                               \
            JS_FreeValue(Locker::currentContextChecked(), val_);                                                       \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    ValueType::ValueType(ValueType const& copy) : val_(JS_DupValue(Locker::currentContextChecked(), copy.val_)) {}     \
    ValueType& ValueType::operator=(ValueType const& copy) {                                                           \
        if (this != &copy) {                                                                                           \
            if (isValid()) {                                                                                           \
                JS_FreeValue(Locker::currentContextChecked(), val_);                                                   \
            }                                                                                                          \
            val_ = JS_DupValue(Locker::currentContextChecked(), copy.val_);                                            \
        }                                                                                                              \
        return *this;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    ValueType::ValueType(ValueType&& move) noexcept : val_(move.val_) { move.val_ = JS_UNDEFINED; }                    \
    ValueType& ValueType::operator=(ValueType&& move) noexcept {                                                       \
        if (this != &move) {                                                                                           \
            if (isValid()) {                                                                                           \
                JS_FreeValue(Locker::currentContextChecked(), val_);                                                   \
            }                                                                                                          \
            val_      = move.val_;                                                                                     \
            move.val_ = JS_UNDEFINED;                                                                                  \
        }                                                                                                              \
        return *this;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    bool ValueType::isValid() const { return !JS_IsUninitialized(val_) && !JS_IsUndefined(val_) && !JS_IsNull(val_); } \
                                                                                                                       \
    void ValueType::reset() {                                                                                          \
        if (isValid()) {                                                                                               \
            JS_FreeValue(Locker::currentContextChecked(), val_);                                                       \
            val_ = JS_UNDEFINED;                                                                                       \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    String ValueType::toString() const {                                                                               \
        auto str = JS_ToString(Locker::currentContextChecked(), val_);                                                 \
        JsException::check(str);                                                                                       \
        return Value::move<String>(str);                                                                               \
    }                                                                                                                  \
                                                                                                                       \
    Value ValueType::asValue() const { return Value{val_}; }                                                           \
                                                                                                                       \
    ValueType::operator Value() const { return asValue(); }                                                            \
                                                                                                                       \
    bool ValueType::operator==(Value const& other) const {                                                             \
        return JS_IsStrictEqual(Locker::currentContextChecked(), val_, Value::extract(other));                         \
    }


} // namespace qjspp
