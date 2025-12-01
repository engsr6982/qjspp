#pragma once
#include "qjspp/bind/meta/ClassDefine.hpp"
#include "qjspp/bind/meta/EnumDefine.hpp"
#include "qjspp/bind/meta/ModuleDefine.hpp"

#include <string>
#include <vector>


namespace qjspp::bind {


struct ModuleDefineBuilder {
private:
    std::string                           name_;
    std::vector<meta::ClassDefine const*> refClass_;
    std::vector<meta::EnumDefine const*>  refEnum_;

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

    [[nodiscard]] meta::ModuleDefine build() {
        return meta::ModuleDefine{std::move(name_), std::move(refClass_), std::move(refEnum_)};
    }
};

inline ModuleDefineBuilder defineModule(std::string&& name) { return ModuleDefineBuilder{std::move(name)}; }

} // namespace qjspp::bind