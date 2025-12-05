#include "qjspp/bind/meta/ModuleDefine.hpp"
#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/JsException.hpp"
#include "qjspp/types/Value.hpp"

#include "qjspp/runtime/detail/BindRegistry.hpp"

#include <algorithm>
#include <cassert>
#include <utility>


namespace qjspp::bind {


meta::ModuleDefine::ModuleDefine(
    std::string                     name,
    std::vector<ClassDefine const*> refClass,
    std::vector<EnumDefine const*>  refEnum,
    std::vector<ConstantExport>     exports,
    std::vector<FunctionExport>     exportsFunctions
)
: name_(std::move(name)),
  refClass_(std::move(refClass)),
  refEnum_(std::move(refEnum)),
  variables_(std::move(exports)),
  functions_(std::move(exportsFunctions)) {}

JSModuleDef* bind::meta::ModuleDefine::init(JsEngine* engine) const {
    auto module = JS_NewCModule(engine->context_, name_.c_str(), [](JSContext* ctx, JSModuleDef* module) -> int {
        auto engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));

        ModuleDefine const* def = nullptr;

        auto iter = engine->bindRegistry_->loadedModules_.find(module);
        if (iter != engine->bindRegistry_->loadedModules_.end()) {
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

            auto iter = engine->bindRegistry_->lazyModules_.find(cname); // 查找模块定义(使用模块名)
            JS_FreeCString(ctx, cname);
            if (iter == engine->bindRegistry_->lazyModules_.end()) {
                return -1; // 逻辑错误，未注册的模块
            }
            def = iter->second;
        }
        assert(def != nullptr);
        def->_performExports(engine, ctx, module);
        engine->bindRegistry_->loadedModules_.emplace(module, def);
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
    for (auto& v : variables_) {
        JsException::check(JS_AddModuleExport(engine->context_, module, v.name_.c_str()));
    }
    for (auto& f : functions_) {
        JsException::check(JS_AddModuleExport(engine->context_, module, f.name_.c_str()));
    }
}
void bind::meta::ModuleDefine::_performExports(JsEngine* engine, JSContext* ctx, JSModuleDef* module) const {
    for (auto& def : refClass_) {
        Value ctor{}; // undefined

        if (def->hasConstructor()) {
            auto iter = engine->bindRegistry_->instanceClasses_.find(def);
            if (iter != engine->bindRegistry_->instanceClasses_.end()) {
                ctor = Value::wrap<Value>(iter->second.first);
            }
        } else {
            auto iter = engine->bindRegistry_->staticClasses_.find(def);
            if (iter != engine->bindRegistry_->staticClasses_.end()) {
                ctor = iter->second;
            }
        }

        if (!ctor.isValid()) {
            ctor = engine->bindRegistry_->_registerClass(*def); // 无缓存进行注册
        }

        JsException::check(JS_SetModuleExport(ctx, module, def->name_.c_str(), JS_DupValue(ctx, Value::extract(ctor))));
    }

    for (auto& def : refEnum_) {
        if (!engine->bindRegistry_->enums_.contains(def)) {
            auto enu = engine->bindRegistry_->_buildEnum(*def);
            engine->bindRegistry_->enums_.emplace(def, enu);
        }
        auto obj = engine->bindRegistry_->enums_.at(def);
        JsException::check(JS_SetModuleExport(ctx, module, def->name_.c_str(), JS_DupValue(ctx, Value::extract(obj))));
    }

    if (!engine->bindRegistry_->moduleExports_.contains(module)) {
        engine->bindRegistry_->_buildModuleExports(*this, module);
    }
    auto& exports = engine->bindRegistry_->moduleExports_.at(module);
    for (auto& var : variables_) {
        auto& constant = exports.constants_.at(&var);
        JsException::check(
            JS_SetModuleExport(ctx, module, var.name_.c_str(), JS_DupValue(ctx, Value::extract(constant)))
        );
    }
    for (auto& func : functions_) {
        auto& fn = exports.functions_.at(&func);
        JsException::check(JS_SetModuleExport(ctx, module, func.name_.c_str(), JS_DupValue(ctx, Value::extract(fn))));
    }
}


} // namespace qjspp::bind