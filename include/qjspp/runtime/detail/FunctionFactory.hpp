#pragma once
#include "qjspp/Forward.hpp"
#include "qjspp/types/Function.hpp"

namespace qjspp {
class JsEngine;
}

namespace qjspp::detail {


struct FunctionFactory {
    FunctionFactory() = delete;

    using RawFunctionData = Value (*)(Arguments const&, void*, void*);

    [[nodiscard]] static Function create(JsEngine& engine, void* data1, void* data2, RawFunctionData rawFn);

private:
    static JSValue newOpaque(JsEngine& engine, void* data);
};


} // namespace qjspp::detail