#pragma once
#include "qjspp/Concepts.hpp"
#include "qjspp/Global.hpp"
#include "qjspp/Types.hpp"
#include "quickjs.h"
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace qjspp {


enum class ValueType {
    Undefined,
    Null,
    Boolean,
    Number,
    BigInt,
    String,
    Object,
    Array,
    Function,
};

#define SPECIALIZE_FRIEND()                                                                                            \
private:                                                                                                               \
    friend class Value;                                                                                                \
    friend class Undefined;                                                                                            \
    friend class Null;                                                                                                 \
    friend class Boolean;                                                                                              \
    friend class Number;                                                                                               \
    friend class BigInt;                                                                                               \
    friend class String;                                                                                               \
    friend class Object;                                                                                               \
    friend class Array;                                                                                                \
    friend class Function;                                                                                             \
    friend class Global

#define SPECIALIZE_RAII(TYPE)                                                                                          \
public:                                                                                                                \
    QJSPP_DISABLE_NEW();                                                                                               \
                                                                                                                       \
    ~TYPE();                                                                                                           \
                                                                                                                       \
    Value asValue() const;                                                                                             \
                                                                                                                       \
    void reset();                                                                                                      \
                                                                                                                       \
    String toString() const;                                                                                           \
                                                                                                                       \
    bool operator==(Value const& other) const;                                                                         \
                                                                                                                       \
    operator bool() const;                                                                                             \
                                                                                                                       \
private:                                                                                                               \
    explicit TYPE(::JSValue value);                                                                                    \
    ::JSValue val_ { JS_UNDEFINED }

#define SPECIALIZE_COPY_AND_MOVE(TYPE)                                                                                 \
public:                                                                                                                \
    TYPE(TYPE const& copy);                                                                                            \
    TYPE(TYPE&& move) noexcept;                                                                                        \
    TYPE& operator=(TYPE const& copy);                                                                                 \
    TYPE& operator=(TYPE&& move) noexcept

#define SPECIALIZE_NON_VALUE(TYPE)                                                                                     \
public:                                                                                                                \
    inline operator Value() const { return this->asValue(); }                                                          \
    bool   operator==(TYPE const& other) const { return operator==(other.asValue()); }

// collapsed
#define SPECIALIZE_ALL(TYPE)                                                                                           \
    SPECIALIZE_COPY_AND_MOVE(TYPE);                                                                                    \
    SPECIALIZE_RAII(TYPE);                                                                                             \
    SPECIALIZE_FRIEND()


/**
 * @note JSValue 的封装类
 * @note 注意：每个 Value 都有一个引用计数，Value 内部使用 RAII 管理引用计数
 * @note 注意：Value 析构时栈上需要有活动的 JsScope，否则会抛出 logic_error
 */
class Value final {
    SPECIALIZE_ALL(Value);

public:
    Value(); // undefined

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

    template <IsWrappedType T>
    [[nodiscard]] T as() const;

    /**
     * @note 危险的操作，解包后您需要自行管理引用计数
     */
    template <IsWrappedType T>
    [[nodiscard]] inline static ::JSValue extract(T const& ty) {
        return ty.val_;
    }

    /**
     * @note 包装值，内部会进行增加引用计数
     */
    template <IsWrappedType T>
    [[nodiscard]] inline static T wrap(::JSValue val) {
        return T(val);
    }

    /**
     * @note 移交值，内部不进行增加引用计数
     */
    template <IsWrappedType T>
    [[nodiscard]] inline static T move(JSValue ty) {
        auto undefined = T{JS_UNDEFINED};
        undefined.val_ = std::move(ty);
        return undefined;
    }
};

class Undefined final {
    SPECIALIZE_ALL(Undefined);
    SPECIALIZE_NON_VALUE(Undefined);

public:
    Undefined();
};

class Null final {
    SPECIALIZE_ALL(Null);
    SPECIALIZE_NON_VALUE(Null);

public:
    Null();
};

class Boolean final {
    SPECIALIZE_ALL(Boolean);
    SPECIALIZE_NON_VALUE(Boolean);

public:
    explicit Boolean(bool value);

    [[nodiscard]] bool value() const;
};

class Number final {
    SPECIALIZE_ALL(Number);
    SPECIALIZE_NON_VALUE(Number);

public:
    explicit Number(double d);
    explicit Number(float f);
    explicit Number(int i32);
    explicit Number(int64_t i64);

    [[nodiscard]] float   getFloat() const;
    [[nodiscard]] double  getDouble() const;
    [[nodiscard]] int     getInt32() const;
    [[nodiscard]] int64_t getInt64() const;
};

class BigInt final {
    SPECIALIZE_ALL(BigInt);
    SPECIALIZE_NON_VALUE(BigInt);

public:
    explicit BigInt(int64_t i64);
    explicit BigInt(uint64_t u64);

    [[nodiscard]] int64_t  getInt64() const;
    [[nodiscard]] uint64_t getUInt64() const;
};

class String final {
    SPECIALIZE_ALL(String);
    SPECIALIZE_NON_VALUE(String);

public:
    explicit String(std::string_view utf8);
    explicit String(std::string const& value);
    explicit String(char const* value);

    [[nodiscard]] std::string value() const;
};

class Object final {
    SPECIALIZE_ALL(Object);
    SPECIALIZE_NON_VALUE(Object);

public:
    Object();

    [[nodiscard]] bool has(String const& key) const;
    [[nodiscard]] bool has(std::string const& key) const;

    [[nodiscard]] Value get(String const& key) const;
    [[nodiscard]] Value get(std::string const& key) const;

    void set(String const& key, Value const& value);
    void set(std::string const& key, Value const& value);

    void remove(String const& key);
    void remove(std::string const& key);

    [[nodiscard]] std::vector<String> getOwnPropertyNames() const;

    [[nodiscard]] std::vector<std::string> getOwnPropertyNamesAsString() const;

    [[nodiscard]] bool instanceOf(Value const& value) const;

    bool defineOwnProperty(String const& key, Value const& value, PropertyAttributes attr = PropertyAttributes::None);
    bool
    defineOwnProperty(std::string const& key, Value const& value, PropertyAttributes attr = PropertyAttributes::None);
};

class Array final {
    SPECIALIZE_ALL(Array);
    SPECIALIZE_NON_VALUE(Array);

public:
    explicit Array(size_t size = 0);

    [[nodiscard]] size_t length() const;

    [[nodiscard]] Value get(size_t index) const;

    [[nodiscard]] Value operator[](size_t index) const;

    void set(size_t index, Value const& value);

    void push(Value const& value);

    void clear();
};

class Function final {
    SPECIALIZE_ALL(Function);
    SPECIALIZE_NON_VALUE(Function);

    Value callImpl(Value const& thiz, int argc, Value const* argv) const;

public:
    explicit Function(FunctionCallback callback);

    template <typename T>
        requires(!IsFunctionCallback<T>)
    explicit Function(T&& func);

    template <typename... Fn>
        requires(sizeof...(Fn) > 1 && (!IsFunctionCallback<Fn> && ...))
    explicit Function(Fn&&... func);

    Value call(Value const& thiz, std::vector<Value> const& args) const;

    Value call(Value const& thiz, std::initializer_list<Value> args) const;

    Value call(Value const& thiz, std::span<const Value> args) const;

    Value call() const;

    Value callAsConstructor(std::vector<Value> const& args = {}) const;

    bool isConstructor() const;
};

#undef SPECIALIZE_ALL
#undef SPECIALIZE_COPY_AND_MOVE
#undef SPECIALIZE_RAII
#undef SPECIALIZE_FRIEND
#undef SPECIALIZE_NON_VALUE


/**
 * @note 带作用域的值
 * @note 此类为了解决持有 Value 时析构需要 JsScope 的问题
 * @note 在析构时，会自动在栈上创建 JsScope 保证 Value 安全析构
 */
class ScopedValue final {
    JsEngine* engine_{nullptr};
    Value     val_{};

public:
    QJSPP_DISABLE_NEW();

    ScopedValue() = default;
    ScopedValue(Value value); // need active JsScope
    explicit ScopedValue(JsEngine* engine, Value val);

    ScopedValue(ScopedValue&& other) noexcept;
    ScopedValue& operator=(ScopedValue&& other) noexcept;

    ScopedValue(ScopedValue const& copy);
    ScopedValue& operator=(ScopedValue const& copy);

    ~ScopedValue();

    void reset(); // reset engine and value

    [[nodiscard]] JsEngine* engine() const;

    [[nodiscard]] Value value() const;

    [[nodiscard]] operator Value() const;
};


class Arguments final {
    JsEngine*                 engine_;
    JSValueConst              thiz_;
    int                       length_;
    JSValueConst*             args_;
    struct JsManagedResource* managed_{nullptr};

    friend class JsEngine;
    friend class Function;

    explicit Arguments(JsEngine* engine, JSValueConst thiz, int length, JSValueConst* args);

public:
    QJSPP_DISABLE_COPY_MOVE(Arguments);

    [[nodiscard]] JsEngine* engine() const;

    [[nodiscard]] bool hasThiz() const;

    [[nodiscard]] Object thiz() const; // this

    [[nodiscard]] size_t length() const;

    // Obtain internal resource management wrapper.
    // This function is only used internally.
    // note: This resource is only valid during instance class calls (method, property).
    [[nodiscard]] bool                      hasJsManagedResource() const;
    [[nodiscard]] struct JsManagedResource* getJsManagedResource() const;

    Value operator[](size_t index) const;
};


} // namespace qjspp

#include "Values.inl"
