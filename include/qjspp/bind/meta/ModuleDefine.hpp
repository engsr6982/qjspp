#pragma once
#include "ClassDefine.hpp"
#include "EnumDefine.hpp"
#include "qjspp/Forward.hpp"

#include <string>

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

    explicit ModuleDefine(
        std::string                     name,
        std::vector<ClassDefine const*> refClass,
        std::vector<EnumDefine const*>  refEnum
    );

private:
    JSModuleDef* init(JsEngine* engine) const;

    void _performExportDeclarations(JsEngine* engine, JSModuleDef* module) const;
    void _performExports(JsEngine* engine, JSContext* ctx, JSModuleDef* module) const;
    friend JsEngine;
};


} // namespace qjspp::bind::meta