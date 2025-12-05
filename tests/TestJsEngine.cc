#include "TestEngineFixture.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_exception.hpp"
#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/Arguments.hpp"
#include "qjspp/types/Boolean.hpp"
#include "qjspp/types/Function.hpp"
#include "qjspp/types/Number.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"


#include <algorithm>
#include <filesystem>


TEST_CASE_METHOD(TestEngineFixture, "Test JsEngine") {
    qjspp::Locker scope(engine_);

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

    SECTION("Test Promise") {
        bool done = false;
        auto test = qjspp::Function{[&done](qjspp::Arguments const& args) -> qjspp::Value {
            REQUIRE(args.length() == 1);
            REQUIRE(args[0].isBoolean());
            done = args[0].asBoolean().value();
            return {};
        }};
        engine_->globalThis().set(qjspp::String{"setDone"}, test);

        engine_->eval(R"(
            new Promise((resolve, reject) => {
                resolve();
            }).then(() => {
                new Promise((resolve, reject) => {
                    resolve();
                }).then(() => {
                    setDone(true);
                });
            });
        )");
        engine_->getTaskQueue()->shutdown(true);
        engine_->getTaskQueue()->loopAndWait();
        REQUIRE(done == true);
    }
}
