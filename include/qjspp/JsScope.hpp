#pragma once
#include "Global.hpp"
#include "qjspp/Types.hpp"
#include <tuple>


namespace qjspp {


class JsScope final {
public:
    explicit JsScope(JsEngine& engine);
    explicit JsScope(JsEngine* engine);
    ~JsScope();

    QJSPP_DISALLOW_COPY_AND_MOVE(JsScope);
    QJSPP_DISALLOW_NEW();

    static JsEngine* currentEngine();

    static JsEngine& currentEngineChecked();

    static std::tuple<::JSRuntime*, ::JSContext*> currentRuntimeAndContextChecked();

    static ::JSRuntime* currentRuntimeChecked();

    static ::JSContext* currentContextChecked();

private:
    // 作用域链
    JsEngine* engine_{nullptr};
    JsScope*  prev_{nullptr};

    static thread_local JsScope* gCurrentScope_;
    friend class ExitJsScope;
};

class ExitJsScope final {
    JsEngine* engine_{nullptr};

public:
    explicit ExitJsScope();
    ~ExitJsScope();

    QJSPP_DISALLOW_COPY_AND_MOVE(ExitJsScope);
    QJSPP_DISALLOW_NEW();
};


} // namespace qjspp