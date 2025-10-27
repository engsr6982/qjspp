#include "TestEngineFixture.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_exception.hpp"
#include "qjspp/JsEngine.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/JsScope.hpp"
#include "qjspp/Types.hpp"
#include "qjspp/Values.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>


qjspp::Value sub(qjspp::Arguments const& args) {
    REQUIRE(args.length() == 2);
    REQUIRE(args[0].isNumber());
    REQUIRE(args[1].isNumber());
    return qjspp::Number{args[0].asNumber().getDouble() - args[1].asNumber().getDouble()};
}
int         add(int a, int b) { return a + b; }
std::string append(std::string const& a, std::string const& b) { return a + b; }
std::string append(std::string const& a, int b) { return a + std::to_string(b); }


TEST_CASE_METHOD(TestEngineFixture, "Values") {
    qjspp::JsScope scope(engine_);

    SECTION("Value::is*") {
        auto empty = qjspp::Value{};
        REQUIRE(empty.isUndefined() == true);

        auto undefined = qjspp::Undefined{};
        REQUIRE(undefined.asValue().isUndefined() == true);

        auto null = qjspp::Null{};
        REQUIRE(null.asValue().isNull() == true);

        auto boolean = qjspp::Boolean{true};
        REQUIRE(boolean.asValue().isBoolean() == true);
        REQUIRE(boolean.value() == true);

        auto number = qjspp::Number{42.0};
        REQUIRE(number.asValue().isNumber() == true);
        REQUIRE(number.getDouble() == 42.0);

        auto string = qjspp::String{"Hello"};
        REQUIRE(string.asValue().isString() == true);
        REQUIRE(string.value() == "Hello");

        auto object = qjspp::Object{};
        REQUIRE(object.asValue().isObject() == true);

        auto array = qjspp::Array{};
        REQUIRE(array.asValue().isArray() == true);

        auto function = qjspp::Function{[](qjspp::Arguments const&) { return qjspp::Boolean{true}; }};
        REQUIRE(function.asValue().isFunction() == true);
        REQUIRE(function.call().asBoolean().value() == true);
    }

    SECTION("Value::as*") {
        auto empty = qjspp::Value{};
        REQUIRE_THROWS_MATCHES(
            empty.asBoolean(),
            qjspp::JsException,
            Catch::Matchers::ExceptionMessageMatcher("can't convert to Boolean")
        );

        REQUIRE_THROWS_MATCHES(
            empty.asNumber(),
            qjspp::JsException,
            Catch::Matchers::ExceptionMessageMatcher("can't convert to Number")
        );

        auto i64 = qjspp::BigInt{int64_t{456}};
        REQUIRE(i64.getInt64() == 456);
    }

    SECTION("Test Object") {
        auto object = qjspp::Object{};
        REQUIRE(object.has("foo") == false);

        object.set("foo", qjspp::Number{42.0});
        REQUIRE(object.has("foo") == true);
        REQUIRE(object.get("foo").asNumber().getDouble() == 42.0);

        object.set("bar", qjspp::String{"Hello"});
        REQUIRE(object.get("bar").asString().value() == "Hello");

        auto keys = object.getOwnPropertyNames();
        REQUIRE(keys.size() == 2);

        auto keyStr = object.getOwnPropertyNamesAsString();
        REQUIRE(keyStr.size() == 2);
        REQUIRE(std::find(keyStr.begin(), keyStr.end(), "foo") != keyStr.end());
        REQUIRE(std::find(keyStr.begin(), keyStr.end(), "bar") != keyStr.end());

        object.remove("foo");
        object.remove("bar");
        REQUIRE(object.getOwnPropertyNames().empty() == true);

        REQUIRE_THROWS_MATCHES(
            object.instanceOf(qjspp::Object{}), // fake object
            qjspp::JsException,
            Catch::Matchers::ExceptionMessageMatcher("invalid 'instanceof' right operand")
        );


        // Test Object::defineOwnProperty
        engine_->globalThis().defineOwnProperty(
            "aaa",
            qjspp::Number{123},
            qjspp::PropertyAttributes::DontDelete | qjspp::PropertyAttributes::ReadOnly
        );
        REQUIRE(engine_->globalThis().get("aaa").asNumber().getInt32() == 123);

        // 非严格模式，静默失败
        engine_->eval("globalThis.aaa = 321;");
        REQUIRE(engine_->globalThis().get("aaa").asNumber().getInt32() == 123);
        REQUIRE(engine_->globalThis().get("aaa").asNumber().getInt32() != 321);

        engine_->eval("delete globalThis.aaa;");
        REQUIRE(engine_->globalThis().has("aaa"));

        // 严格模式，抛出异常
        REQUIRE_THROWS_MATCHES(
            engine_->eval(R"(
                "use strict";
                globalThis.aaa = 321;
            )"),
            qjspp::JsException,
            Catch::Matchers::ExceptionMessageMatcher("'aaa' is read-only")
        );
        REQUIRE_THROWS_MATCHES(
            engine_->eval(R"(
                "use strict";
                delete globalThis.aaa;
            )"),
            qjspp::JsException,
            Catch::Matchers::ExceptionMessageMatcher("could not delete property")
        );
    }

    SECTION("Test Array") {
        auto array = qjspp::Array{};
        REQUIRE(array.length() == 0);

        array.push(qjspp::Number{888});
        REQUIRE(array.length() == 1);
        REQUIRE(array[0].asNumber().getInt32() == 888);

        array.clear();
        REQUIRE(array.length() == 0);
    }

    SECTION("Test Function") {
        auto sub    = qjspp::Function{&::sub};
        auto add    = qjspp::Function{&::add};
        auto append = qjspp::Function{
            static_cast<std::string (*)(std::string const&, std::string const&)>(&::append),
            static_cast<std::string (*)(std::string const&, int)>(&::append)
        };
        engine_->globalThis().set("sub", sub);
        engine_->globalThis().set("add", add);
        engine_->globalThis().set("append", append);

        REQUIRE(engine_->eval("sub(1, 2)").asNumber().getInt32() == -1);
        REQUIRE(engine_->eval("add(1, 2)").asNumber().getInt32() == 3);
        REQUIRE(engine_->eval("append('hello', 'world')").asString().value() == "helloworld");
        REQUIRE(engine_->eval("append('hello', 123)").asString().value() == "hello123");


        // 异常传递
        auto nativeThrow = qjspp::Function{[](qjspp::Arguments const& /* args */) -> qjspp::Value {
            throw qjspp::JsException{"native throw"};
        }};
        engine_->globalThis().set("nativeThrow", nativeThrow);
        REQUIRE_THROWS_MATCHES(
            engine_->eval(R"(
                try {
                    nativeThrow();
                } catch (e) {
                    if (e.message == "native throw") {
                        throw new Error("js throw");
                    }
                }
            )"),
            qjspp::JsException,
            Catch::Matchers::ExceptionMessageMatcher("js throw")
        );
    }

    SECTION("Test ConstructorFunction") {
        engine_->globalThis().set(
            "reg", //
            qjspp::Function{[](qjspp::Arguments const& arguments) -> qjspp::Value {
                REQUIRE(arguments.length() == 1);
                REQUIRE(arguments[0].isFunction());

                auto fn = arguments[0].asFunction();
                REQUIRE(fn.isConstructor());

                auto res = fn.callAsConstructor();
                REQUIRE(res.isObject());

                auto obj = res.asObject();
                REQUIRE(obj.has("bar"));

                REQUIRE(obj.get("bar").asFunction().call(obj, {}).asString().value() == "bar!");
                return res;
            }}
        );
        engine_->eval(R"(
            class Foo {
                bar() {
                    return "bar!";
                }
            }
            reg(Foo);
        )");
    }
}
