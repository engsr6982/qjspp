#pragma once
#include "qjspp/Types.hpp"
#include <stdexcept>
#include <string_view>
#include <type_traits>


namespace qjspp {


template <typename T>
struct TypeConverter {
    static_assert(
        sizeof(T) == 0,
        "TypeConverter does not have a specialization for type T. Did you forget to specialize the template?"
    );
};

template <typename T>
concept HasTypeConverter = requires { typename TypeConverter<T>; };


// std::function -> Function
template <typename R, typename... Args>
struct TypeConverter<std::function<R(Args...)>> {
    static_assert(
        (HasTypeConverter<Args> && ...),
        "Cannot convert std::function to Function; all parameter types must have a TypeConverter"
    );

    // ! UnSupported: cannot convert function to Value
    static Value toJs(std::function<R(Args...)> const& /* value */);

    static std::function<R(Args...)> toCpp(Value const& value); // impl in Binding.inl
};


namespace internal {

template <typename T>
using RawType = std::remove_pointer_t<std::decay_t<T>>;

template <typename T>
using RawTypeConverter = TypeConverter<RawType<T>>;

namespace detail {

template <typename T, typename = void>
constexpr bool has_toJs_Tref = false;

template <typename T>
constexpr bool has_toJs_Tref<T, std::void_t<decltype(RawTypeConverter<T>::toJs(std::declval<RawType<T>&>()))>> = true;

template <typename T, typename = void>
constexpr bool has_toJs_Tval = false;


template <typename T>
constexpr bool has_toJs_Tval<T, std::void_t<decltype(RawTypeConverter<T>::toJs(std::declval<RawType<T>>()))>> = true;

template <typename T, typename = void>
constexpr bool has_toJs_Tptr = false;

template <typename T>
constexpr bool has_toJs_Tptr<T, std::void_t<decltype(RawTypeConverter<T>::toJs(std::declval<RawType<T>*>()))>> = true;

} // namespace detail

template <typename T, typename = void>
struct IsTypeConverterAvailable : std::false_type {};

template <typename T>
struct IsTypeConverterAvailable<T, std::void_t<decltype(RawTypeConverter<T>::toCpp(std::declval<Value>()))>>
: std::bool_constant<detail::has_toJs_Tref<T> || detail::has_toJs_Tval<T> || detail::has_toJs_Tptr<T>> {};


template <typename T>
constexpr bool IsTypeConverterAvailable_v = IsTypeConverterAvailable<T>::value;


/**
 * @brief C++ 值类型转换器
 * @note 此转换器设计目的是对于某些特殊情况，例如 void foo(std::string_view)
 *       在绑定时，TypeConverter 对字符串的特化是接受 StringLike_v，但返回值统一为 std::string
 *       这种特殊情况下，会导致 ConvertToCpp<std::string_view> 内部类型断言失败:
 * @code using RawConvRet = std::remove_cv_t<std::remove_reference_t<TypedToCppRet<std::string_view>>> // std::string
 * @code std::same_v<RawConvRet, std::string_view> // false
 *
 * @note 为了解决此问题，引入了 CppValueTypeTransformer，用于放宽类型约束
 * @note 需要注意的是 CppValueTypeTransformer 仅放宽了类型约束，实际依然需要特化 TypeConverter<T>
 */
template <typename From, typename To>
struct CppValueTypeTransformer : std::false_type {};

template <>
struct CppValueTypeTransformer<std::string, std::string_view> : std::true_type {};

template <typename From, typename To>
inline constexpr bool CppValueTypeTransformer_v = CppValueTypeTransformer<From, To>::value;


} // namespace internal


template <typename T>
[[nodiscard]] inline Value ConvertToJs(T&& value);

template <typename T>
[[nodiscard]] inline decltype(auto) ConvertToCpp(Value const& value);

} // namespace qjspp

#include "TypeConverter.inl"
