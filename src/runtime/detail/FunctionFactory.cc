#include "qjspp/runtime/detail/FunctionFactory.hpp"
#include "qjspp/types/Function.hpp"
#include "qjspp/types/Value.hpp"

namespace qjspp::detail {


Function FunctionFactory::create(JsEngine& engine, void* data1, void* data2, RawFunctionData rawFn) {
    auto context = engine.context_;

    auto op1   = newOpaque(engine, data1);
    auto op2   = newOpaque(engine, data2);
    auto anyCb = newOpaque(engine, reinterpret_cast<void*>(rawFn));

    std::array<JSValue, 3> dataArray{op1, op2, anyCb};

    auto fn = JS_NewCFunctionData(
        context,
        [](JSContext* ctx, JSValueConst thiz, int argc, JSValueConst* argv, int /* magic */, JSValue* data) -> JSValue {
            auto engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));

            auto data1    = JS_GetOpaque(data[0], engine->kPointerClassId);
            auto data2    = JS_GetOpaque(data[1], engine->kPointerClassId);
            auto callback = reinterpret_cast<RawFunctionData>(JS_GetOpaque(data[2], engine->kPointerClassId));

            try {
                auto arguments = Arguments{engine, thiz, argc, argv};
                auto ret       = callback(arguments, data1, data2);
                return JS_DupValue(ctx, Value::extract(ret));
            } catch (JsException const& e) {
                return e.rethrowToEngine();
            }
        },
        0,
        0,
        dataArray.size(),
        dataArray.data()
    );

    JS_FreeValue(context, op1);
    JS_FreeValue(context, op2);
    JS_FreeValue(context, anyCb);

    JsException::check(fn);
    return Value::move<Function>(fn);
}

JSValue FunctionFactory::newOpaque(JsEngine& engine, void* data) {
    if (!data) return JS_UNDEFINED;
    auto dt = JS_NewObjectClass(engine.context_, static_cast<int>(engine.kPointerClassId));
    JsException::check(dt);
    JS_SetOpaque(dt, data);
    return dt;
}


} // namespace qjspp::detail