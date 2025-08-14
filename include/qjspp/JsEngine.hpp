#pragma once
#include "Global.hpp"
#include "qjspp/Binding.hpp"
#include "qjspp/Concepts.hpp"
#include "qjspp/TaskQueue.hpp"
#include "qjspp/Types.hpp"
#include "quickjs.h"
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
    void registerNativeESModule(ESModuleDefine const& module);

    /**
     * 创建一个新的 JavaScript 类实例
     */
    Object newInstance(ClassDefine const& def, std::unique_ptr<WrappedResource>&& wrappedResource);

    /**
     * 创建一个新的 JavaScript 类实例
     * @warning C++实例必须分配在堆上(使用 `new` 操作符), 栈分配的实例会导致悬垂引用和 GC 崩溃。
     * @note qjspp 会接管实例的生命周期，GC 时自动销毁
     */
    template <typename T>
    Object newInstanceOfRaw(ClassDefine const& def, T* instance);

    /**
     * 创建一个新的 JavaScript 类实例
     * @note qjspp 不接管实例的生命周期，由外部管理实例的生命周期 (不自动销毁)
     */
    template <typename T>
    Object newInstanceOfView(ClassDefine const& def, T* instance);

    /**
     * 创建一个新的 JavaScript 类实例
     * @note qjspp 不接管实例的生命周期，对子资源增加引用计数关联生命周期(常见于对类成员创建Js实例，防止主实例 GC)
     */
    template <typename T>
    Object newInstanceOfView(ClassDefine const& def, T* instance, Object const& ownerJs);

    /**
     * 创建一个新的 JavaScript 类实例
     * @note qjspp 接管实例的生命周期，GC 时自动销毁
     */
    template <typename T>
    Object newInstanceOfUnique(ClassDefine const& def, std::unique_ptr<T>&& instance);

    /**
     * 创建一个新的 JavaScript 类实例
     * @note qjspp 共享对此实例的引用，仅在 Gc 时重置引用
     */
    template <typename T>
    Object newInstanceOfShared(ClassDefine const& def, std::shared_ptr<T>&& instance);

    /**
     * 创建一个新的 JavaScript 类实例
     * @note qjspp 仅在运行时尝试获取资源，如果获取不到资源则返回 null
     */
    template <typename T>
    Object newInstanceOfWeak(ClassDefine const& def, std::weak_ptr<T>&& instance);


    [[nodiscard]] bool isInstanceOf(Object const& thiz, ClassDefine const& def) const;

    [[nodiscard]] void* getNativeInstanceOf(Object const& thiz, ClassDefine const& def) const;

    template <typename T>
    [[nodiscard]] inline T* getNativeInstanceOf(Object const& obj, ClassDefine const& def) const;

private:
    using RawFunctionCallback = Value (*)(const Arguments&, void*, void*, bool constructCall);
    Object   createJavaScriptClassOf(ClassDefine const& def);
    Function createQuickJsCFunction(void* data1, void* data2, RawFunctionCallback cb);
    Object   createConstructor(ClassDefine const& def);
    Object   createPrototype(ClassDefine const& def);
    void     implStaticRegister(Object& ctor, StaticDefine const& def);

    ::JSRuntime* runtime_{nullptr};
    ::JSContext* context_{nullptr};

    int              pauseGcCount_ = 0;      // 暂停GC计数
    bool             isDestroying_{false};   // 正在销毁
    std::atomic_bool pumpScheduled_ = false; // 任务队列是否已经调度

    std::shared_ptr<void>        userData_{nullptr}; // 用户数据
    std::unique_ptr<TaskQueue>   queue_{nullptr};    // 任务队列
    mutable std::recursive_mutex mutex_;             // 线程安全互斥量
    JSAtom                       lengthAtom_ = {};   // for Array

    std::unordered_map<ClassDefine const*, std::pair<JSValue, JSValue>> nativeClassData_; // {ctor, proto}
    std::unordered_map<JSModuleDef*, ESModuleDefine const*>             nativeESModules_;

#define QJSPP_ENABLE_INSTANCE_CALL_CHECK_CLASS_DEFINE
#ifdef QJSPP_ENABLE_INSTANCE_CALL_CHECK_CLASS_DEFINE
    static constexpr bool kInstanceCallCheckClassDefine = true;
#else
    static constexpr bool kInstanceCallCheckClassDefine = false;
#endif

    // helpers
    JSClassID kPointerClassId{JS_INVALID_CLASS_ID};
    JSClassID kFunctionDataClassId{JS_INVALID_CLASS_ID}; // Function

    static void kTemplateClassFinalizer(JSRuntime*, JSValue val);

    friend class JsScope;
    friend class ExitJsScope;
    friend class Array; // 访问 lengthAtom_
    friend class Function;
    friend class PauseGc;
    friend struct ESModuleDefine;
};

class PauseGc final {
    JsEngine* engine_;
    QJSPP_DISALLOW_COPY_AND_MOVE(PauseGc);

public:
    explicit PauseGc(JsEngine* engine);
    ~PauseGc();
};


} // namespace qjspp

#include "JsEngine.inl"
