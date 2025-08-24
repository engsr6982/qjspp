#pragma once
#include "Global.hpp"
#include "qjspp/Concepts.hpp"
#include "qjspp/TaskQueue.hpp"
#include "qjspp/Types.hpp"
#include "quickjs.h"
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>


namespace qjspp {

struct WrappedResource;


class JsEngine final {
public:
    QJSPP_DISALLOW_COPY(JsEngine);
    explicit JsEngine();
    ~JsEngine();

    [[nodiscard]] ::JSRuntime* runtime() const;
    [[nodiscard]] ::JSContext* context() const;

    bool isJobPending() const;
    void pumpJobs();

    enum class EvalType { kGlobal, kModule };
    Value eval(String const& code, EvalType type = EvalType::kGlobal);
    Value eval(String const& code, String const& source, EvalType type = EvalType::kGlobal);
    Value eval(std::string const& code, std::string const& source = "<eval>", EvalType type = EvalType::kGlobal);

    Value loadScript(std::filesystem::path const& path, bool main = false);
    void  loadByteCode(std::filesystem::path const& path, bool main = false);

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
    Object registerNativeClass(ClassDefine const& def);

    /**
     * 注册一个原生模块
     * @param module 模块
     * @note 注册为模块后，需要使用 import xx from "<name>" 导入
     */
    void registerNativeModule(ModuleDefine const& module);

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
    Object newInstanceOfView(ClassDefine const& def, T* instance, Object ownerJs);

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


    using UnhandledJsExceptionCallback =
        void (*)(JsEngine* engine, JsException const& exception, UnhandledExceptionOrigin origin);

    void setUnhandledJsExceptionCallback(UnhandledJsExceptionCallback cb);
    void invokeUnhandledJsExceptionCallback(JsException const& exception, UnhandledExceptionOrigin origin);

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

    UnhandledJsExceptionCallback unhandledJsExceptionCallback_{nullptr};

    std::unordered_map<ClassDefine const*, Object>                      nativeStaticClasses_;   // {def, obj}
    std::unordered_map<ClassDefine const*, std::pair<JSValue, JSValue>> nativeInstanceClasses_; // {ctor, proto}
    std::unordered_map<std::string, ModuleDefine const*>                nativeModules_;         // {name, def} (懒加载)
    std::unordered_map<JSModuleDef*, ModuleDefine const*>               loadedModules_;         // {module, def}

#ifndef QJSPP_SKIP_INSTANCE_CALL_CHECK_CLASS_DEFINE
    static constexpr bool kInstanceCallCheckClassDefine = true;
#else
    static constexpr bool kInstanceCallCheckClassDefine = false; // 跳过实例调用时检查类定义
#endif

    // helpers
    JSClassID kPointerClassId{JS_INVALID_CLASS_ID};
    JSClassID kFunctionDataClassId{JS_INVALID_CLASS_ID}; // Function

    static void kTemplateClassFinalizer(JSRuntime*, JSValue val);
    static bool kUpdateModuleMainFlag(JSContext* ctx, JSModuleDef* module, bool isMain);
    static bool kUpdateModuleUrl(JSContext* ctx, JSModuleDef* module, std::string_view url);
    static bool kUpdateModuleMeta(JSContext* ctx, JSModuleDef* module, std::string_view url, bool isMain);

    struct ModuleLoader {
        static char*        normalize(JSContext* ctx, const char* base, const char* name, void* opaque);
        static JSModuleDef* loader(JSContext* ctx, const char* canonical, void* opaque);
    };

    class PauseGc final {
        JsEngine* engine_;
        QJSPP_DISALLOW_COPY_AND_MOVE(PauseGc);

    public:
        explicit PauseGc(JsEngine* engine);
        ~PauseGc();
    };

    friend class JsScope;
    friend class ExitJsScope;
    friend class Array; // 访问 lengthAtom_
    friend class Function;
    friend class PauseGc;
    friend struct ModuleDefine;
};


} // namespace qjspp

#include "JsEngine.inl"
