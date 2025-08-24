#include "qjspp/Module.hpp"
#include "qjspp/JsEngine.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/Values.hpp"
#include "quickjs.h"
#include <algorithm>
#include <cassert>
#include <utility>


namespace qjspp {


ModuleDefine::ModuleDefine(std::string name, std::vector<ClassDefine const*> class_)
: name_(std::move(name)),
  class_(std::move(class_)) {}

JSModuleDef* ModuleDefine::init(JsEngine* engine) const {
    auto module = JS_NewCModule(engine->context_, name_.c_str(), [](JSContext* ctx, JSModuleDef* module) -> int {
        auto engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));

        // 1) 查询缓存
        if (engine->loadedModules_.contains(module)) {
            auto& mdef = engine->loadedModules_.at(module);
            for (auto& clsDef : mdef->class_) {
                // 1.1) 检查类类别，进行分别查表
                if (clsDef->hasInstanceConstructor()) {
                    auto& mapped = engine->nativeInstanceClasses_.at(clsDef);
                    JsException::check(
                        JS_SetModuleExport(ctx, module, clsDef->name_.c_str(), JS_DupValue(ctx, mapped.first))
                    );
                } else {
                    auto object = engine->nativeStaticClasses_.at(clsDef);
                    JsException::check(
                        JS_SetModuleExport(ctx, module, clsDef->name_.c_str(), JS_DupValue(ctx, Value::extract(object)))
                    );
                }
            }
            return 0;
        }

        // 2) 无缓存，进行注册
        auto atom  = JS_GetModuleName(ctx, module);
        auto cname = JS_AtomToCString(ctx, atom);
        JS_FreeAtom(ctx, atom);
        if (cname == nullptr) return -1; // 无法获取模块名
        auto iter = engine->nativeModules_.find(cname);
        JS_FreeCString(ctx, cname);
        if (iter == engine->nativeModules_.end()) return -1; // 逻辑错误，未注册的模块
        auto& mdef = iter->second;

        for (auto& clsDef : mdef->class_) {
            // 2.1) 检查类别并尝试查表，不存在进行注册
            Value ctor;
            if (clsDef->hasInstanceConstructor()) {
                auto iter = engine->nativeInstanceClasses_.find(clsDef);
                if (iter != engine->nativeInstanceClasses_.end()) {
                    ctor = Value::wrap<Value>(iter->second.first);
                }
            } else {
                auto iter = engine->nativeStaticClasses_.find(clsDef);
                if (iter != engine->nativeStaticClasses_.end()) {
                    ctor = iter->second;
                }
            }

            if (!ctor) {
                ctor = engine->createJavaScriptClassOf(*clsDef);
            }

            JsException::check(
                JS_SetModuleExport(ctx, module, clsDef->name_.c_str(), JS_DupValue(ctx, Value::extract(ctor)))
            );
        }
        engine->loadedModules_.emplace(module, mdef);
        return 0;
    });

    for (auto& c : class_) {
        int code = JS_AddModuleExport(engine->context_, module, c->name_.c_str());
        JsException::check(code);
    }
    return module;
}


ModuleDefineBuilder::ModuleDefineBuilder(std::string name) : name_(std::move(name)) {}

ModuleDefineBuilder& ModuleDefineBuilder::exportClass(ClassDefine const& def) {
    class_.push_back(&def);
    return *this;
}

ModuleDefine ModuleDefineBuilder::build() const { return ModuleDefine{name_, class_}; }


} // namespace qjspp