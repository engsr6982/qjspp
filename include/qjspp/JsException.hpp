#pragma once
#include "qjspp/Values.hpp"
#include <exception>
#include <memory>
#include <string>


namespace qjspp {


/**
 * @note 由于 QuickJs C API 限制，此异常可能和标准 JavaScript 异常行为不同
 *
 * @note 对于 C++ 抛出的 Any 类型异常，在 JavaScript 侧是非标准异常 (无 .message 属性)
 * @note 对于 JavaScript 侧抛出的异常，由于没有获取类型 API，因此 type-_ == Type::Any
 */
class JsException final : public std::exception {
    explicit JsException(Value exception);

public:
    enum class Type { Any, RangeError, ReferenceError, SyntaxError, TypeError };

    explicit JsException(std::string message, Type type = Type::ReferenceError);
    explicit JsException(Type type, std::string message);

    // The C++ standard requires exception classes to be reproducible
    JsException(JsException const&)                = default;
    JsException& operator=(JsException const&)     = default;
    JsException(JsException&&) noexcept            = default;
    JsException& operator=(JsException&&) noexcept = default;

    [[nodiscard]] Type type() const noexcept;

    [[nodiscard]] char const* what() const noexcept override;

    [[nodiscard]] std::string message() const noexcept;

    [[nodiscard]] std::string stacktrace() const noexcept;

    [[nodiscard]] JSValue rethrowToEngine() const;

public:
    static void check(::JSValue val);
    static void check(int code, const char* msg = "Unknown error");
    static void check(JSContext* ctx);

private:
    void extractMessage() const noexcept;

    struct ExceptionContext {
        Type                type_{Type::Any};
        mutable std::string message_{};
        Value               exception_{};
    };

    std::shared_ptr<ExceptionContext> data_{nullptr};
};


} // namespace qjspp