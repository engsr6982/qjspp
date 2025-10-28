#include "qjspp/JsEngine.hpp"
#include "qjspp/Locker.hpp"
#include <mutex>

namespace qjspp {

thread_local Locker* Locker::gCurrentScope_ = nullptr;

Locker::Locker(JsEngine& engine) : Locker(&engine) {}
Locker::Locker(JsEngine* engine) : engine_(engine), prev_(gCurrentScope_) {
    if (prev_) {
        this->prev_->engine_->mutex_.unlock();
    }
    this->engine_->mutex_.lock();
    gCurrentScope_ = this;
    JS_UpdateStackTop(this->engine_->runtime_);
}
Locker::~Locker() {
    this->engine_->pumpJobs();
    this->engine_->mutex_.unlock();
    if (prev_) {
        this->prev_->engine_->mutex_.lock();
    }
    gCurrentScope_ = this->prev_;
}

JsEngine* Locker::currentEngine() {
    if (gCurrentScope_) {
        return gCurrentScope_->engine_;
    }
    return nullptr;
}

JsEngine& Locker::currentEngineChecked() {
    auto current = currentEngine();
    if (current == nullptr) [[unlikely]] {
        throw std::logic_error("Failed to get current engine, no Locker is active!");
    }
    return *current;
}

std::tuple<::JSRuntime*, ::JSContext*> Locker::currentRuntimeAndContextChecked() {
    auto& current = currentEngineChecked();
    return std::make_tuple(current.runtime_, current.context_);
}

::JSRuntime* Locker::currentRuntimeChecked() { return currentEngineChecked().runtime_; }

::JSContext* Locker::currentContextChecked() { return currentEngineChecked().context_; }


Unlocker::Unlocker() {
    if (Locker::gCurrentScope_) {
        this->engine_ = Locker::gCurrentScope_->engine_;
        this->engine_->mutex_.unlock();
    }
}
Unlocker::~Unlocker() {
    if (this->engine_) {
        this->engine_->mutex_.lock();
    }
}


} // namespace qjspp
