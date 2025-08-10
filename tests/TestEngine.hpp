#pragma once
#include "qjspp/JsEngine.hpp"


struct TestEngine {
    qjspp::JsEngine* engine_;

    TestEngine() : engine_(new qjspp::JsEngine()) {}
    ~TestEngine() { delete engine_; }
};
