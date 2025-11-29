#pragma once
#include "qjspp/Forward.hpp"
#include "qjspp/Global.hpp"
#include <tuple>


namespace qjspp {

class JsEngine;

class Locker final {
public:
    explicit Locker(JsEngine& engine);
    explicit Locker(JsEngine* engine);
    ~Locker();

    QJSPP_DISABLE_COPY_MOVE(Locker);
    QJSPP_DISABLE_NEW();

    static JsEngine* currentEngine();

    static JsEngine& currentEngineChecked();

    static std::tuple<::JSRuntime*, ::JSContext*> currentRuntimeAndContextChecked();

    static ::JSRuntime* currentRuntimeChecked();

    static ::JSContext* currentContextChecked();

private:
    // 作用域链
    JsEngine* engine_{nullptr};
    Locker*   prev_{nullptr};

    static thread_local Locker* gCurrentScope_;
    friend class Unlocker;
};

class Unlocker final {
    JsEngine* engine_{nullptr};

public:
    explicit Unlocker();
    ~Unlocker();

    QJSPP_DISABLE_COPY_MOVE(Unlocker);
    QJSPP_DISABLE_NEW();
};


} // namespace qjspp