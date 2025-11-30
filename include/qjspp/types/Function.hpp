#pragma once
#include "Value.hpp"
#include "ValueBase.hpp"
#include "qjspp/concepts/BasicConcepts.hpp"
#include "qjspp/concepts/ScriptConcepts.hpp"

#include <span>

namespace qjspp {

class Function final {
    Value callImpl(Value const& thiz, int argc, Value const* argv) const;

public:
    QJSPP_DEFINE_VALUE_COMMON(Function);
    explicit Function(FunctionCallback callback);

    [[nodiscard]] static Function newFunction(FunctionCallback&& callback);

    template <typename T>
        requires(!concepts::JsFunctionCallback<T>)
    [[nodiscard]] static Function newFunction(T&& func);

    template <typename... Overloads>
        requires(sizeof...(Overloads) > 1)
    [[nodiscard]] static Function newFunction(Overloads&&... overloads);

    Value call(Value const& thiz, std::vector<Value> const& args) const;

    Value call(Value const& thiz, std::initializer_list<Value> args) const;

    Value call(Value const& thiz, std::span<const Value> args) const;

    Value call() const;

    template <typename... Args>
        requires(concepts::JsValueType<std::remove_cvref_t<Args>> && ...)
    Value call(Value const& thiz, Args&&... args) const;

    Value callAsConstructor(std::vector<Value> const& args = {}) const;

    bool isConstructor() const;
};


} // namespace qjspp

#include "Function.inl"
