#include "TestEngineFixture.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_exception.hpp"
#include "qjspp/Binding.hpp"
#include "qjspp/JsEngine.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/JsScope.hpp"
#include "qjspp/Types.hpp"
#include "qjspp/Values.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>


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
        CHECK(engine_->eval("Derived.name").asString().value() == "Base");
    }
}