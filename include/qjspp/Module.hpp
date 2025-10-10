#pragma once
#include "qjspp/Definitions.hpp"
#include "qjspp/Types.hpp"
#include <string>
#include <vector>


namespace qjspp {


/**
 * ESModule - JavaScript ES Module
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Modules
 *
 * @lifetime: Static or longer than the lifetime of all engines
 */
struct ModuleDefine {
    std::string const                     name_;
    std::vector<ClassDefine const*> const refClass_;
    std::vector<EnumDefine const*> const  refEnum_;

    explicit ModuleDefine(
        std::string                     name,
        std::vector<ClassDefine const*> refClass,
        std::vector<EnumDefine const*>  refEnum
    );

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
    std::vector<ClassDefine const*> refClass_;
    std::vector<EnumDefine const*>  refEnum_;

public:
    explicit ModuleDefineBuilder(std::string name) : name_{std::move(name)} {}

    ModuleDefineBuilder& addClass(ClassDefine const& def) {
        refClass_.push_back(&def);
        return *this;
    }

    ModuleDefineBuilder& addEnum(EnumDefine const& def) {
        refEnum_.push_back(&def);
        return *this;
    }

    [[nodiscard]] ModuleDefine build() {
        return ModuleDefine{std::move(name_), std::move(refClass_), std::move(refEnum_)};
    }
};


inline ModuleDefineBuilder defineModule(std::string&& name) { return ModuleDefineBuilder{std::move(name)}; }


} // namespace qjspp