#pragma once
#include <functional>

#pragma warning(push, 0)
#include <quickjs-libc.h>
#include <quickjs.h>
#pragma warning(pop)


namespace qjspp {


class JsEngine;

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
    int flags = 0;
    if (!(static_cast<uint32_t>(attr) & static_cast<uint32_t>(PropertyAttributes::DontDelete))) {
        flags |= JS_PROP_CONFIGURABLE; // 没有禁止删除 => 允许配置
    }
    if (!(static_cast<uint32_t>(attr) & static_cast<uint32_t>(PropertyAttributes::ReadOnly))) {
        flags |= JS_PROP_WRITABLE; // 没有只读 => 允许写
    }
    if (!(static_cast<uint32_t>(attr) & static_cast<uint32_t>(PropertyAttributes::DontEnum))) {
        flags |= JS_PROP_ENUMERABLE; // 没有禁止枚举 => 允许枚举
    }
    return flags;
}


} // namespace qjspp