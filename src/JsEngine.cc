#include "qjspp/JsEngine.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/JsScope.hpp"
#include "qjspp/Values.hpp"
#include "quickjs-libc.h"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace qjspp {

JSClassID             JsEngine::kPointerClassId      = 0;
JSClassID             JsEngine::kInstanceClassId     = 0;
JSClassID             JsEngine::kFunctionDataClassId = 0;
static std::once_flag kGlobalQjsClass;


JsEngine::JsEngine() : runtime_(JS_NewRuntime()) {
    if (context_) {
        context_ = JS_NewContext(runtime_);
    }

    if (!runtime_ || !context_) {
        throw std::logic_error("Failed to create JS runtime or context");
    }

    std::call_once(kGlobalQjsClass, [this]() {
        JS_NewClassID(runtime_, &kPointerClassId);
        JS_NewClassID(runtime_, &kInstanceClassId);
        JS_NewClassID(runtime_, &kFunctionDataClassId);
    });

    JSClassDef pointer{};
    pointer.class_name = "RawPointer";
    JS_NewClass(runtime_, kPointerClassId, &pointer);

    JSClassDef function{};
    function.class_name = "RawFunction";
    function.finalizer  = [](JSRuntime* /*rt*/, JSValue val) {
        auto ptr = JS_GetOpaque(val, kFunctionDataClassId);
        if (ptr) {
            delete static_cast<FunctionCallback*>(ptr);
        }
    };
    JS_NewClass(runtime_, kFunctionDataClassId, &function);

    JSClassDef instance{};
    instance.class_name = "NativeInstance";
    instance.finalizer  = [](JSRuntime* /*rt*/, JSValue val) {
        auto ptr = JS_GetOpaque(val, kInstanceClassId);
        if (ptr) {
            // TODO: fix this
            // auto opaque = static_cast<InstanceClassOpaque*>(ptr);
            // // reset the weak reference
            // PauseGc pauseGc(opaque->scriptClassPointer->internalState_.engine);
            // opaque->scriptClassPointer->internalState_.weakRef_ = JS_UNDEFINED;
            // delete opaque->scriptClassPointer;
            // delete opaque;
        }
    };
    JS_NewClass(runtime_, kInstanceClassId, &instance);

    lengthAtom_ = JS_NewAtom(context_, "length");

    JS_SetModuleLoaderFunc(runtime_, NULL, js_module_loader, NULL);
}

JsEngine::~JsEngine() {
    isDestroying_ = true;
    userData_.reset();

    JS_FreeAtom(context_, lengthAtom_);

    // TODO: free all managed resources

    JS_RunGC(runtime_);
    JS_FreeContext(context_);
    JS_FreeRuntime(runtime_);
}

::JSRuntime* JsEngine::runtime() const { return runtime_; }

::JSContext* JsEngine::context() const { return context_; }

void JsEngine::pumpJobs() {
    if (isDestroying()) return;

    bool no = false;
    if (JS_IsJobPending(runtime_) && tickScheduled_.compare_exchange_strong(no, true)) {
        JsScope lock(this);
        while (JS_ExecutePendingJob(runtime_, &context_) > 0) {}
        tickScheduled_ = false;
    }
}

Value JsEngine::eval(String const& code) { return eval(code.value()); }
Value JsEngine::eval(String const& code, String const& filename) { return eval(code.value(), filename.value()); }
Value JsEngine::eval(std::string const& code, std::string const& filename) {
    auto result = JS_Eval(context_, code.c_str(), code.size(), filename.c_str(), JS_EVAL_TYPE_GLOBAL);
    JsException::check(result);
    pumpJobs();
    return Value::wrap<Value>(result);
}

Value JsEngine::loadScript(std::filesystem::path const& path) {
    if (!std::filesystem::exists(path)) {
        throw JsException{"File not found: " + path.string()};
    }

    std::ifstream ifs(path);
    if (!ifs) {
        throw JsException{"Failed to open file: " + path.string()};
    }
    std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    auto        file = path.filename().string();
    std::replace(file.begin(), file.end(), '\\', '/'); // replace \\ to /

    auto result = JS_Eval(context_, code.c_str(), code.size(), file.c_str(), JS_EVAL_TYPE_MODULE);
    JsException::check(result); // check SyntaxError

    // module return rejected promise
    JSPromiseStateEnum state = JS_PromiseState(context_, result);
    if (state == JSPromiseStateEnum::JS_PROMISE_REJECTED) {
        JSValue msg = JS_PromiseResult(context_, result);
        JS_Throw(context_, msg);
        JsException::check(-1);
    }
    pumpJobs();

    return Value::wrap<Value>(result);
}

void JsEngine::loadByteCode(std::filesystem::path const& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw JsException{"Failed to open binary file: " + path.string()};
    }
    std::vector<uint8_t> bytecode((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    js_std_eval_binary(context_, bytecode.data(), bytecode.size(), JS_EVAL_TYPE_MODULE);

    JsException::check(context_); // 检查是否有挂起的异常

    pumpJobs();
}

Object JsEngine::globalThis() const {
    auto global = JS_GetGlobalObject(context_);
    JsException::check(global);
    return Value::wrap<Object>(global);
}

bool JsEngine::isDestroying() const { return isDestroying_; }

void JsEngine::gc() {
    JsScope lock(this);
    if (isDestroying() || pauseGcCount_ != 0) return;
    JS_RunGC(runtime_);
}

size_t JsEngine::getMemoryUsage() {
    JsScope       lock(this);
    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(runtime_, &usage);
    return usage.memory_used_size;
}

void JsEngine::setData(std::shared_ptr<void> data) { userData_ = std::move(data); }


void JsEngine::registerNativeClass(ClassDefine const& binding) {}


} // namespace qjspp