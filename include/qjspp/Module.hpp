#pragma once
#include "qjspp/Binding.hpp"
#include "qjspp/Definitions.hpp"
#include "qjspp/Types.hpp"
#include <string>
#include <vector>


namespace qjspp {


/**
 * ESModule - JavaScript ES Module
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Modules
 */
struct ModuleDefine {
    std::string const                     name_;
    std::vector<ClassDefine const*> const class_;
    std::vector<EnumDefine const*> const  enum_;

    explicit ModuleDefine(std::string name, std::vector<ClassDefine const*> class_);

private:
    JSModuleDef* init(JsEngine* engine) const;

    void _performExportDeclarations(JsEngine* engine, JSModuleDef* module) const;
    void _performExports(JsEngine* engine, JSContext* ctx, JSModuleDef* module) const;
    friend class JsEngine;
    friend struct ModuleLoader;
};


struct ModuleDefineBuilder {
private:
    std::string                     name_;
    std::vector<ClassDefine const*> class_;
    std::vector<EnumDefine const*>  enum_;

public:
    explicit ModuleDefineBuilder(std::string name) : name_{std::move(name)} {}

    ModuleDefineBuilder& exportClass(ClassDefine const& def) {
        class_.push_back(&def);
        return *this;
    }

    ModuleDefineBuilder& exportEnum(EnumDefine const& def) {
        enum_.push_back(&def);
        return *this;
    }

    [[nodiscard]] ModuleDefine build() const { return ModuleDefine{name_, class_}; }
};


inline ModuleDefineBuilder defineModule(std::string&& name) { return ModuleDefineBuilder{std::move(name)}; }


} // namespace qjspp