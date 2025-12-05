#pragma once
#include "qjspp/Forward.hpp"
#include "qjspp/bind/meta/ClassDefine.hpp"
#include "qjspp/bind/meta/EnumDefine.hpp"
#include "qjspp/bind/meta/ModuleDefine.hpp"

#include "qjspp/bind/adapter/FunctionAdapter.hpp"
#include "qjspp/concepts/ScriptConcepts.hpp"

#include <string>
#include <utility>
#include <vector>


namespace qjspp::bind {


struct ModuleDefineBuilder {
private:
    std::string                           name_;
    std::vector<meta::ClassDefine const*> refClass_;
    std::vector<meta::EnumDefine const*>  refEnum_;

    std::vector<meta::ModuleDefine::ConstantExport> constantExports_;
    std::vector<meta::ModuleDefine::FunctionExport> functionExports_;

public:
    explicit ModuleDefineBuilder(std::string name) : name_{std::move(name)} {}

    ModuleDefineBuilder& addClass(bind::meta::ClassDefine const& def) {
        refClass_.push_back(&def);
        return *this;
    }

    ModuleDefineBuilder& addEnum(bind::meta::EnumDefine const& def) {
        refEnum_.push_back(&def);
        return *this;
    }

    ModuleDefineBuilder& exportConstant(std::string const& name, GetterCallback getter) {
        constantExports_.emplace_back(name, std::move(getter));
        return *this;
    }

    ModuleDefineBuilder& exportFunction(std::string const& name, FunctionCallback callback) {
        functionExports_.emplace_back(name, std::move(callback));
        return *this;
    }

    template <typename T>
        requires(!concepts::JsFunctionCallback<T>)
    ModuleDefineBuilder& exportFunction(std::string const& name, T&& callback) {
        auto fn = adapter::bindStaticFunction(std::forward<T>(callback));
        functionExports_.emplace_back(name, std::move(fn));
        return *this;
    }

    template <typename... Overloads>
        requires(sizeof...(Overloads) > 1)
    ModuleDefineBuilder& exportFunction(std::string const& name, Overloads&&... callbacks) {
        auto fns = adapter::bindStaticOverloadedFunction(std::forward<Overloads>(callbacks)...);
        functionExports_.emplace_back(name, std::move(fns));
        return *this;
    }

    [[nodiscard]] meta::ModuleDefine build() {
        return meta::ModuleDefine{
            std::move(name_),
            std::move(refClass_),
            std::move(refEnum_),
            std::move(constantExports_),
            std::move(functionExports_)
        };
    }
};

inline ModuleDefineBuilder defineModule(std::string&& name) { return ModuleDefineBuilder{std::move(name)}; }

} // namespace qjspp::bind