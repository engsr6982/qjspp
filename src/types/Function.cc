#include "qjspp/types/Function.hpp"

#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/Arguments.hpp"
#include "qjspp/types/String.hpp"
#include "qjspp/types/Value.hpp"

#include <cassert>
#include <memory>

namespace qjspp {

IMPL_QJSPP_DEFINE_VALUE_COMMON(Function);
Function::Function(FunctionCallback callback) {
    auto ptr = std::make_unique<FunctionCallback>(std::move(callback));

    auto& engine = Locker::currentEngineChecked();

    auto fnData = JS_NewObjectClass(engine.context_, static_cast<int>(engine.kFunctionDataClassId));
    JsException::check(fnData);
    JS_SetOpaque(fnData, ptr.release());

    JSValue data[1] = {fnData};

    auto fn = JS_NewCFunctionData(
        engine.context_,
        [](JSContext* ctx, JSValueConst thiz, int argc, JSValueConst* argv, int /*magic*/, JSValue* data) -> JSValue {
            auto kFuncID = JS_GetClassID(data[0]);
            assert(kFuncID != JS_INVALID_CLASS_ID);

            auto cb     = static_cast<FunctionCallback*>(JS_GetOpaque(data[0], static_cast<int>(kFuncID)));
            auto engine = static_cast<JsEngine*>(JS_GetContextOpaque(ctx));
            assert(kFuncID == engine->kFunctionDataClassId);

            try {
                auto result = (*cb)(Arguments{engine, thiz, argc, argv});
                return JS_DupValue(ctx, Value::extract(result));
            } catch (JsException const& e) {
                return e.rethrowToEngine();
            }
        },
        0,
        0,
        1,
        data
    );
    JsException::check(fn);
    JS_FreeValue(engine.context_, fnData);
    val_ = fn;
}

Function Function::newFunction(FunctionCallback&& callback) { return Function{std::move(callback)}; }

Value Function::callImpl(Value const& thiz, int argc, Value const* argv) const {
    auto& engine = Locker::currentEngineChecked();

    static_assert(sizeof(Value) == sizeof(JSValue), "Value and JSValue must have the same size");
    auto* argv_ = reinterpret_cast<JSValue*>(const_cast<Value*>(argv)); // fast

    auto ret = JS_Call(engine.context_, val_, thiz.isObject() ? Value::extract(thiz) : JS_UNDEFINED, argc, argv_);
    JsException::check(ret);
    engine.pumpJobs();
    return Value::move<Value>(ret);
}

Value Function::call(Value const& thiz, std::vector<Value> const& args) const {
    return callImpl(thiz, static_cast<int>(args.size()), args.data());
}

Value Function::call(Value const& thiz, std::initializer_list<Value> args) const {
    return callImpl(thiz, static_cast<int>(args.size()), args.begin());
}

Value Function::call(Value const& thiz, std::span<const Value> args) const {
    return callImpl(thiz, static_cast<int>(args.size()), args.data());
}

Value Function::call() const { return call(Value{}, {}); }

Value Function::callAsConstructor(std::vector<Value> const& args) const {
    auto& engine = Locker::currentEngineChecked();
    if (!JS_IsConstructor(engine.context_, val_)) {
        throw JsException{JsException::Type::TypeError, "Function is not a constructor"};
    }

    static_assert(sizeof(Value) == sizeof(JSValue), "Value and JSValue must have the same size");
    auto* argv = reinterpret_cast<JSValue*>(const_cast<Value*>(args.data())); // fast

    auto res = JS_CallConstructor(engine.context_, val_, static_cast<int>(args.size()), argv);
    JsException::check(res);
    engine.pumpJobs();
    return Value::move<Value>(res);
}

bool Function::isConstructor() const { return JS_IsConstructor(Locker::currentContextChecked(), val_); }


} // namespace qjspp