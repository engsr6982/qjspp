#pragma once
#include "qjspp/Forward.hpp"
#include "qjspp/bind/TypeConverter.hpp"
#include "qjspp/bind/adapter/AdaptHelper.hpp"
#include "qjspp/concepts/ScriptConcepts.hpp"
#include "qjspp/traits/FunctionTraits.hpp"
#include "qjspp/types/Arguments.hpp"

namespace qjspp::bind::adapter {

template <typename Tuple, std::size_t... Is>
inline decltype(auto) ConvertArgsToTuple(Arguments const& args, std::index_sequence<Is...>);

template <typename Func>
FunctionCallback bindStaticFunction(Func&& func) {
    if constexpr (concepts::JsFunctionCallback<Func>) {
        return std::forward<Func>(func);
    }
    return [f = std::forward<Func>(func)](Arguments const& args) -> Value {
        using Traits       = traits::FunctionTraits<std::decay_t<Func>>;
        using R            = typename Traits::ReturnType;
        using Tuple        = typename Traits::ArgsTuple;
        constexpr size_t N = std::tuple_size_v<Tuple>;

        if (args.length() != N) [[unlikely]] {
            throw JsException(JsException::Type::TypeError, "argument count mismatch");
        }

        if constexpr (std::is_void_v<R>) {
            std::apply(f, ConvertArgsToTuple<Tuple>(args, std::make_index_sequence<N>()));
            return {}; // undefined
        } else {
            decltype(auto) ret = std::apply(f, ConvertArgsToTuple<Tuple>(args, std::make_index_sequence<N>()));
            return ConvertToJs(ret);
        }
    };
}

template <typename... Func>
FunctionCallback bindStaticOverloadedFunction(Func&&... funcs) {
    std::vector functions = {bindStaticFunction(std::forward<Func>(funcs))...};

    // TODO: consider optimizing overload dispatch (e.g. arg-count lookup)
    // if we ever hit cases with >3 overloads. Current linear dispatch is ideal
    // for small sets and keeps the common path fast.

    return [fs = std::move(functions)](Arguments const& args) -> Value {
        for (size_t i = 0; i < sizeof...(Func); ++i) {
            try {
                return std::invoke(fs[i], args);
            } catch (JsException const&) {
                if (i == sizeof...(Func) - 1) [[unlikely]] {
                    throw JsException{JsException::Type::TypeError, "no overload found"};
                }
            }
        }
        return {}; // undefined
    };
}


} // namespace qjspp::bind::adapter