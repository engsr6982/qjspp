#include "TestEngine.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_exception.hpp"
#include "qjspp/JsEngine.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/JsScope.hpp"
#include "qjspp/Values.hpp"
#include <filesystem>
#include <iostream>
#include <thread>


TEST_CASE_METHOD(TestEngine, "Test JsEngine") {
    qjspp::JsScope scope(engine_);

    SECTION("Test JsEngine::eval") {
        auto val = engine_->eval("1+1");
        REQUIRE(val.isNumber());
        REQUIRE(val.asNumber().getInt32() == 2);

        // Test exception
        REQUIRE_THROWS_MATCHES(
            engine_->eval("null.foo()"),
            qjspp::JsException,
            Catch::Matchers::ExceptionMessageMatcher("cannot read property 'foo' of null")
        );

        try {
            engine_->eval(R"(
                function foo() {
                    throw new Error("foo error");
                }
                foo();
            )");
        } catch (const qjspp::JsException& e) {
            // std::cout << e.message() << std::endl << e.stacktrace() << std::endl;
            REQUIRE(e.message() == "foo error");
        }
    }

    SECTION("Test JsEngine::loadScript") {
        auto test = qjspp::Function{[](qjspp::Arguments const& args) -> qjspp::Value {
            REQUIRE(args.length() == 1);
            REQUIRE(args[0].isNumber());
            REQUIRE(args[0].asNumber().getInt32() == 1);
            return {};
        }};
        engine_->globalThis().set(qjspp::String{"foo"}, test);

        REQUIRE_NOTHROW(engine_->loadScript(std::filesystem::current_path() / "tests" / "test.js"));
    }

    SECTION("Test JsEngine::loadByteCode") {
        auto test = qjspp::Function{[](qjspp::Arguments const& args) -> qjspp::Value {
            REQUIRE(args.length() == 1);
            REQUIRE(args[0].isNumber());
            REQUIRE(args[0].asNumber().getInt32() == 1);
            return {};
        }};
        engine_->globalThis().set(qjspp::String{"foo"}, test);

        REQUIRE_NOTHROW(engine_->loadByteCode(std::filesystem::current_path() / "tests" / "test.bin"));
    }
}
