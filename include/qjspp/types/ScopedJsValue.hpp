#pragma once
#include "Value.hpp"

namespace qjspp {

class JsEngine;

/**
 * @class ScopedJsValue
 * @brief 作用域安全的 JS 值封装类。
 *
 * @details
 * 该类用于在 C++ 侧安全持有一个 JS 值。
 * 析构时会自动创建一个 Locker，以保证 JS 引擎上下文安全。
 * 常用于延迟释放 Value 或在非 JS 调用栈中保持 JS 引用。
 *
 * @note 适合在非 JS 线程或跨作用域时安全持有 Value。
 */
class ScopedJsValue final {
    JsEngine* engine_{nullptr};
    Value     val_{};

public:
    QJSPP_DISABLE_NEW();

    ScopedJsValue() = default;
    ScopedJsValue(Value value); // need active Locker
    explicit ScopedJsValue(JsEngine* engine, Value val);

    ScopedJsValue(ScopedJsValue&& other) noexcept;
    ScopedJsValue& operator=(ScopedJsValue&& other) noexcept;

    ScopedJsValue(ScopedJsValue const& copy);
    ScopedJsValue& operator=(ScopedJsValue const& copy);

    ~ScopedJsValue();

    void reset(); // reset engine and value

    [[nodiscard]] JsEngine* engine() const;

    [[nodiscard]] Value value() const;

    [[nodiscard]] operator Value() const;
};


} // namespace qjspp