# qjspp - Modern C++ Wrapper for QuickJS / QuickJS-ng

- [中文](./README.md) | [English](./README_EN.md)

`qjspp` is a modern C++ wrapper library for QuickJS / QuickJS-ng.  
It supports class binding, module registration, callback binding, and more — making **C++ ↔ JS interop** natural and
clean.

Features:

- **Non-intrusive bindings**: only need to register bindings, no modification to existing C++ code.
- **Strong type support**: supports standard C++ types (`std::string`, `int`, etc.), enums, and reference semantics for
  properties.
- **Flexible binding modes**: static binding, instance binding, function binding, callback binding, module registration,
  Builder pattern, etc.
- **Exception forwarding**: full two-way exception model between C++ and JS.

---

## ✨ Features

- ✅ Function binding
- ✅ Static binding
- ✅ Instance binding
- ✅ Enum binding
- ✅ Module registration
- ✅ Callback binding (JS → C++)
- ✅ Builder pattern support
- ✅ Reference semantics for instance properties
- ✅ Constructor overloading
- ✅ Arbitrary function overload binding
- ✅ Two-way exception model
- ✅ Type conversion bridge

---

## ⚙️ Behavior Control Macros

### `QJSPP_SKIP_INSTANCE_CALL_CHECK_CLASS_DEFINE`

- **Default: Off**
- Controls whether instance method/property calls check if `JsManagedResource::define_` matches the registered
  `ClassDefine`.
- When enabled, skips validation. When disabled, mismatch throws `JsException`.

### `QJSPP_CALLBACK_ALWAYS_THROW_IF_NEED_RETURN_VALUE`

- **Default: Off**
- When invoking bound callbacks, if JS throws and a return value is required, a `std::runtime_error` is thrown.
- By default, it tries to use the default constructor of the return type if possible.

### `QJSPP_INT64_OR_UINT64_ALWAYS_USE_NUMBER_OF_BIGINT_IN_TYPE_CONVERTER`

- **Default: Off**
- If enabled, `int64_t` / `uint64_t` conversions use JS `Number` instead of `BigInt`.

### `QJSPP_DONT_GENERATE_HELPER_EQLAUS_METHDO`

- **Default: Off**
- Automatically generates a `$equals` helper method for comparing instance pointers or `operator==`.  
  Disable to skip generation.

---

## 🧩 Native Binding Examples

All examples below are extracted from unit tests and cover major features.

### Static Binding

```cpp
struct Util {
    static int add(int a, int b) { return a + b; }
    static std::string append(std::string const& a, std::string const& b) { return a + b; }
    static std::string append(std::string const& a, std::string const& b, std::string const& c) { return a + b + c; }

    static int foo;
    static std::string cus;
    static constexpr int bar = 666;
};
std::string Util::cus = "cus";
int Util::foo = 42;

qjspp::ClassDefine const UtilDefine =
    qjspp::defineClass<void>("Util")
        .function("add", &Util::add)
        .function(
            "append",
            static_cast<std::string (*)(std::string const&, std::string const&)>(Util::append),
            static_cast<std::string (*)(std::string const&, std::string const&, std::string const&)>(Util::append)
        )
        .function("custom", [](qjspp::Arguments const&) -> qjspp::Value { return qjspp::String{"custom"}; })
        .property("foo", &Util::foo)
        .property("bar", &Util::bar)
        .property(
            "cus",
            []() -> qjspp::Value { return qjspp::String{Util::cus}; },
            [](qjspp::Value const& value) { Util::cus = value.asString().value(); }
        )
        .build();
```

### Instance Binding & Inheritance

```cpp
class Base { /* ... */ };
class Derived : public Base { /* ... */ };

qjspp::ClassDefine const BaseDefine = qjspp::defineClass<Base>("Base") /* ... */;
qjspp::ClassDefine const DerivedDefine = qjspp::defineClass<Derived>("Derived").extends(BaseDefine) /* ... */;

// JS usage:
engine_->eval("let obj = new Derived(123); obj.baseBar(); obj.type();");
engine_->eval("Derived.foo"); // static property
```

- Instance inheritance supports parent instance methods and properties.
- Static properties must be accessed via the class, not instance (as per JS standard).
- `$equals` helper auto-generated (can be disabled).

### Module Registration

```cpp
qjspp::ModuleDefine NativeModuleDef =
    qjspp::defineModule("native").addClass(UtilDefine).addClass(BaseDefine).addClass(DerivedDefine).build();
engine_->registerModule(NativeModuleDef);

// JS usage
engine_->eval("import { Base, Util } from 'native'; Base.baseTrue(); Util.add(1,2);");
```

### Function Binding

```cpp
int add(int a, int b) { return a + b; }
std::string append(std::string const& a, int b) { return a + std::to_string(b); }

engine_->globalThis().set("add", qjspp::Function{&add});
engine_->globalThis().set("append", qjspp::Function{static_cast<std::string(*)(std::string const&, int)>(&append)});
```

### Callback Binding (JS → C++)

```cpp
class TestForm {
public:
    using Callback = std::function<void(int)>;
    void setCallback(Callback cb) { cb_ = std::move(cb); }
    void call(int val) { if (cb_) cb_(val); }
private:
    Callback cb_;
};

qjspp::ClassDefine TestFormDefine = qjspp::defineClass<TestForm>("TestForm")
                                        .constructor<>()
                                        .instanceMethod("setCallback", &TestForm::setCallback)
                                        .instanceMethod("call", &TestForm::call)
                                        .build();
```

JS usage:

```js
let fm = new TestForm();
fm.setCallback(val => console.log(val));
fm.call(42);
```

### Builder Pattern

```cpp
class Builder { /* ... */ };
qjspp::ClassDefine BuilderDefine = qjspp::defineClass<Builder>("Builder") /* ... */;
```

JS usage:

```js
let builder = new Builder();
let str = builder.append("Hello").append(" World").build(); // "Hello World"
```

### Enum Binding

```cpp
enum class Color { Red, Green, Blue };
qjspp::EnumDefine ColorDef_ = qjspp::defineEnum<Color>("Color")
                                  .value("Red", Color::Red)
                                  .value("Green", Color::Green)
                                  .value("Blue", Color::Blue)
                                  .build();
```

JS usage:

```js
Color.Red   // 0
Color.Green // 1
Color.Blue  // 2
```

## Exception Forwarding

- `JsException` thrown in C++ can be caught in JS.
- JS exceptions can be caught in C++ as `JsException`.

```cpp
auto nativeThrow = qjspp::Function{[](qjspp::Arguments const&) { throw qjspp::JsException{"native throw"}; }};
engine_->globalThis().set("nativeThrow", nativeThrow); 
```

