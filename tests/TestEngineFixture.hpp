#pragma once
#include "qjspp/runtime/JsEngine.hpp"


struct TestEngineFixture {
    qjspp::JsEngine* engine_;

    TestEngineFixture() { engine_ = new qjspp::JsEngine(); }
    ~TestEngineFixture() { delete engine_; }
};
