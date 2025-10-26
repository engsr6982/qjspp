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
    if (!value.isFunction()) [[unlikely]] {
        throw JsException(JsException::Type::TypeError, "expected function");
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
                engine->invokeUnhandledJsException(e, UnhandledExceptionOrigin::Callback);
                if constexpr (!std::is_void_v<R>) return R{};
            } else {
                throw std::runtime_error{
                    "unhandled js exception in callback, qjspp cannot handle the callback return value!"
                };
            }
#else
            if constexpr (std::is_void_v<R>) {
                engine->invokeUnhandledJsException(e, UnhandledExceptionOrigin::Callback);
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
    if constexpr (IsFunctionCallback<std::remove_cvref_t<Func>>) {
        return std::forward<Func>(func);
    }
    return [f = std::forward<Func>(func)](Arguments const& args) -> Value {
        using Traits       = FunctionTraits<std::decay_t<Func>>;
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
    // 原则上，我们需要实现引用机制，来保证 JavaScript 中的对象引用机制。
    // 但是，对于静态属性，我们没有可以与之关联的Js对象(Object)，这就无法创建引用。
    // 所以对于静态属性，qjspp 只能对其进行拷贝传递。
    static_assert(
        std::copyable<concepts::RawType<Ty>>,
        "Static property must be copyable; otherwise, a getter/setter must be specified."
    );
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
        constexpr size_t N = sizeof...(Args);
        if constexpr (N == 0) {
            static_assert(
                HasDefaultConstructor_v<C>,
                "Class C must have a no-argument constructor; otherwise, a constructor must be specified."
            );
            if (args.length() != 0) return nullptr; // Parameter mismatch
            return new C();

        } else {
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
    if constexpr (IsInstanceMethodCallback<std::remove_cvref_t<Func>>) {
        return std::forward<Func>(fn); // 已是标准的回调，直接转发不需要进行绑定
    }
    return [f = std::forward<Func>(fn)](void* inst, const Arguments& args) -> Value {
        using Traits       = FunctionTraits<std::decay_t<Func>>;
        using R            = typename Traits::ReturnType;
        using Tuple        = typename Traits::ArgsTuple;
        constexpr size_t N = std::tuple_size_v<Tuple>;

        if (args.length() != N) [[unlikely]] {
            throw JsException(JsException::Type::TypeError, "argument count mismatch");
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

    // TODO: consider optimizing overload dispatch (e.g. arg-count lookup)
    // if we ever hit cases with >3 overloads. Current linear dispatch is ideal
    // for small sets and keeps the common path fast.

    return [fs = std::move(functions)](void* inst, Arguments const& args) -> Value {
        for (size_t i = 0; i < sizeof...(Func); ++i) {
            try {
                return std::invoke(fs[i], inst, args);
            } catch (JsException const&) {
                if (i == sizeof...(Func) - 1) [[unlikely]] {
                    throw JsException{JsException::Type::TypeError, "no overload found"};
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
std::pair<InstanceGetterCallback, InstanceSetterCallback> bindInstanceProperty(Ty C::* member) {
    static_assert(
        std::copyable<concepts::RawType<Ty>>,
        "bindInstanceProperty only supports copying properties, Ty does not support copying."
    );
    if constexpr (std::is_const_v<Ty>) {
        return {bindInstanceGetter<C>([member](C* inst) -> Ty { return inst->*member; }), nullptr};
    } else {
        return {
            bindInstanceGetter<C>([member](C* inst) -> Ty { return inst->*member; }),
            bindInstanceSetter<C>([member](C* inst, Ty val) -> void { inst->*member = val; })
        };
    }
}

template <typename C, typename Fn>
InstanceGetterCallback bindInstanceGetterRef(Fn&& fn, ClassDefine const* def) {
    return [f = std::forward<Fn>(fn), def](void* inst, Arguments const& arguments) {
        using Ret = FunctionTraits<std::decay_t<Fn>>::ReturnType;
        static_assert(std::is_pointer_v<Ret>, "InstanceGetterRef must return a pointer");

        auto typeId = reflection::getTypeId<concepts::RawType<Ret>>();
        if (typeId != def->typeId_) [[unlikely]] {
            throw JsException{
                JsException::Type::InternalError,
                "Type mismatch, ClassDefine::typeId_ and lambda return value are not the same type"
            };
        }
        if (!arguments.hasThiz()) [[unlikely]] {
            throw JsException{
                JsException::Type::TypeError,
                "Cannot access class member; the current access does not have a valid 'this' reference."
            };
        }

        decltype(auto) result = std::invoke(f, static_cast<C*>(inst)); // const T* / T*

        void* unk = nullptr;
        if constexpr (std::is_const_v<Ret>) {
            unk = const_cast<void*>(static_cast<const void*>(result));
        } else {
            unk = static_cast<void*>(result);
        }
        return arguments.engine()->newInstanceOfView(*def, unk, arguments.thiz());
    };
}

template <typename C, typename Ty>
std::pair<InstanceGetterCallback, InstanceSetterCallback>
bindInstancePropertyRef(Ty C::* member, ClassDefine const* def) {
    using Raw = concepts::RawType<Ty>;
    if constexpr (IsJsValueLike_v<Raw> && std::copyable<Raw>) {
        return bindInstanceProperty<C>(std::forward<Ty C::*>(member)); // Value type, can be copied directly
    } else {
        if constexpr (std::is_const_v<Ty>) {
            return {
                bindInstanceGetterRef<C>([member](C* inst) -> Ty const* { return &(inst->*member); }, def),
                nullptr
            };
        } else {
            return {
                bindInstanceGetterRef<C>([member](C* inst) -> Ty* { return &(inst->*member); }, def),
                bindInstanceSetter<C>([member](C* inst, Ty* val) -> void { inst->*member = *val; })
            };
        }
    }
}


template <typename C>
InstanceMemberDefine::InstanceEqualsCallback bindInstanceEqualsImpl(std::false_type) {
    return [](void* lhs, void* rhs) -> bool { return lhs == rhs; };
}
template <typename C>
InstanceMemberDefine::InstanceEqualsCallback bindInstanceEqualsImpl(std::true_type) {
    return [](void* lhs, void* rhs) -> bool {
        if (!lhs || !rhs) return false;
        return *static_cast<C*>(lhs) == *static_cast<C*>(rhs);
    };
}
template <typename C>
InstanceMemberDefine::InstanceEqualsCallback bindInstanceEquals() {
    // use tag dispatch to fix MSVC pre name lookup or overload resolution
    return bindInstanceEqualsImpl<C>(std::bool_constant<HasEquality<C>>{});
}


} // namespace internal


// impl for Function
template <typename T>
    requires(!IsFunctionCallback<T>)
Function::Function(T&& func) : qjspp::Function(internal::bindStaticFunction(std::forward<T>(func))) {}

template <typename... Fn>
    requires(sizeof...(Fn) > 1 && (!IsFunctionCallback<Fn> && ...))
Function::Function(Fn&&... func) : qjspp::Function(internal::bindStaticOverloadedFunction(std::forward<Fn>(func)...)) {}


} // namespace qjspp
