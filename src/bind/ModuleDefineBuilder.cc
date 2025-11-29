#include "qjspp/bind/builder/ModuleDefineBuilder.hpp"
#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/JsException.hpp"
#include "qjspp/types/Value.hpp"

#include <algorithm>
#include <cassert>
#include <utility>


namespace qjspp::bind {


meta::ModuleDefine::ModuleDefine(
    std::string                     name,
    std::vector<ClassDefine const*> class_,
    std::vector<EnumDefine const*>  enum_
)
: name_(std::move(name)),
  refClass_(std::move(class_)),
  refEnum_(std::move(enum_)) {}

JSModuleDef* bind::meta::ModuleDefine::init(JsEngine* engine) const {
    auto module = JS_NewCModule(engine->context_, name_.c_str(), [](JSContext* ctx, JSModuleDef* module) -> int {
        auto engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));

        ModuleDefine const* def = nullptr;

        auto iter = engine->loadedModules_.find(module);
        if (iter != engine->loadedModules_.end()) {
            // 模块已实例化，直接获取
            def = iter->second;
        } else {
            // 模块未实例化，进行实例化
            auto atom  = JS_GetModuleName(ctx, module);
            auto cname = JS_AtomToCString(ctx, atom);
            JS_FreeAtom(ctx, atom);
            if (cname == nullptr) {
                return -1; // 无法获取模块名
            }

            auto iter = engine->nativeModules_.find(cname); // 查找模块定义(使用模块名)
            JS_FreeCString(ctx, cname);
            if (iter == engine->nativeModules_.end()) {
                return -1; // 逻辑错误，未注册的模块
            }
            def = iter->second;
        }
        assert(def != nullptr);
        def->_performExports(engine, ctx, module);
        engine->loadedModules_.emplace(module, def);
        return 0;
    });

    _performExportDeclarations(engine, module);

    return module;
}

void bind::meta::ModuleDefine::_performExportDeclarations(JsEngine* engine, JSModuleDef* module) const {
    for (auto& c : refClass_) {
        JsException::check(JS_AddModuleExport(engine->context_, module, c->name_.c_str()));
    }
    for (auto& e : refEnum_) {
        JsException::check(JS_AddModuleExport(engine->context_, module, e->name_.c_str()));
    }
}
void bind::meta::ModuleDefine::_performExports(JsEngine* engine, JSContext* ctx, JSModuleDef* module) const {
    for (auto& def : refClass_) {
        Value ctor{}; // undefined

        if (def->hasConstructor()) {
            auto iter = engine->nativeInstanceClasses_.find(def);
            if (iter != engine->nativeInstanceClasses_.end()) {
                ctor = Value::wrap<Value>(iter->second.first);
            }
        } else {
            auto iter = engine->nativeStaticClasses_.find(def);
            if (iter != engine->nativeStaticClasses_.end()) {
                ctor = iter->second;
            }
        }

        if (!ctor.isValid()) {
            ctor = engine->newJsClass(*def); // 无缓存进行注册
        }

        JsException::check(JS_SetModuleExport(ctx, module, def->name_.c_str(), JS_DupValue(ctx, Value::extract(ctor))));
    }
    for (auto& def : refEnum_) {
        if (!engine->nativeEnums_.contains(def)) {
            engine->implRegisterEnum(*def); // 无缓存进行注册
        }
        auto obj = engine->nativeEnums_.at(def);
        JsException::check(JS_SetModuleExport(ctx, module, def->name_.c_str(), JS_DupValue(ctx, Value::extract(obj))));
    }
}


} // namespace qjspp::bind