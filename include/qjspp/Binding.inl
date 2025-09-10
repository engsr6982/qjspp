#pragma once
#include "Binding.hpp"
#include "qjspp/Concepts.hpp"
#include "qjspp/Global.hpp"
#include "qjspp/JsEngine.hpp"
#include "qjspp/JsException.hpp"
#include "qjspp/JsScope.hpp"
#include "qjspp/TypeConverter.hpp"
#include "qjspp/Types.hpp"
#include "qjspp/Values.hpp"
#include <array>
#include <cassert>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>


namespace qjspp {

namespace internal {

// Primary template: redirect to operator()
template <typename T>
struct FunctionTraits : FunctionTraits<decltype(&T::operator())> {};

// 普通函数 / 函数指针
template <typename R, typename... Args>
struct FunctionTraits<R (*)(Args...)> {
    using ReturnType          = R;
    using ArgsTuple           = std::tuple<Args...>;
    static constexpr size_t N = sizeof...(Args);
};

template <typename R, typename... Args>
struct FunctionTraits<R(Args...)> : FunctionTraits<R (*)(Args...)> {};

// std::function
template <typename R, typename... Args>
struct FunctionTraits<std::function<R(Args...)>> : FunctionTraits<R (*)(Args...)> {};

// 成员函数指针（包括 const / noexcept 等）
template <typename C, typename R, typename... Args>
struct FunctionTraits<R (C::*)(Args...)> {
    using ReturnType          = R;
    using ArgsTuple           = std::tuple<Args...>;
    static constexpr size_t N = sizeof...(Args);
};

template <typename C, typename R, typename... Args>
struct FunctionTraits<R (C::*)(Args...) const> : FunctionTraits<R (C::*)(Args...)> {};

template <typename C, typename R, typename... Args>
struct FunctionTraits<R (C::*)(Args...) const noexcept> : FunctionTraits<R (C::*)(Args...) const> {};

template <typename T>
struct DecayedFunctionTraits : FunctionTraits<std::remove_cvref_t<T>> {};


// 获取参数个数
template <typename T>
constexpr size_t ArgsCount_v = FunctionTraits<T>::N;

// 获取第N个参数类型
template <typename T, size_t N>
using ArgNType = std::tuple_element_t<N, typename DecayedFunctionTraits<T>::ArgsTuple>;

// 获取首个参数类型
template <typename T>
using ArgType_t = ArgNType<T, 0>;


// 辅助模板：根据参数类型决定存储类型
// template <typename T>
// using TupleElementType = std::conditional_t<
//     std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>,
//     T,                     // 保持 T&（非 const 左值引用）
//     std::remove_cvref_t<T> // 其他情况按值存储（移除 cv 和引用）
//     >;

// 辅助模板：根据 ConvertToCpp<T> 的返回类型决定存储类型
template <typename T>
using ConvertReturnType = decltype(ConvertToCpp<T>(std::declval<Arguments const&>()[std::declval<size_t>()]));

template <typename T>
using TupleElementType = std::conditional_t<
    std::is_lvalue_reference_v<ConvertReturnType<T>>,
    ConvertReturnType<T>,                     // 保持返回的引用类型（U& 或 const U&）
    std::remove_cvref_t<ConvertReturnType<T>> // 否则按值存储
    >;

// 转换参数类型
template <typename Tuple, std::size_t... Is>
inline decltype(auto) ConvertArgsToTuple(Arguments const& args, std::index_sequence<Is...>) {
    // [1]deprecated: 使用 std::make_tuple 进行全值传递
    //  - std::make_tuple 会强制将所有元素按值存储，即使原类型是引用
    //  - 若 Tuple 中包含引用类型（如 T&），此处会被 decay 为值类型，导致类型不匹配
    // return std::make_tuple(ConvertToCpp<std::tuple_element_t<Is, Tuple>>(args[Is])...);

    // [2]deprecated: 直接使用目标 Tuple 类型构造
    //  - 当 Tuple 元素类型为 const T& 时，可能绑定到 ConvertToCpp 返回的临时对象（右值）
    //  - 临时对象在当前语句结束后销毁，导致元组中存储的引用变为悬空引用
    // return Tuple(ConvertToCpp<std::tuple_element_t<Is, Tuple>>(args[Is])...);

    // [3]deprecated: 使用 TupleElementType 辅助模板，将引用类型转换为值类型
    //  - 此方法可避免 [1] 和 [2] 的问题，但依然会造成不必要的拷贝
    // using ResultTuple = std::tuple<TupleElementType<std::tuple_element_t<Is, Tuple>>...>;
    // return ResultTuple(ConvertToCpp<std::tuple_element_t<Is, Tuple>>(args[Is])...);

    using ResultTuple = std::tuple<TupleElementType<std::tuple_element_t<Is, Tuple>>...>;
    return ResultTuple(ConvertToCpp<std::tuple_element_t<Is, Tuple>>(args[Is])...);
}


} // namespace internal


// Function -> std::function
template <typename R, typename... Args>
inline decltype(auto) WrapCallback(Value const& value) {
    if (!value.isFunction()) {
        throw JsException("expected function");
    }
    auto engine = JsScope::currentEngine();
    auto scoped = ScopedValue{engine, value};
    return [sc = std::move(scoped)](Args&&... args) -> R {
        auto    engine = sc.engine();
        JsScope lock{engine};

        auto cb = sc.value().asFunction();
        try {
            std::array<Value, sizeof...(Args)> argv{ConvertToJs(std::forward<Args>(args))...};
            if constexpr (std::is_void_v<R>) {
                cb.call(Undefined{}, argv);
            } else {
                return ConvertToCpp<R>(cb.call(Undefined{}, argv));
            }
        } catch (JsException const& e) {
#ifndef QJSPP_CALLBACK_ALWAYS_THROW_IF_NEED_RETURN_VALUE
            if constexpr (std::is_void_v<R> || std::is_default_constructible_v<R>) {
                engine->invokeUnhandledJsExceptionCallback(e, UnhandledExceptionOrigin::Callback);
                if constexpr (!std::is_void_v<R>) return R{};
            } else {
                throw std::runtime_error{
                    "unhandled js exception in callback, qjspp cannot handle the callback return value!"
                };
            }
#else
            if constexpr (std::is_void_v<R>) {
                engine->invokeUnhandledJsExceptionCallback(e, UnhandledExceptionOrigin::Callback);
            } else {
                throw std::runtime_error{
                    "unhandled js exception in callback, qjspp cannot handle the callback return value!"
                };
            }
#endif
        }
    };
}
template <typename R, typename... Args>
std::function<R(Args...)> TypeConverter<std::function<R(Args...)>>::toCpp(Value const& value) {
    return WrapCallback<R, Args...>(value);
}


namespace internal {

// static
template <typename Func>
FunctionCallback bindStaticFunction(Func&& func) {
    return [f = std::forward<Func>(func)](Arguments const& args) -> Value {
        using Traits       = FunctionTraits<std::decay_t<Func>>;
        using R            = typename Traits::ReturnType;
        using Tuple        = typename Traits::ArgsTuple;
        constexpr size_t N = std::tuple_size_v<Tuple>;

        if (args.length() != N) {
            throw JsException("argument count mismatch");
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
    return [fs = std::move(functions)](Arguments const& args) -> Value {
        for (size_t i = 0; i < sizeof...(Func); ++i) {
            try {
                return std::invoke(fs[i], args);
            } catch (JsException const&) {
                if (i == sizeof...(Func) - 1) {
                    throw JsException{"no overload found"};
                }
            }
        }
        return {}; // undefined
    };
}

// Fn: () -> Ty
template <typename Fn>
GetterCallback bindStaticGetter(Fn&& fn) {
    return [f = std::forward<Fn>(fn)]() -> Value { return ConvertToJs(std::invoke(f)); };
}

// Fn: (Ty) -> void
template <typename Fn>
SetterCallback bindStaticSetter(Fn&& fn) {
    using Ty = ArgType_t<Fn>;
    return [f = std::forward<Fn>(fn)](Value const& value) { std::invoke(f, ConvertToCpp<Ty>(value)); };
}

template <typename Ty>
std::pair<GetterCallback, SetterCallback> bindStaticProperty(Ty* p) {
    if constexpr (std::is_const_v<Ty>) {
        return {
            bindStaticGetter([p]() -> Ty { return *p; }),
            nullptr // const
        };
    } else {
        return {bindStaticGetter([p]() -> Ty { return *p; }), bindStaticSetter([p](Ty val) { *p = val; })};
    }
}


// Instance
template <typename C, typename... Args>
InstanceConstructor bindInstanceConstructor() {
    return [](Arguments const& args) -> void* {
        if constexpr (sizeof...(Args) == 0) {
            static_assert(
                HasDefaultConstructor_v<C>,
                "Class C must have a no-argument constructor; otherwise, a constructor must be specified."
            );
            if (args.length() != 0) return nullptr; // Parameter mismatch
            return new C();

        } else {
            constexpr size_t N = sizeof...(Args);
            if (args.length() != N) return nullptr; // Parameter mismatch

            using Tuple = std::tuple<Args...>;

            auto parameters = ConvertArgsToTuple<Tuple>(args, std::make_index_sequence<N>());
            return std::apply(
                [](auto&&... unpackedArgs) { return new C(std::forward<decltype(unpackedArgs)>(unpackedArgs)...); },
                std::move(parameters)
            );
        }
    };
}

template <typename C, typename Func>
InstanceMethodCallback bindInstanceMethod(Func&& fn) {
    return [f = std::forward<Func>(fn)](void* inst, const Arguments& args) -> Value {
        using Traits       = FunctionTraits<std::decay_t<Func>>;
        using R            = typename Traits::ReturnType;
        using Tuple        = typename Traits::ArgsTuple;
        constexpr size_t N = std::tuple_size_v<Tuple>;

        if (args.length() != N) {
            throw JsException("argument count mismatch");
        }

        auto typedInstance = static_cast<C*>(inst);

        if constexpr (std::is_void_v<R>) {
            std::apply(
                [typedInstance, &f](auto&&... unpackedArgs) {
                    (typedInstance->*f)(std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
                },
                ConvertArgsToTuple<Tuple>(args, std::make_index_sequence<N>())
            );
            return {}; // undefined
        } else {
            decltype(auto) ret = std::apply(
                [typedInstance, &f](auto&&... unpackedArgs) -> R {
                    return (typedInstance->*f)(std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
                },
                ConvertArgsToTuple<Tuple>(args, std::make_index_sequence<N>())
            );
            // 特殊情况，对于 Builder 模式，返回 this
            if constexpr (std::is_same_v<R, C&>) {
                assert(args.hasThiz() && "this is required for Builder pattern");
                return args.thiz();
            } else {
                return ConvertToJs(ret);
            }
        }
    };
}

template <typename C, typename... Func>
InstanceMethodCallback bindInstanceOverloadedMethod(Func&&... funcs) {
    std::vector functions = {bindInstanceMethod<C>(std::forward<Func>(funcs))...};
    return [fs = std::move(functions)](void* inst, Arguments const& args) -> Value {
        for (size_t i = 0; i < sizeof...(Func); ++i) {
            try {
                return std::invoke(fs[i], inst, args);
            } catch (JsException const&) {
                if (i == sizeof...(Func) - 1) {
                    throw JsException{"no overload found"};
                }
            }
        }
        return {}; // undefined
    };
}

// Fn: (C*) -> Ty
template <typename C, typename Fn>
InstanceGetterCallback bindInstanceGetter(Fn&& fn) {
    return [f = std::forward<Fn>(fn)](void* inst, Arguments const& /* args */) -> Value {
        return ConvertToJs(std::invoke(f, static_cast<C*>(inst)));
    };
}

// Fn: (void* inst, Ty val) -> void
template <typename C, typename Fn>
InstanceSetterCallback bindInstanceSetter(Fn&& fn) {
    using Ty = ArgNType<Fn, 1>; // (void* inst, Ty val)
    return [f = std::forward<Fn>(fn)](void* inst, Arguments const& args) -> void {
        std::invoke(f, static_cast<C*>(inst), ConvertToCpp<Ty>(args[0]));
    };
}

template <typename C, typename Ty>
std::pair<InstanceGetterCallback, InstanceSetterCallback> bindInstanceProperty(Ty C::* prop) {
    if constexpr (std::is_const_v<Ty>) {
        return {
            bindInstanceGetter<C>([prop](C* inst) -> Ty { return inst->*prop; }),
            nullptr // const
        };
    } else {
        return {
            bindInstanceGetter<C>([prop](C* inst) -> Ty { return inst->*prop; }),
            bindInstanceSetter<C>([prop](C* inst, Ty val) -> void { inst->*prop = val; })
        };
    }
}


template <typename C>
InstanceDefine::InstanceEqualsCallback bindInstanceEqualsImpl(std::false_type) {
    return [](void* lhs, void* rhs) -> bool { return lhs == rhs; };
}
template <typename C>
InstanceDefine::InstanceEqualsCallback bindInstanceEqualsImpl(std::true_type) {
    return [](void* lhs, void* rhs) -> bool {
        if (!lhs || !rhs) return false;
        return *static_cast<C*>(lhs) == *static_cast<C*>(rhs);
    };
}
template <typename C>
InstanceDefine::InstanceEqualsCallback bindInstanceEquals() {
    // use tag dispatch to fix MSVC pre name lookup or overload resolution
    return bindInstanceEqualsImpl<C>(std::bool_constant<HasEquality<C>>{});
}


} // namespace internal

} // namespace qjspp
