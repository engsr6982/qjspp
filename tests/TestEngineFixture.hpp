#pragma once
#include "qjspp/JsEngine.hpp"
#include "qjspp/Types.hpp"


struct TestEngineFixture {
    qjspp::JsEngine* engine_;

    TestEngineFixture() { engine_ = new qjspp::JsEngine(); }
    ~TestEngineFixture() { delete engine_; }
};
