#include "qjspp/JsScope.hpp"
#include "qjspp/JsEngine.hpp"
#include <mutex>

namespace qjspp {

thread_local JsScope* JsScope::gCurrentScope_ = nullptr;

JsScope::JsScope(JsEngine& engine) : JsScope(&engine) {}
JsScope::JsScope(JsEngine* engine) : engine_(engine), prev_(gCurrentScope_) {
    if (prev_) {
        this->prev_->engine_->mutex_.unlock();
    }
    this->engine_->mutex_.lock();
    gCurrentScope_ = this;
    JS_UpdateStackTop(this->engine_->runtime_);
}
JsScope::~JsScope() {
    this->engine_->mutex_.unlock();
    if (prev_) {
        this->prev_->engine_->mutex_.lock();
        gCurrentScope_ = this->prev_;
    }
}

JsEngine* JsScope::currentEngine() {
    if (gCurrentScope_) {
        return const_cast<JsEngine*>(gCurrentScope_->engine_);
    }
    return nullptr;
}

JsEngine& JsScope::currentEngineChecked() {
    auto current = currentEngine();
    if (current == nullptr) {
        throw std::logic_error("Failed to get current engine, no JsScope is active!");
    }
    return *current;
}

std::tuple<::JSRuntime*, ::JSContext*> JsScope::currentRuntimeAndContextChecked() {
    auto& current = currentEngineChecked();
    return std::make_tuple(current.runtime_, current.context_);
}

::JSRuntime* JsScope::currentRuntimeChecked() { return currentEngineChecked().runtime_; }

::JSContext* JsScope::currentContextChecked() { return currentEngineChecked().context_; }


ExitJsScope::ExitJsScope() {
    if (JsScope::gCurrentScope_) {
        this->engine_ = JsScope::gCurrentScope_->engine_;
        this->engine_->mutex_.unlock();
    }
}
ExitJsScope::~ExitJsScope() {
    if (this->engine_) {
        this->engine_->mutex_.lock();
    }
}


} // namespace qjspp
