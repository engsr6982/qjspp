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
struct ESModuleDefine {
    std::string const                     name_;
    std::vector<ClassDefine const*> const class_;

    explicit ESModuleDefine(std::string name, std::vector<ClassDefine const*> class_);

private:
    JSModuleDef* init(JsEngine* engine) const;
    friend class JsEngine;
};


struct ESModuleDefineBuilder {
private:
    std::string                     name_;
    std::vector<ClassDefine const*> class_;

public:
    explicit ESModuleDefineBuilder(std::string name);

    ESModuleDefineBuilder& exportClass(ClassDefine const& def);

    [[nodiscard]] ESModuleDefine build() const;
};


inline ESModuleDefineBuilder defineESModule(std::string&& name) { return ESModuleDefineBuilder{std::move(name)}; }


} // namespace qjspp