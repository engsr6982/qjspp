#pragma once
#include <filesystem>
#include <optional>
#include <string_view>

#include "qjspp/Forward.hpp"

namespace qjspp::detail {


struct ModuleLoader {
    static constexpr std::string_view kFilePrefix = "file://";

    static bool setModuleMainFlag(JSContext* ctx, JSModuleDef* module, bool isMain);
    static bool setModuleUrl(JSContext* ctx, JSModuleDef* module, std::string_view url);
    static bool setModuleMeta(JSContext* ctx, JSModuleDef* module, std::string_view url, bool isMain);

    static std::optional<std::filesystem::path> resolveWithFallback(const std::filesystem::path& p);

    // quickjs loader callback
    static char*        normalize(JSContext* ctx, const char* base, const char* name, void* opaque);
    static JSModuleDef* loader(JSContext* ctx, const char* canonical, void* opaque);
};


} // namespace qjspp::detail