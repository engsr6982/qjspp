#pragma once
#include "Global.hpp"
#include "qjspp/Binding.hpp"
#include "qjspp/Concepts.hpp"
#include "qjspp/TaskQueue.hpp"
#include "qjspp/Types.hpp"
#include <cstddef>
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

    TaskQueue* getTaskQueue() const;

    void setData(std::shared_ptr<void> data);

    template <typename T>
    std::shared_ptr<T> getData() const;

    /**
     * 注册一个原生类
     * @param def 类定义
     * @note 默认此函数会注册的 native 类挂载到 JavaScript 的全局对象(globalThis)上
     */
    void registerNativeClass(ClassDefine const& def);

    /**
     * 注册一个原生模块
     * @param module 模块
     * @note 注册为模块后，需要使用 import xx from "<name>" 导入
     */
    // void registerNativeModule(std::unique_ptr<ESModule> module);

    Object newInstance(ClassDefine const& def, std::unique_ptr<WrappedResource>&& wrappedResource);

    // newInstanceOfRaw
    // newInstanceOfView
    // newInstanceOfView(owner)
    // newInstanceOfUnique
    // newInstanceOfShared
    // newInstanceOfWeak

    [[nodiscard]] bool isInstanceOf(Object const& thiz, ClassDefine const& def) const;

    [[nodiscard]] void* getNativeInstanceOf(Object const& thiz, ClassDefine const& def) const;

private:
    using RawFunctionCallback = Value (*)(const Arguments&, void*, void*, bool constructCall);
    Object   createJavaScriptClassOf(ClassDefine const& def);
    Function createQuickJsCFunction(void* data1, void* data2, RawFunctionCallback cb);
    Object   createConstuctor(ClassDefine const& def);
    Object   createPrototype(ClassDefine const& def);
    void     implStaticRegister(Object& ctor, StaticDefine const& def);

    ::JSRuntime* runtime_{nullptr};
    ::JSContext* context_{nullptr};

    int              pauseGcCount_ = 0;      // 暂停GC计数
    bool             isDestroying_{false};   // 正在销毁
    std::atomic_bool tickScheduled_ = false; // 是否已经调度了 tick (pumpJobs)

    std::shared_ptr<void>        userData_{nullptr}; // 用户数据
    std::unique_ptr<TaskQueue>   queue_{nullptr};    // 任务队列
    mutable std::recursive_mutex mutex_;             // 线程安全互斥量
    JSAtom                       lengthAtom_ = {};   // for Array

    std::unordered_map<ClassDefine const*, std::pair<JSValue, JSValue>> nativeClassData_; // {ctor, proto}
    // std::unordered_map<std::string, ESModule*>                          registeredNativeModules_;

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
