#pragma once
#include <functional>

#pragma warning(push, 0)
#include <quickjs.h>
#pragma warning(pop)


namespace qjspp {

class Value; // base
class Undefined;
class Null;
class Boolean;
class Number;
class BigInt;
class String;
class Object;
class Array;
class Function;

class Arguments;

using FunctionCallback = std::function<Value(Arguments const&)>;
using GetterCallback   = std::function<Value()>;
using SetterCallback   = std::function<void(Value const&)>;

using InstanceConstructor    = std::function<void*(Arguments const& args)>;
using InstanceMethodCallback = std::function<Value(void*, Arguments const& args)>;
using InstanceGetterCallback = std::function<Value(void*, Arguments const& args)>;
using InstanceSetterCallback = std::function<void(void*, Arguments const& args)>;

enum class PropertyAttributes : uint32_t {
    None       = 0,
    DontDelete = 1 << 0, // 禁止删除 (无 JS_PROP_CONFIGURABLE)
    ReadOnly   = 1 << 1, // 不可写（Writable 的反义）
    DontEnum   = 1 << 2, // 不可枚举（Enumerable 的反义）
};

inline constexpr int toQuickJSFlags(PropertyAttributes attr) {
    int flags = JS_PROP_C_W_E; // 默认可配置、可写、可枚举

    if (static_cast<uint32_t>(attr) & static_cast<uint32_t>(PropertyAttributes::DontDelete)) {
        flags &= ~JS_PROP_CONFIGURABLE; // 禁止删除
    }
    if (static_cast<uint32_t>(attr) & static_cast<uint32_t>(PropertyAttributes::ReadOnly)) {
        flags &= ~JS_PROP_WRITABLE; // 只读
    }
    if (static_cast<uint32_t>(attr) & static_cast<uint32_t>(PropertyAttributes::DontEnum)) {
        flags &= ~JS_PROP_ENUMERABLE; // 不可枚举
    }

    return flags;
}

enum class ExceptionDispatchOrigin {
    Unknown = 0, // 未知来源
    Callback,    // C++ 调用 JS 回调
    Script,      // JS 脚本执行
    Constructor, // JS 构造函数
    Method,      // JS 调用绑定方法
    Getter,      // JS 访问属性 getter
    Setter,      // JS 修改属性 setter
    Finalizer    // 托管对象析构 / finalizer
};

} // namespace qjspp

inline constexpr qjspp::PropertyAttributes
operator|(qjspp::PropertyAttributes lhs, qjspp::PropertyAttributes rhs) noexcept {
    using UT = std::underlying_type_t<qjspp::PropertyAttributes>;
    return static_cast<qjspp::PropertyAttributes>(static_cast<UT>(lhs) | static_cast<UT>(rhs));
}
inline constexpr qjspp::PropertyAttributes
operator&(qjspp::PropertyAttributes lhs, qjspp::PropertyAttributes rhs) noexcept {
    using UT = std::underlying_type_t<qjspp::PropertyAttributes>;
    return static_cast<qjspp::PropertyAttributes>(static_cast<UT>(lhs) & static_cast<UT>(rhs));
}
inline constexpr qjspp::PropertyAttributes
operator^(qjspp::PropertyAttributes lhs, qjspp::PropertyAttributes rhs) noexcept {
    using UT = std::underlying_type_t<qjspp::PropertyAttributes>;
    return static_cast<qjspp::PropertyAttributes>(static_cast<UT>(lhs) ^ static_cast<UT>(rhs));
}
inline constexpr qjspp::PropertyAttributes operator~(qjspp::PropertyAttributes val) noexcept {
    using UT = std::underlying_type_t<qjspp::PropertyAttributes>;
    return static_cast<qjspp::PropertyAttributes>(~static_cast<UT>(val));
}
inline constexpr qjspp::PropertyAttributes&
operator|=(qjspp::PropertyAttributes& lhs, qjspp::PropertyAttributes rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}
inline constexpr qjspp::PropertyAttributes&
operator&=(qjspp::PropertyAttributes& lhs, qjspp::PropertyAttributes rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}
inline constexpr qjspp::PropertyAttributes&
operator^=(qjspp::PropertyAttributes& lhs, qjspp::PropertyAttributes rhs) noexcept {
    lhs = lhs ^ rhs;
    return lhs;
}