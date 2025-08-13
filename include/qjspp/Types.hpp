#pragma once
#include <functional>

#pragma warning(push, 0)
#include <quickjs-libc.h>
#include <quickjs.h>
#pragma warning(pop)


namespace qjspp {


class JsEngine;
class JsException;

enum class ValueType;

class Value; // base
class Undefined;
class Null;
class Boolean;
class Number;
class String;
class Object;
class Array;
class Function;


class Arguments;

struct ESModuleDefine;

struct StaticDefine;
struct InstanceDefine;
class ClassDefine;


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

inline int toQuickJSFlags(PropertyAttributes attr) {
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


} // namespace qjspp

// 位或
inline constexpr qjspp::PropertyAttributes
operator|(qjspp::PropertyAttributes lhs, qjspp::PropertyAttributes rhs) noexcept {
    using UT = std::underlying_type_t<qjspp::PropertyAttributes>;
    return static_cast<qjspp::PropertyAttributes>(static_cast<UT>(lhs) | static_cast<UT>(rhs));
}

// 位与
inline constexpr qjspp::PropertyAttributes
operator&(qjspp::PropertyAttributes lhs, qjspp::PropertyAttributes rhs) noexcept {
    using UT = std::underlying_type_t<qjspp::PropertyAttributes>;
    return static_cast<qjspp::PropertyAttributes>(static_cast<UT>(lhs) & static_cast<UT>(rhs));
}

// 异或
inline constexpr qjspp::PropertyAttributes
operator^(qjspp::PropertyAttributes lhs, qjspp::PropertyAttributes rhs) noexcept {
    using UT = std::underlying_type_t<qjspp::PropertyAttributes>;
    return static_cast<qjspp::PropertyAttributes>(static_cast<UT>(lhs) ^ static_cast<UT>(rhs));
}

// 取反
inline constexpr qjspp::PropertyAttributes operator~(qjspp::PropertyAttributes val) noexcept {
    using UT = std::underlying_type_t<qjspp::PropertyAttributes>;
    return static_cast<qjspp::PropertyAttributes>(~static_cast<UT>(val));
}

// |=
inline constexpr qjspp::PropertyAttributes&
operator|=(qjspp::PropertyAttributes& lhs, qjspp::PropertyAttributes rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

// &=
inline constexpr qjspp::PropertyAttributes&
operator&=(qjspp::PropertyAttributes& lhs, qjspp::PropertyAttributes rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}

// ^=
inline constexpr qjspp::PropertyAttributes&
operator^=(qjspp::PropertyAttributes& lhs, qjspp::PropertyAttributes rhs) noexcept {
    lhs = lhs ^ rhs;
    return lhs;
}