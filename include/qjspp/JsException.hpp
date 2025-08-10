#pragma once
#include "qjspp/Values.hpp"
#include <exception>
#include <memory>
#include <string>


namespace qjspp {


class JsException final : public std::exception {
    explicit JsException(Value exception);

public:
    enum class Type { Error, RangeError, ReferenceError, SyntaxError, TypeError };

    explicit JsException(std::string message, Type type = Type::Error);
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
    void makeException() const;

    struct ExceptionContext {
        Type                type_{Type::Error};
        mutable std::string message_{};
        Value               exception_{};
    };

    std::shared_ptr<ExceptionContext> data_{nullptr};
};


} // namespace qjspp