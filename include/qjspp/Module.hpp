#pragma once
#include "qjspp/Binding.hpp"
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

    explicit ModuleDefine(std::string name, std::vector<ClassDefine const*> class_);

private:
    JSModuleDef* init(JsEngine* engine) const;
    friend class JsEngine;
    friend struct ModuleLoader;
};


struct ModuleDefineBuilder {
private:
    std::string                     name_;
    std::vector<ClassDefine const*> class_;

public:
    explicit ModuleDefineBuilder(std::string name);

    ModuleDefineBuilder& exportClass(ClassDefine const& def);

    [[nodiscard]] ModuleDefine build() const;
};


inline ModuleDefineBuilder defineModule(std::string&& name) { return ModuleDefineBuilder{std::move(name)}; }


} // namespace qjspp