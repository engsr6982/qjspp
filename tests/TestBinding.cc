#include "TestEngineFixture.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_exception.hpp"
#include "qjspp/Binding.hpp"
#include "qjspp/Definitions.hpp"
#include "qjspp/JsEngine.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/JsScope.hpp"
#include "qjspp/Module.hpp"
#include "qjspp/Types.hpp"
#include "qjspp/Values.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <utility>


struct Util {
    static int         add(int a, int b) { return a + b; }
    static std::string append(std::string const& a, std::string const& b) { return a + b; }
    static std::string append(std::string const& a, std::string const& b, std::string const& c) { return a + b + c; }

    static int           foo;
    static std::string   cus;
    static constexpr int bar = 666;
};
std::string Util::cus = "cus";
int         Util::foo = 42;


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


TEST_CASE_METHOD(TestEngineFixture, "Static Binding") {
    qjspp::JsScope scope{engine_};
    engine_->registerNativeClass(UtilDefine);

    REQUIRE(engine_->eval("Util.add(1, 2)").asNumber().getInt32() == 3);
    REQUIRE(engine_->eval("Util.append('a', 'b')").asString().value() == "ab");
    REQUIRE(engine_->eval("Util.append('a', 'b', 'c')").asString().value() == "abc");

    REQUIRE(engine_->eval("Util.custom()").asString().value() == "custom");


    // properies
    REQUIRE(engine_->eval("Util.foo").asNumber().getInt32() == 42);
    REQUIRE_NOTHROW(engine_->eval("Util.foo = 128"));
    REQUIRE(Util::foo == 128);

    REQUIRE(engine_->eval("Util.bar").asNumber().getInt32() == 666);
    REQUIRE_THROWS_MATCHES(
        engine_->eval("'use strict'; Util.bar = 777"),
        qjspp::JsException,
        Catch::Matchers::Message("no setter for property")
    );

    REQUIRE(engine_->eval("Util.cus").asString().value() == "cus");
    REQUIRE_NOTHROW(engine_->eval("Util.cus = 'new'"));
    REQUIRE(Util::cus == "new");
}


// Instance binding
class Base {
public:
    virtual ~Base() = default;

    int baseMember = 466;

    virtual std::string type() const { return "Base"; }

    int baseBar() { return 0; }

    static bool        baseTrue() { return true; }
    static std::string name;
};
std::string Base::name = "Base";


class Derived : public Base {
public:
    Derived(int mem) : derivedMember(mem) {}

    int derivedMember = 666;

    std::string type() const override { return "Derived"; }

    static std::string foo;
};
std::string Derived::foo = "Derived::foo";


qjspp::ClassDefine const BaseDefine = qjspp::defineClass<Base>("Base")
                                          .disableConstructor()
                                          .instanceProperty("baseMember", &Base::baseMember)
                                          .instanceMethod("type", &Base::type)
                                          .instanceMethod("baseBar", &Base::baseBar)
                                          .property("name", &Base::name)
                                          .function("baseTrue", &Base::baseTrue)
                                          .build();

qjspp::ClassDefine const DerivedDefine = qjspp::defineClass<Derived>("Derived")
                                             .extends(BaseDefine)
                                             .constructor<int>()
                                             .instanceProperty("derivedMember", &Derived::derivedMember)
                                             .instanceMethod("type", &Derived::type)
                                             .property("foo", &Derived::foo)
                                             .build();


TEST_CASE_METHOD(TestEngineFixture, "Instance Binding") {
    qjspp::JsScope scope{engine_};
    engine_->registerNativeClass(BaseDefine);
    engine_->registerNativeClass(DerivedDefine);

    engine_->globalThis().set("debug", qjspp::Function{[](qjspp::Arguments const& args) -> qjspp::Value {
                                  std::ostringstream oss;
                                  for (int i = 0; i < args.length(); ++i) {
                                      oss << args[i].toString().value();
                                      if (i != args.length() - 1) {
                                          oss << ", ";
                                      }
                                  }
                                  std::cout << "[DEBUG] " << oss.str() << std::endl;
                                  return {};
                              }});
    engine_->eval("debug(Base)");
    engine_->eval("debug(Derived)");

    SECTION("JavaScript new") {
        auto der = engine_->eval("new Derived(114514);");
        REQUIRE(der.isObject());

        auto raw = engine_->getNativeInstanceOf<Derived>(der.asObject(), DerivedDefine);
        REQUIRE(raw != nullptr);
        REQUIRE(raw->derivedMember == 114514);


        // 实例继承链
        REQUIRE(engine_->eval("new Derived(1234).baseBar()").asNumber().getInt32() == 0);
        REQUIRE(engine_->eval("new Derived(1234).baseMember").asNumber().getInt32() == 466);
        REQUIRE(engine_->eval("new Derived(1234).type()").asString().value() == "Derived");

        // 静态属性继承链：
        REQUIRE(engine_->eval("Derived.foo").asString().value() == "Derived::foo"); // Derived::foo
        REQUIRE(engine_->eval("Base.name").asString().value() == "Base");           // Base::name
        REQUIRE(engine_->eval("new Derived(789).name").isUndefined() == true); // 按照标准，静态属性不能通过实例访问

        // TODO: 静态链继承
        CHECK(engine_->eval("Derived.baseTrue()").asBoolean().value() == true);
        CHECK_FALSE(engine_->eval("Derived.name").asString().value() == "Base");
    }

    SECTION("C++ new") {
        engine_->globalThis().set("getDerived", qjspp::Function{[](qjspp::Arguments const& args) -> qjspp::Value {
                                      auto engine = args.engine();
                                      auto der    = engine->newInstanceOfRaw(DerivedDefine, new Derived{888});
                                      return der;
                                  }});
        auto der = engine_->eval("getDerived()");
        REQUIRE(der.isObject());
        REQUIRE(engine_->eval("getDerived().derivedMember").asNumber().getInt32() == 888);
    }

    SECTION("JavaSceipt inherit") {
        REQUIRE_NOTHROW(engine_->eval(R"(
            class MyDerived extends Derived {
                constructor() {
                    super(123456);
                }
            };
            const my = new MyDerived();
            debug(`baseMember: ${my.baseMember}`);
            debug(`baseBar: ${my.baseBar()}`);
        )"));
    }
}

qjspp::ModuleDefine NativeModuleDef =
    qjspp::defineModule("native").exportClass(UtilDefine).exportClass(BaseDefine).exportClass(DerivedDefine).build();

TEST_CASE_METHOD(TestEngineFixture, "Module Binding") {
    qjspp::JsScope scope{engine_};

    engine_->registerNativeModule(NativeModuleDef);

    REQUIRE_NOTHROW(
        engine_->eval("import { Base } from 'native'; Base.baseTrue();", "<eval>", qjspp::JsEngine::EvalType::kModule)
    );

    REQUIRE_NOTHROW(
        engine_->eval("import { Util } from 'native'; Util.add(8,8);", "<eval>", qjspp::JsEngine::EvalType::kModule)
    );

    REQUIRE_NOTHROW(engine_->loadScript(std::filesystem::current_path() / "tests" / "module.js"));
}


class TestForm {
public:
    using Callback = std::function<void(int)>;
    Callback cb_;

    TestForm() = default;

    void setCallback(Callback cb) { cb_ = std::move(cb); }

    void call(int val) {
        if (cb_) {
            cb_(val);
        }
    }
};


qjspp::Value jsassert(qjspp::Arguments const& args) {
    REQUIRE(args.length() == 1);
    REQUIRE(args[0].isBoolean());
    REQUIRE(args[0].asBoolean().value() == true);
    return {};
}

qjspp::ClassDefine TestFormDefine = qjspp::defineClass<TestForm>("TestForm")
                                        .constructor<>()
                                        .instanceMethod("setCallback", &TestForm::setCallback)
                                        .instanceMethod("call", &TestForm::call)
                                        .build();

TEST_CASE_METHOD(TestEngineFixture, "Test Callback") {
    qjspp::JsScope scope{engine_};

    engine_->registerNativeClass(TestFormDefine);
    engine_->globalThis().set("assert", qjspp::Function{&jsassert});

    engine_->eval(R"(
        let fm = new TestForm();
        fm.setCallback((val) => {
            assert(val == 114514);
        });
        fm.call(114514);
    )");
}


class AbstractFoo {
public:
    AbstractFoo() = default;

    virtual std::string foo() = 0;
};

qjspp::ClassDefine AbstractFooDef = qjspp::defineClass<AbstractFoo>("AbstractFoo")
                                        .disableConstructor()
                                        .instanceMethod("foo", &AbstractFoo::foo)
                                        .build();

class FooImpl : public AbstractFoo {
public:
    std::string foo() override { return "foo"; }
};

TEST_CASE_METHOD(TestEngineFixture, "Abstract Class") {
    qjspp::JsScope scope{engine_};

    auto impl = std::make_shared<FooImpl>();

    engine_->registerNativeClass(AbstractFooDef);
    engine_->globalThis().set("assert", qjspp::Function{&jsassert});
    engine_->globalThis().set("getAbstractFoo", qjspp::Function{[impl](qjspp::Arguments const& args) -> qjspp::Value {
                                  REQUIRE(args.length() == 0);
                                  auto i = impl;
                                  return args.engine()->newInstanceOfShared(AbstractFooDef, std::move(i));
                              }});

    REQUIRE_NOTHROW(engine_->eval(R"(
        let foo = getAbstractFoo();
        assert(foo.foo() == "foo");
    )"));
}


class Builder {
public:
    std::ostringstream str_;

    Builder() = default;

    Builder& append(std::string const& str) {
        str_ << str;
        return *this;
    }

    std::string build() const { return str_.str(); }
};

qjspp::ClassDefine BuilderDefine = qjspp::defineClass<Builder>("Builder")
                                       .constructor<>()
                                       .instanceMethod("append", &Builder::append)
                                       .instanceMethod("build", &Builder::build)
                                       .build();

TEST_CASE_METHOD(TestEngineFixture, "Builder Pattern") {
    qjspp::JsScope scope{engine_};

    engine_->registerNativeClass(BuilderDefine);
    engine_->globalThis().set("assert", qjspp::Function{&jsassert});

    REQUIRE_NOTHROW(engine_->eval(R"(
        let builder = new Builder();
        let str = builder.append("Hello").append(" World").build();
        assert(str == "Hello World");
    )"));
}


// enum bind

enum class Color {
    Red = 0,
    Green,
    Blue,
};

qjspp::EnumDefine ColorDef_ = qjspp::defineEnum<Color>("Color")
                                  .value("Red", Color::Red)
                                  .value("Green", Color::Green)
                                  .value("Blue", Color::Blue)
                                  .build();

TEST_CASE_METHOD(TestEngineFixture, "Enum Bind") {
    qjspp::JsScope scope{engine_};

    engine_->registerEnum(ColorDef_);

    REQUIRE(engine_->eval("Color.$name").asString().value() == "Color");
    REQUIRE(engine_->eval("Color.Red").asNumber().getInt32() == 0);
    REQUIRE(engine_->eval("Color.Green").asNumber().getInt32() == 1);
    REQUIRE(engine_->eval("Color.Blue").asNumber().getInt32() == 2);
}

qjspp::ModuleDefine ColorModuleDef_ = qjspp::defineModule("Color").exportEnum(ColorDef_).build();

TEST_CASE_METHOD(TestEngineFixture, "Enum Module Bind") {
    qjspp::JsScope scope{engine_};

    engine_->registerNativeModule(ColorModuleDef_);
    engine_->globalThis().set("assert", qjspp::Function{&jsassert});

    REQUIRE_NOTHROW(engine_->eval(
        R"(
            import { Color } from "Color";
            assert(Color.$name == "Color");
            assert(Color.Red == 0);
            assert(Color.Green == 1);
            assert(Color.Blue == 2);
        )",
        "<eval>",
        qjspp::JsEngine::EvalType::kModule
    ));
}
