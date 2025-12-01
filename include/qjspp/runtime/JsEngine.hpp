#pragma once
#include "TaskQueue.hpp"
#include "qjspp/Forward.hpp"
#include "qjspp/Global.hpp"
#include "quickjs.h"
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>


namespace qjspp {
class JsException;
namespace bind {
struct JsManagedResource;
namespace meta {
struct StaticMemberDefine;
class EnumDefine;
struct ModuleDefine;
class ClassDefine;
} // namespace meta
} // namespace bind

class JsEngine final {
public:
    QJSPP_DISABLE_COPY(JsEngine);
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
    Object registerClass(bind::meta::ClassDefine const& def);

    /**
     * 注册一个原生模块
     * @param module 模块
     * @note 注册为模块后，需要使用 import xx from "<name>" 导入
     */
    void registerModule(bind::meta::ModuleDefine const& module);

    /**
     * 注册一个枚举
     * @param def 枚举定义
     * @note 对于枚举，qjspp 的行为如下:
     *       C++ -> Js  对 enum 进行 static_cast 到 int32(number) 传给 Js (TypeConverter)
     *       Js  -> C++ 因为 Js 没有枚举类型，手动传递 num 值麻烦，所以使用本API，创建一个静态的 Object
     *        对象，将枚举值映射到对象属性上，方便 Js 获取枚举值
     * @note qjspp 会对每个 enum object 设置一个 $name 属性，值为 enum 的名字
     */
    Object registerEnum(bind::meta::EnumDefine const& def);

    /**
     * 创建一个新的 JavaScript 类实例
     */
    Object newInstance(bind::meta::ClassDefine const& def, std::unique_ptr<bind::JsManagedResource>&& managedResource);

    /**
     * 创建一个新的 JavaScript 类实例
     * @warning C++实例必须分配在堆上(使用 `new` 操作符), 栈分配的实例会导致悬垂引用和 GC 崩溃。
     * @note qjspp 会接管实例的生命周期，GC 时自动销毁
     */
    template <typename T>
    Object newInstanceOfRaw(bind::meta::ClassDefine const& def, T* instance);

    /**
     * 创建一个新的 JavaScript 类实例
     * @note qjspp 不接管实例的生命周期，由外部管理实例的生命周期 (不自动销毁)
     */
    template <typename T>
    Object newInstanceOfView(bind::meta::ClassDefine const& def, T* instance);

    /**
     * 创建一个新的 JavaScript 类实例
     * @note qjspp 不接管实例的生命周期，对子资源增加引用计数关联生命周期(常见于对类成员创建Js实例，防止主实例 GC)
     */
    template <typename T>
    Object newInstanceOfView(bind::meta::ClassDefine const& def, T* instance, Object ownerJs);

    /**
     * 创建一个新的 JavaScript 类实例
     * @note qjspp 接管实例的生命周期，GC 时自动销毁
     */
    template <typename T>
    Object newInstanceOfUnique(bind::meta::ClassDefine const& def, std::unique_ptr<T>&& instance);

    /**
     * 创建一个新的 JavaScript 类实例
     * @note qjspp 共享对此实例的引用，仅在 Gc 时重置引用
     */
    template <typename T>
    Object newInstanceOfShared(bind::meta::ClassDefine const& def, std::shared_ptr<T>&& instance);

    /**
     * 创建一个新的 JavaScript 类实例
     * @note qjspp 仅在运行时尝试获取资源，如果获取不到资源则返回 null
     */
    template <typename T>
    Object newInstanceOfWeak(bind::meta::ClassDefine const& def, std::weak_ptr<T>&& instance);


    [[nodiscard]] bool isInstanceOf(Object const& thiz, bind::meta::ClassDefine const& def) const;

    [[nodiscard]] void* getNativeInstanceOf(Object const& thiz, bind::meta::ClassDefine const& def) const;

    template <typename T>
    [[nodiscard]] inline T* getNativeInstanceOf(Object const& obj, bind::meta::ClassDefine const& def) const;


    using UnhandledJsExceptionCallback =
        void (*)(JsEngine* engine, JsException const& exception, ExceptionDispatchOrigin origin);

    void setUnhandledJsExceptionCallback(UnhandledJsExceptionCallback cb);
    void invokeUnhandledJsException(JsException const& exception, ExceptionDispatchOrigin origin);

private:
    using RawFunctionCallback = Value (*)(Arguments const&, void*, void*);
    Object   newJsClass(bind::meta::ClassDefine const& def);
    Function newManagedRawFunction(void* data1, void* data2, RawFunctionCallback cb) const;
    Object   newJsConstructor(bind::meta::ClassDefine const& def) const;
    Object   newJsPrototype(bind::meta::ClassDefine const& def) const;
    void     implStaticRegister(Object& ctor, bind::meta::StaticMemberDefine const& def) const;
    Object   implRegisterEnum(bind::meta::EnumDefine const& def);

    ::JSRuntime* runtime_{nullptr};
    ::JSContext* context_{nullptr};

    int              pauseGcCount_ = 0;      // 暂停GC计数
    bool             isDestroying_{false};   // 正在销毁
    std::atomic_bool pumpScheduled_ = false; // 任务队列是否已经调度

    std::shared_ptr<void>        userData_{nullptr}; // 用户数据
    std::unique_ptr<TaskQueue>   queue_{nullptr};    // 任务队列
    mutable std::recursive_mutex mutex_;             // 线程安全互斥量
    JSAtom                       lengthAtom_ = {};   // for Array

#ifndef QJSPP_DONT_PATCH_CLASS_TO_STRING_TAG
    JSAtom toStringTagSymbol_{}; // for class、enum...
    void   updateToStringTag(Object& obj, std::string_view tag) const;
#endif

    UnhandledJsExceptionCallback unhandledJsExceptionCallback_{nullptr};

    std::unordered_map<bind::meta::EnumDefine const*, Object>  nativeEnums_;         // {def, obj}
    std::unordered_map<bind::meta::ClassDefine const*, Object> nativeStaticClasses_; // {def, obj}
    std::unordered_map<bind::meta::ClassDefine const*, std::pair<JSValue, JSValue>>
                                                                      nativeInstanceClasses_; // {ctor, proto}
    std::unordered_map<std::string, bind::meta::ModuleDefine const*>  nativeModules_;         // {name, def} (懒加载)
    std::unordered_map<JSModuleDef*, bind::meta::ModuleDefine const*> loadedModules_;         // {module, def}

#ifndef QJSPP_SKIP_INSTANCE_CALL_CHECK_CLASS_DEFINE
    static constexpr bool kInstanceCallCheckClassDefine = true;
#else
    static constexpr bool kInstanceCallCheckClassDefine = false; // 跳过实例调用时检查类定义
#endif

    static constexpr auto kEnumNameHelperProperty    = "$name";
    static constexpr auto kInstanceClassHelperEqlaus = "$equals";

    // helpers
    JSClassID kPointerClassId{JS_INVALID_CLASS_ID};
    JSClassID kFunctionDataClassId{JS_INVALID_CLASS_ID}; // Function

    static void kTemplateClassFinalizer(JSRuntime*, JSValue val);
    static bool kUpdateModuleMainFlag(JSContext* ctx, JSModuleDef* module, bool isMain);
    static bool kUpdateModuleUrl(JSContext* ctx, JSModuleDef* module, std::string_view url);
    static bool kUpdateModuleMeta(JSContext* ctx, JSModuleDef* module, std::string_view url, bool isMain);

    struct ModuleLoader {
        static std::optional<std::filesystem::path> resolveWithFallback(const std::filesystem::path& p);

        static char*        normalize(JSContext* ctx, const char* base, const char* name, void* opaque);
        static JSModuleDef* loader(JSContext* ctx, const char* canonical, void* opaque);
    };

    class PauseGc final {
        JsEngine* engine_;
        QJSPP_DISABLE_COPY_MOVE(PauseGc);

    public:
        explicit PauseGc(JsEngine* engine);
        ~PauseGc();
    };

    friend class Locker;
    friend class Unlocker;
    friend class Array; // 访问 lengthAtom_
    friend class Function;
    friend class PauseGc;
    friend bind::meta::ModuleDefine;
};


} // namespace qjspp

#include "JsEngine.inl"
