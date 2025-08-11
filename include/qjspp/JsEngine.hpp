#pragma once
#include "Global.hpp"
#include "qjspp/Concepts.hpp"
#include "qjspp/Types.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>


namespace qjspp {


class JsEngine final {
public:
    QJSPP_DISALLOW_COPY(JsEngine);
    explicit JsEngine();
    ~JsEngine();

    [[nodiscard]] ::JSRuntime* runtime() const;
    [[nodiscard]] ::JSContext* context() const;

    void pumpJobs();

    Value eval(String const& code);
    Value eval(String const& code, String const& filename);
    Value eval(std::string const& code, std::string const& filename = "<eval>");

    Value loadScript(std::filesystem::path const& path);
    void  loadByteCode(std::filesystem::path const& path);

    Object globalThis() const;

    [[nodiscard]] bool isDestroying() const;

    void gc();

    size_t getMemoryUsage();

    void setData(std::shared_ptr<void> data);

    template <typename T>
    std::shared_ptr<T> getData() const;

    void registerNativeClass(ClassDefine const& binding);

    // newInstance
    // newInstanceOfRaw
    // newInstanceOfView
    // newInstanceOfView(owner)
    // newInstanceOfUnique
    // newInstanceOfShared
    // newInstanceOfWeak
    // isInstanceOf
    // getNativeInstanceOf

private:
    struct ManagedResource {
        JsEngine*                  engine_;
        void*                      resource_;
        std::function<void(void*)> deleter_;
    };

    ::JSRuntime* runtime_{nullptr};
    ::JSContext* context_{nullptr};

    int              pauseGcCount_ = 0;
    bool             isDestroying_{false};
    std::atomic_bool tickScheduled_ = false;

    std::shared_ptr<void> userData_{nullptr};

    mutable std::recursive_mutex mutex_;           // for thread safety
    JSAtom                       lengthAtom_ = {}; // for Array

#ifdef QJSPP_DEBUG
    std::atomic<uint64_t> valueConter_{0}; // 统计分配的 Value 数量(检查内存泄漏)
#endif

    // TODO:
    // std::unordered_map<, std::pair<JSValue, JSValue>> nativeClassRegistry_;
    // std::unordered_map<std::string, ClassBinding const*>                      mRegisteredBindings;
    // std::unordered_map<ClassBinding const*, v8::Global<v8::FunctionTemplate>> mJsClassConstructor;

    // helpers
    static JSClassID kPointerClassId;
    static JSClassID kFunctionDataClassId; // Function
    static JSClassID kInstanceClassId;

    friend class JsScope;
    friend class ExitJsScope;
    friend class Array; // 访问 lengthAtom_
    friend class Function;
    friend class EventLoop;
};


} // namespace qjspp

#include "JsEngine.inl"
