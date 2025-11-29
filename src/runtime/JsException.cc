#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/Object.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"
#include "quickjs.h"

#include <exception>


namespace qjspp {

struct JsException::ExceptionContext {
    Type                type_{Type::Any};
    mutable std::string message_{};
    Value               exception_{};
};

JsException::JsException(std::string message, Type type) : qjspp::JsException{type, std::move(message)} {}
JsException::JsException(Type type, std::string message)
: std::exception(),
  data_(std::make_shared<ExceptionContext>()) {
    data_->type_    = type;
    data_->message_ = std::move(message);
}

JsException::JsException(Value exception) : std::exception(), data_(std::make_shared<ExceptionContext>()) {
    data_->type_      = Type::Any;
    data_->exception_ = std::move(exception);
}

JsException::Type JsException::type() const noexcept { return data_->type_; }

char const* JsException::what() const noexcept {
    extractMessage();
    return data_->message_.c_str();
}

std::string JsException::message() const noexcept {
    extractMessage();
    return data_->message_;
}

Value JsException::exception() const noexcept {
    if (data_->exception_.isUndefined()) {
        auto ctx = Locker::currentContextChecked();
        switch (data_->type_) {
        case Type::RangeError:
            JS_ThrowRangeError(ctx, "%s", data_->message_.c_str());
            break;
        case Type::ReferenceError:
            JS_ThrowReferenceError(ctx, "%s", data_->message_.c_str());
            break;
        case Type::SyntaxError:
            JS_ThrowSyntaxError(ctx, "%s", data_->message_.c_str());
            break;
        case Type::TypeError:
            JS_ThrowTypeError(ctx, "%s", data_->message_.c_str());
            break;
        case Type::InternalError:
            JS_ThrowInternalError(ctx, "%s", data_->message_.c_str());
            break;
        case Type::Any:
        default:
            JS_Throw(ctx, Value::extract(String(data_->message_)));
        }
        data_->exception_ = Value::move<Value>(JS_GetException(ctx));
    }
    return data_->exception_;
}

std::string JsException::stacktrace() const noexcept {
    try {
        return data_->exception_.asObject().get("stack").asString().value();
    } catch (JsException const&) {
        return "[ERROR: failed to obtain stacktrace]";
    }
}

JSValue JsException::rethrowToEngine() const {
    JS_Throw(
        Locker::currentContextChecked(),
        JS_DupValue(Locker::currentContextChecked(), Value::extract(exception()))
    );
    return JS_EXCEPTION;
}

void JsException::extractMessage() const noexcept {
    if (!data_->message_.empty()) {
        return;
    }
    try {
        if (data_->exception_.isString()) {
            data_->message_ = data_->exception_.asString().value();
            return; // 非标准异常
        }
        auto obj        = data_->exception_.asObject();
        data_->message_ = obj.get("message").asString().value();
    } catch (JsException const&) {
        data_->message_ = "[ERROR: failed to obtain message]";
    }
}


// helpers
void JsException::check(JSValue value) {
    if (JS_IsException(value)) {
        check(-1);
    }
}

void JsException::check(int code, const char* msg) {
    if (code < 0) {
        auto ctx   = Locker::currentContextChecked();
        auto error = JS_GetException(ctx);

        if (JS_IsObject(error)) {
            throw JsException(Value::move<Value>(error));
        } else {
            JS_FreeValue(ctx, error);
            throw JsException(msg);
        }
    }
}


} // namespace qjspp
