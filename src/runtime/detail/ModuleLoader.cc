#include "qjspp/runtime/detail/ModuleLoader.hpp"
#include "qjspp/bind/meta/ModuleDefine.hpp"
#include "qjspp/runtime/JsEngine.hpp"

#include <fstream>


namespace qjspp::detail {

bool ModuleLoader::setModuleMainFlag(JSContext* ctx, JSModuleDef* module, bool isMain) {
    auto meta = JS_GetImportMeta(ctx, module);
    if (JS_IsException(meta)) {
        return false;
    }
    JS_DefinePropertyValueStr(ctx, meta, "main", JS_NewBool(ctx, isMain), JS_PROP_C_W_E);
    JS_FreeValue(ctx, meta);
    return true;
}
bool ModuleLoader::setModuleUrl(JSContext* ctx, JSModuleDef* module, std::string_view url) {
    auto meta = JS_GetImportMeta(ctx, module);
    if (JS_IsException(meta)) {
        return false;
    }
    JS_DefinePropertyValueStr(ctx, meta, "url", JS_NewString(ctx, url.data()), JS_PROP_C_W_E);
    JS_FreeValue(ctx, meta);
    return true;
}
bool ModuleLoader::setModuleMeta(JSContext* ctx, JSModuleDef* module, std::string_view url, bool isMain) {
    return setModuleUrl(ctx, module, url) && setModuleMainFlag(ctx, module, isMain);
}

std::optional<std::filesystem::path> ModuleLoader::resolveWithFallback(const std::filesystem::path& p) {
    if (is_regular_file(p)) return p;

    auto js  = p;
    js      += ".js";
    if (is_regular_file(js)) return js;

    auto mjs  = p;
    mjs      += ".mjs";
    if (is_regular_file(mjs)) return mjs;

    return std::nullopt;
}

/* ModuleLoader impl */
char* ModuleLoader::normalize(JSContext* ctx, const char* base, const char* name, void* opaque) {
    auto* engine = static_cast<JsEngine*>(opaque);
    // std::cout << "[normalize] base: " << base << ", name: " << name << std::endl;

    std::string_view baseView{base};
    std::string_view nameView{name};

    // 1) 各种奇奇怪怪的 <eval>
    if (baseView.starts_with('<') && baseView.ends_with('>')) {
        return js_strdup(ctx, name);
    }

    // 2) 检查是否是原生模块
    if (engine->nativeModules_.contains(name)) {
        return js_strdup(ctx, name);
    }

    // 3) 检查是否是标准文件协议开头
    if (std::strncmp(name, kFilePrefix.data(), kFilePrefix.size()) == 0) {
        return js_strdup(ctx, name);
    }

    // 处理相对路径（./ 或 ../）
    std::string baseStr(base);
    if (baseStr.rfind(kFilePrefix, 0) == 0) {
        baseStr.erase(0, kFilePrefix.size()); // 去掉 file://
    }

    // 获取 base 所在目录
    std::filesystem::path basePath(baseStr);
    basePath = basePath.parent_path();

    // 拼接相对路径
    std::filesystem::path targetPath = basePath / name;

    // 规范化（处理 .. 和 .）
    std::error_code ec;
    targetPath = std::filesystem::weakly_canonical(targetPath, ec);
    if (ec) {
        JS_ThrowReferenceError(ctx, "Invalid path: %s", name);
        return nullptr;
    }

    auto resolved = resolveWithFallback(targetPath);
    if (!resolved) {
        JS_ThrowReferenceError(ctx, "Cannot resolve module: %s", name);
        return nullptr;
    }

    // 重新加上 file:// 前缀
    std::string fullUrl = std::string(kFilePrefix) + (*resolved).generic_string();

    return js_strdup(ctx, fullUrl.c_str());
}

JSModuleDef* ModuleLoader::loader(JSContext* ctx, const char* canonical, void* opaque) {
    auto* engine = static_cast<JsEngine*>(opaque);
    // std::cout << "[loader] canonical: " << canonical << std::endl;

    // 1) 检查是否是原生模块
    auto iter = engine->nativeModules_.find(canonical);
    if (iter != engine->nativeModules_.end()) {
        auto module = iter->second;
        return module->init(engine);
    }

    // 2) file:// 协议 => 读取文件并编译
    if (std::strncmp(canonical, kFilePrefix.data(), kFilePrefix.size()) == 0) {
        std::string   path = canonical + kFilePrefix.size();
        std::ifstream ifs(path);
        if (!ifs) {
            JS_ThrowReferenceError(ctx, "Module file not found: %s", path.c_str());
            return nullptr;
        }
        std::string source((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        // 编译模块
        JSValue result =
            JS_Eval(ctx, source.c_str(), source.size(), canonical, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        if (JS_IsException(result)) return nullptr;

        // 更新 import meta
        auto* m = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(result));
        if (!setModuleMeta(ctx, m, canonical, false)) {
            JS_FreeValue(ctx, result);
            return nullptr;
        }
        JS_FreeValue(ctx, result);
        return m;
    }

    return nullptr;
}


} // namespace qjspp::detail