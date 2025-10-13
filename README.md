# qjspp - QuickJS/QuickJS-ng 的现代 C++包装

`qjspp` 是一个现代 C++ 封装库，用于 QuickJS/QuickJS-ng，支持类绑定、模块注册、回调绑定等功能，使 C++ 与 JS 双向交互更自然。

特点：

- **无侵入性绑定**：只需注册绑定即可，无需改动现有 C++ 代码。
- **良好的类型支持**：支持标准 C++ 类型（`std::string`、`int` 等）、枚举、引用语义属性。
- **灵活的绑定模式**：支持静态绑定、实例绑定、函数绑定、回调绑定、模块注册、Builder 模式等。
- **异常传递**：C++ ↔ JS 双向异常模型。

---

## 功能列表

- [x] 函数绑定
- [x] 静态绑定
- [x] 实例绑定
- [x] 枚举绑定
- [x] 模块注册
- [x] 回调绑定 (JS → C++)
- [x] Builder 模式支持
- [x] 实例类属性对象引用语义
- [x] 构造函数重载
- [x] 任意函数绑定重载
- [x] 双向异常模型
- [x] 类型转换桥

---

## 行为控制宏

### `QJSPP_SKIP_INSTANCE_CALL_CHECK_CLASS_DEFINE`

- **默认：关闭**
- 控制实例方法或属性调用时是否检查 `JsManagedResource::define_` 是否与注册时的 `ClassDefine` 匹配。
- 打开后可跳过检查，关闭时不匹配会抛出 `JsException`。

### `QJSPP_CALLBACK_ALWAYS_THROW_IF_NEED_RETURN_VALUE`

- **默认：关闭**
- 调用绑定回调时，如果 JS 抛异常且需要返回值，则直接抛 `std::runtime_error`。
- 默认行为是尝试用返回值类型的默认构造填充，如果不可行则抛 `std::runtime_error`。

### `QJSPP_INT64_OR_UINT64_ALWAYS_USE_NUMBER_OF_BIGINT_IN_TYPE_CONVERTER`

- **默认：关闭**
- 启用后 `int64_t` / `uint64_t` 类型在转换时使用 `Number` 而不是 `BigInt`。

### `QJSPP_DONT_PATCH_CLASS_TO_STRING_TAG`

- **默认：关闭**
- 默认情况下，会修改注册类、静态类、枚举的 `Symbol.toStringTag` 为类名，便于调试。启用后不修改。

### `QJSPP_DONT_GENERATE_HELPER_EQLAUS_METHDO`

- **默认：关闭**
- 默认会为所有实例类生成 `$equals` 方法，比较指针或 `operator==`。启用后不生成。

---

## 原生绑定示例

以下示例摘自单元测试，覆盖主要功能。

### 静态绑定

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

### 实例绑定 & 继承

```cpp
class Base { /* ... */ };
class Derived : public Base { /* ... */ };

qjspp::ClassDefine const BaseDefine = qjspp::defineClass<Base>("Base") /* ... */;
qjspp::ClassDefine const DerivedDefine = qjspp::defineClass<Derived>("Derived").extends(BaseDefine) /* ... */;

// JS 调用：
engine_->eval("let obj = new Derived(123); obj.baseBar(); obj.type();");
engine_->eval("Derived.foo"); // 静态属性
```

- 支持实例继承链访问父类实例方法/属性。
- 静态属性需通过类访问，按标准不会通过实例访问。
- 自动生成 $equals 比较方法（可关闭）。

### 模块绑定

```cpp
qjspp::ModuleDefine NativeModuleDef =
    qjspp::defineModule("native").addClass(UtilDefine).addClass(BaseDefine).addClass(DerivedDefine).build();
engine_->registerModule(NativeModuleDef);

// JS 使用
engine_->eval("import { Base, Util } from 'native'; Base.baseTrue(); Util.add(1,2);");
```

### 函数绑定

```cpp
int add(int a, int b) { return a + b; }
std::string append(std::string const& a, int b) { return a + std::to_string(b); }

engine_->globalThis().set("add", qjspp::Function{&add});
engine_->globalThis().set("append", qjspp::Function{static_cast<std::string(*)(std::string const&, int)>(&append)});
```

### 回调绑定 (JS → C++)

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

JS 调用：

```js
let fm = new TestForm();
fm.setCallback(val => console.log(val));
fm.call(42);
```

### Builder 模式

```cpp
class Builder { /* ... */ };
qjspp::ClassDefine BuilderDefine = qjspp::defineClass<Builder>("Builder") /* ... */;
```

JS 调用：

```js
let builder = new Builder();
let str = builder.append("Hello").append(" World").build(); // "Hello World"
```

### 枚举绑定

```cpp
enum class Color { Red, Green, Blue };
qjspp::EnumDefine ColorDef_ = qjspp::defineEnum<Color>("Color")
                                  .value("Red", Color::Red)
                                  .value("Green", Color::Green)
                                  .value("Blue", Color::Blue)
                                  .build();
```

JS 调用：

```js
Color.Red   // 0
Color.Green // 1
Color.Blue  // 2
```

## 异常传递

- C++ 抛出的 `JsException` 可被 JS 捕获。
- JS 抛出的异常可在 C++ 捕获为 `JsException`。

```cpp
auto nativeThrow = qjspp::Function{[](qjspp::Arguments const&) { throw qjspp::JsException{"native throw"}; }};
engine_->globalThis().set("nativeThrow", nativeThrow); 
```

