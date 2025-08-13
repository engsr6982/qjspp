#include "qjspp/ESModule.hpp"
#include "qjspp/JsEngine.hpp"
#include "qjspp/JsException.hpp"
#include "quickjs.h"
#include <algorithm>
#include <cassert>
#include <utility>


namespace qjspp {


ESModuleDefine::ESModuleDefine(std::string name, std::vector<ClassDefine const*> class_)
: name_(std::move(name)),
  class_(std::move(class_)) {}

JSModuleDef* ESModuleDefine::init(JsEngine* engine) const {
    auto module = JS_NewCModule(engine->context_, name_.c_str(), [](JSContext* ctx, JSModuleDef* module) -> int {
        auto engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));
        auto iter   = engine->nativeESModules_.find(module);
        assert(iter != engine->nativeESModules_.end());

        auto& def = iter->second;
        for (auto& c : def->class_) {
            auto iter = engine->nativeClassData_.find(c);
            assert(iter != engine->nativeClassData_.end());

            auto ctor = iter->second.first;
            int  code = JS_SetModuleExport(ctx, module, c->name_.c_str(), JS_DupValue(ctx, ctor));
            JsException::check(code);
        }
        return 0;
    });
    for (auto& c : class_) {
        int code = JS_AddModuleExport(engine->context_, module, c->name_.c_str());
        JsException::check(code);
    }
    return module;
}


ESModuleDefineBuilder::ESModuleDefineBuilder(std::string name) : name_(std::move(name)) {}

ESModuleDefineBuilder& ESModuleDefineBuilder::exportClass(ClassDefine const& def) {
    class_.push_back(&def);
    return *this;
}

ESModuleDefine ESModuleDefineBuilder::build() const { return ESModuleDefine{name_, class_}; }


} // namespace qjspp