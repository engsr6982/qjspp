#pragma once
#include "ValueBase.hpp"
#include "ValueType.hpp"
#include "qjspp/Forward.hpp"
#include "qjspp/Global.hpp"

#include "qjspp/concepts/ScriptConcepts.hpp"

namespace qjspp {


/**
 * @note JSValue 的封装类
 * @note 注意：每个 Value 都有一个引用计数，Value 内部使用 RAII 管理引用计数
 * @note 注意：Value 析构时栈上需要有活动的 Locker，否则会抛出 logic_error
 */
class Value final {
public:
    QJSPP_DEFINE_VALUE_COMMON(Value);
    Value();

    [[nodiscard]] ValueType type() const;

    [[nodiscard]] bool isUninitialized() const;
    [[nodiscard]] bool isUndefined() const;
    [[nodiscard]] bool isNull() const;
    [[nodiscard]] bool isBoolean() const;
    [[nodiscard]] bool isNumber() const;
    [[nodiscard]] bool isBigInt() const;
    [[nodiscard]] bool isString() const;
    [[nodiscard]] bool isObject() const;
    [[nodiscard]] bool isArray() const;
    [[nodiscard]] bool isFunction() const;

    [[nodiscard]] Undefined asUndefined() const;
    [[nodiscard]] Null      asNull() const;
    [[nodiscard]] Boolean   asBoolean() const;
    [[nodiscard]] Number    asNumber() const;
    [[nodiscard]] BigInt    asBigInt() const;
    [[nodiscard]] String    asString() const;
    [[nodiscard]] Object    asObject() const;
    [[nodiscard]] Array     asArray() const;
    [[nodiscard]] Function  asFunction() const;

    /**
     * @note 危险的操作，解包后您需要自行管理引用计数
     */
    template <concepts::JsValueType Ty>
    [[nodiscard]] inline static JSValue extract(Ty const& ty) {
        return ty.val_;
    }

    /**
     * @note 包装值，内部会进行增加引用计数
     */
    template <concepts::JsValueType Ty>
    [[nodiscard]] inline static Ty wrap(::JSValue val) {
        return Ty(val);
    }

    /**
     * @note 移交值，内部不进行增加引用计数
     */
    template <concepts::JsValueType Ty>
    [[nodiscard]] inline static Ty move(JSValue ty) {
        auto undefined = Ty{JS_UNDEFINED};
        undefined.val_ = std::move(ty);
        return undefined;
    }
};


} // namespace qjspp

