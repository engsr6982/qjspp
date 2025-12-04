#pragma once
#include "ClassDefine.hpp"
#include "EnumDefine.hpp"
#include "qjspp/Forward.hpp"

#include <string>
#include <vector>

namespace qjspp::bind::meta {

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

    // TODO: export fucntion / export let / export const
    struct VarExport {
        std::string const    name_;
        GetterCallback const getter_;
        SetterCallback const setter_; // if nullptr, it's a const
    };
    struct FunctionExport {
        std::string const      name_;
        FunctionCallback const callback_;
    };
    std::vector<VarExport> const      exports_;
    std::vector<FunctionExport> const exportsFunctions_;

    explicit ModuleDefine(
        std::string                     name,
        std::vector<ClassDefine const*> refClass,
        std::vector<EnumDefine const*>  refEnum,
        std::vector<VarExport>          exports,
        std::vector<FunctionExport>     exportsFunctions
    );

    // internal function
    JSModuleDef* init(JsEngine* engine) const;

private:
    void _performExportDeclarations(JsEngine* engine, JSModuleDef* module) const;
    void _performExports(JsEngine* engine, JSContext* ctx, JSModuleDef* module) const;
    friend JsEngine;
};


} // namespace qjspp::bind::meta