#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include "qjspp/Forward.hpp"
#include "qjspp/bind/meta/ClassDefine.hpp"
#include "qjspp/bind/meta/EnumDefine.hpp"
#include "qjspp/bind/meta/ModuleDefine.hpp"
#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/runtime/TaskQueue.hpp"
#include "qjspp/runtime/detail/BindRegistry.hpp"
#include "qjspp/runtime/detail/ModuleLoader.hpp"
#include "qjspp/types/Arguments.hpp"
#include "qjspp/types/Function.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"


namespace qjspp {

JsEngine::PauseGc::PauseGc(JsEngine* engine) : engine_(engine) { engine_->pauseGcCount_++; }
JsEngine::PauseGc::~PauseGc() { engine_->pauseGcCount_--; }


/* JsEngine impl */
JsEngine::JsEngine() : runtime_(JS_NewRuntime()), queue_(std::make_unique<TaskQueue>()) {
    if (runtime_) {
        context_ = JS_NewContext(runtime_);
    }

    if (!runtime_ || !context_) {
        throw std::logic_error("Failed to create JS runtime or context");
    }

#ifdef QJSPP_DEBUG
    JS_SetDumpFlags(runtime_, JS_DUMP_LEAKS | JS_DUMP_ATOM_LEAKS);
#endif

    // 指针数据
    JSClassDef pointer{};
    pointer.class_name = "RawPointer";
    JS_NewClassID(runtime_, &kPointerClassId);
    JS_NewClass(runtime_, kPointerClassId, &pointer);

    // 函数数据
    JSClassDef function{};
    function.class_name = "RawFunction";
    function.finalizer  = [](JSRuntime*, JSValue val) {
        auto id  = JS_GetClassID(val);
        auto ptr = JS_GetOpaque(val, id);
        if (ptr) {
            delete static_cast<FunctionCallback*>(ptr);
        }
    };
    JS_NewClassID(runtime_, &kFunctionDataClassId);
    JS_NewClass(runtime_, kFunctionDataClassId, &function);

    lengthAtom_ = JS_NewAtom(context_, "length");

    {
        Locker scope{this};
        auto   sym = eval("(Symbol.toStringTag)"); // 获取 Symbol.toStringTag
        if (!JS_IsSymbol(Value::extract(sym))) {
            throw std::logic_error("Failed to get Symbol.toStringTag");
        }
        toStringTagSymbol_ = JS_ValueToAtom(context_, Value::extract(sym));
    }

    bindRegistry_ = std::make_unique<detail::BindRegistry>(*this);

    JS_SetRuntimeOpaque(runtime_, this);
    JS_SetContextOpaque(context_, this);
    JS_SetModuleLoaderFunc(runtime_, &detail::ModuleLoader::normalize, &detail::ModuleLoader::loader, this);
}

JsEngine::~JsEngine() {
    isDestroying_ = true;
    userData_.reset();
    queue_.reset();

    JS_FreeAtom(context_, lengthAtom_);
    JS_FreeAtom(context_, toStringTagSymbol_);

    bindRegistry_.reset();

    JS_RunGC(runtime_);
    JS_FreeContext(context_);
    JS_FreeRuntime(runtime_);
}

::JSRuntime* JsEngine::runtime() const { return runtime_; }

::JSContext* JsEngine::context() const { return context_; }

bool JsEngine::isJobPending() const { return JS_IsJobPending(runtime_); }

void JsEngine::pumpJobs() {
    if (isDestroying()) return;

    bool no = false;
    if (JS_IsJobPending(runtime_) && pumpScheduled_.compare_exchange_strong(no, true)) {
        queue_->postTask(
            [](void* data) {
                auto       engine = static_cast<JsEngine*>(data);
                JSContext* ctx    = nullptr;
                Locker     lock(engine);
                while (JS_ExecutePendingJob(engine->runtime_, &ctx) > 0) {}
                engine->pumpScheduled_ = false;
            },
            this
        );
    }
}

Value JsEngine::eval(String const& code, EvalType type) { return eval(code.value(), "<eval>", type); }
Value JsEngine::eval(String const& code, String const& source, EvalType type) {
    return eval(code.value(), source.isValid() ? source.value() : "<eval>", type);
}
Value JsEngine::eval(std::string const& code, std::string const& source, EvalType type) {
    auto result = JS_Eval(
        context_,
        code.c_str(),
        code.size(),
        source.c_str(),
        type == EvalType::kGlobal ? JS_EVAL_TYPE_GLOBAL : JS_EVAL_TYPE_MODULE
    );
    JsException::check(result);
    pumpJobs();
    return Value::move<Value>(result);
}

Value JsEngine::loadScript(std::filesystem::path const& path, bool main) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error{std::format("File not found: {}", path.string())};
    }

    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error{std::format("Failed to open file: {}", path.string())};
    }
    std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    auto url = path.is_absolute() ? path.string() : std::filesystem::absolute(path).string();
#ifdef _WIN32
    std::replace(url.begin(), url.end(), '\\', '/');
#endif

    // 1) 编译模块
    auto result =
        JS_Eval(context_, code.c_str(), code.size(), url.c_str(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    JsException::check(result); // SyntaxError

    // 2) 设置模块元数据
    assert(JS_VALUE_GET_TAG(result) == JS_TAG_MODULE);
    auto module = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(result));
    detail::ModuleLoader::setModuleMainFlag(context_, module, main);

    // 3) 执行模块
    result = JS_EvalFunction(context_, result);
    JsException::check(result); // SyntaxError

    // 4) 检查模块是否返回 rejected promise
    JSPromiseStateEnum state = JS_PromiseState(context_, result);
    if (state == JSPromiseStateEnum::JS_PROMISE_REJECTED) {
        JSValue msg = JS_PromiseResult(context_, result);
        JS_Throw(context_, msg);
        JsException::check(-1);
    }

    pumpJobs();
    return Value::move<Value>(result);
}

void JsEngine::loadByteCode(std::filesystem::path const& path, bool main) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error{std::format("Failed to open binary file: {}", path.string())};
    }
    std::vector<uint8_t> bytecode((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    // 1) 读取字节码
    JSValue result = JS_ReadObject(context_, bytecode.data(), bytecode.size(), JS_READ_OBJ_BYTECODE);
    JsException::check(result); // SyntaxError

    // 2) 设置模块元数据
    if (JS_VALUE_GET_TAG(result) == JS_TAG_MODULE) {
        if (JS_ResolveModule(context_, result) < 0) {
            JS_FreeValue(context_, result);
            JsException::check(-1, "Failed to resolve module");
        }
        auto url = path.is_absolute() ? path.string() : std::filesystem::absolute(path).string();
#ifdef _WIN32
        std::replace(url.begin(), url.end(), '\\', '/');
#endif
        url         = std::string{detail::ModuleLoader::kFilePrefix} + url;
        auto module = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(result));
        detail::ModuleLoader::setModuleMeta(context_, module, url, main);
    }

    // 3) 执行模块
    result = JS_EvalFunction(context_, result);
    JsException::check(result); // SyntaxError

    // 4) 检查模块是否返回 rejected promise
    JSPromiseStateEnum state = JS_PromiseState(context_, result);
    if (state == JSPromiseStateEnum::JS_PROMISE_REJECTED) {
        JSValue msg = JS_PromiseResult(context_, result);
        JS_Throw(context_, msg);
        JsException::check(-1);
    }

    JS_FreeValue(context_, result);
    pumpJobs();
}

Object JsEngine::globalThis() const {
    auto global = JS_GetGlobalObject(context_);
    JsException::check(global);
    return Value::move<Object>(global);
}

bool JsEngine::isDestroying() const { return isDestroying_; }

void JsEngine::gc() {
    Locker lock(this);
    if (isDestroying() || pauseGcCount_ != 0) return;
    JS_RunGC(runtime_);
}

size_t JsEngine::getMemoryUsage() {
    Locker        lock(this);
    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(runtime_, &usage);
    return usage.memory_used_size;
}

TaskQueue* JsEngine::getTaskQueue() const { return queue_.get(); }

void JsEngine::setData(std::shared_ptr<void> data) { userData_ = std::move(data); }

bool JsEngine::registerClass(bind::meta::ClassDefine const& def) { return bindRegistry_->tryRegister(def); }
bool JsEngine::registerEnum(bind::meta::EnumDefine const& def) { return bindRegistry_->tryRegister(def); }
bool JsEngine::registerModule(bind::meta::ModuleDefine const& module) { return bindRegistry_->tryRegister(module); }

void JsEngine::setObjectToStringTag(Object& obj, std::string_view tag) const {
    JS_DefinePropertyValue(
        context_,
        Value::extract(obj),
        toStringTagSymbol_,
        JS_NewString(context_, tag.data()),
        JS_PROP_CONFIGURABLE
    );
}

Object
JsEngine::newInstance(bind::meta::ClassDefine const& def, std::unique_ptr<bind::JsManagedResource>&& managedResource) {
    auto iter = bindRegistry_->instanceClasses_.find(&def);
    if (iter == bindRegistry_->instanceClasses_.end()) {
        throw std::logic_error{
            std::format("The native class {} is not registered, so an instance cannot be constructed.", def.name_)
        };
    }
    auto instance = JS_NewObjectClass(context_, static_cast<int>(kPointerClassId));
    JsException::check(instance);
    JS_SetOpaque(instance, managedResource.release());

    std::array<JSValue, 1> args = {instance};

    auto& ctor   = iter->second.first;
    auto  result = JS_CallConstructor(context_, ctor, args.size(), args.data());

    JS_FreeValue(context_, instance);
    JsException::check(result);
    pumpJobs();

    return Value::move<Object>(result);
}

bool JsEngine::isInstanceOf(Object const& thiz, bind::meta::ClassDefine const& def) const {
    auto iter = bindRegistry_->instanceClasses_.find(&def);
    if (iter != bindRegistry_->instanceClasses_.end()) {
        return thiz.instanceOf(Value::wrap<Value>(iter->second.first));
    }
    return false;
}

void* JsEngine::getNativeInstanceOf(Object const& thiz, bind::meta::ClassDefine const& def) const {
    if (!isInstanceOf(thiz, def)) {
        return nullptr;
    }
    auto managed_resource =
        static_cast<bind::JsManagedResource*>(JS_GetOpaque(Value::extract(thiz), def.instanceMemberDef_.classId_));
    return (*managed_resource)();
}


} // namespace qjspp