#pragma once
#include <unordered_map>
#include <utility>
#include <vector>

#include "qjspp/Global.hpp"
#include "qjspp/bind/meta/ClassDefine.hpp"
#include "qjspp/bind/meta/EnumDefine.hpp"
#include "qjspp/bind/meta/MemberDefine.hpp"
#include "qjspp/bind/meta/ModuleDefine.hpp"
#include "qjspp/types/Function.hpp"
#include "qjspp/types/Object.hpp"
#include "qjspp/types/Value.hpp"


namespace qjspp {
class JsEngine;
}

namespace qjspp::detail {


struct BindRegistry final {
    JsEngine&                                                                       engine_;
    std::unordered_map<bind::meta::EnumDefine const*, Object>                       enums_;
    std::unordered_map<bind::meta::ClassDefine const*, Object>                      staticClasses_;
    std::unordered_map<bind::meta::ClassDefine const*, std::pair<JSValue, JSValue>> instanceClasses_; // ctor, prototype
    std::unordered_map<std::string, bind::meta::ModuleDefine const*>                lazyModules_;     // loaded lazily
    std::unordered_map<JSModuleDef*, bind::meta::ModuleDefine const*>               loadedModules_;

    // module export constant、functions cache
    struct ModuleExportCache {
        std::unordered_map<bind::meta::ModuleDefine::ConstantExport const*, Value>    constants_;
        std::unordered_map<bind::meta::ModuleDefine::FunctionExport const*, Function> functions_;

        ModuleExportCache()                                        = default;
        ModuleExportCache(ModuleExportCache&&) noexcept            = default;
        ModuleExportCache& operator=(ModuleExportCache&&) noexcept = default;
        QJSPP_DISABLE_COPY(ModuleExportCache);
    };
    std::unordered_map<JSModuleDef*, ModuleExportCache> moduleExports_;

    explicit BindRegistry(JsEngine& engine);
    ~BindRegistry();

    bool tryRegister(bind::meta::EnumDefine const& enumDef);
    bool tryRegister(bind::meta::ClassDefine const& classDef);
    bool tryRegister(bind::meta::ModuleDefine const& moduleDef);

    Object _buildEnum(bind::meta::EnumDefine const& enumDef) const;

    /**
     * if instance class, returns constructor else returns object
     */
    Value    _registerClass(bind::meta::ClassDefine const& def);
    Function _buildClassConstructor(bind::meta::ClassDefine const& def) const;
    Object   _buildClassPrototype(bind::meta::ClassDefine const& def) const;
    void     _buildClassStatic(bind::meta::StaticMemberDefine const& def, Object& ctor) const;

    void _buildModuleExports(bind::meta::ModuleDefine const& def, JSModuleDef* m);

    // quickjs callbacks
    static void kInstanceClassFinalizer(JSRuntime*, JSValue val);
};


} // namespace qjspp::detail