# qjspp - QuickJs(QuickJs-ng) 的现代 C++包装

将 QuickJs(QuickJs-ng) 包装成现代 C++库，并支持类绑定和模块注册。

## 功能特性

## 行为

### `QJSPP_SKIP_INSTANCE_CALL_CHECK_CLASS_DEFINE`

- 默认状态：关闭

启用后，在脚本中调用实例方法、属性时，将不再检查 `WrappedResource::define_` 是否与注册时的 `ClassDefine` 匹配(如果不匹配则会抛出 `JsException` 阻止本次调用)

```cpp
if (kInstanceCallCheckClassDefine && typed->define_ != data2 && typed->define_->extends_ != data2) {
    throw JsException{"This object is not a valid instance of this class."};
}
```

### `QJSPP_CALLBACK_ALWAYS_THROW_IF_NEED_RETURN_VALUE`

- 默认状态：关闭

启用后，在调用绑定的回调函数时(`WrapCallback`)，如果 JS 引发了异常，无法完成调用且需要返回值，则直接抛出 `std::runtime_error`

默认情况下，如果 JS 引发了异常，则会尝试调用返回值 R 的默认构造，如果没有则抛出 `std::runtime_error`

具体行为请查看函数实现: `TypeConverter<std::function<R(Args...)>>::toCpp(Value const& value)`

### `QJSPP_INT64_OR_UINT64_ALWAYS_USE_NUMBER_OF_BIGINT_IN_TYPE_CONVERTER`

- 默认状态：关闭

启用后，在 `TypeConverter<int64/uint64>::toXXX` 时会直接使用 `Number` 类型进行转换，而不是 `BigInt` 类型。

### `QJSPP_DONT_PATCH_CLASS_TO_STRING_TAG`

- 默认状态：关闭

启用后，qjspp 将不会对任何注册到 js 中的实例类、静态类、枚举等，进行修改 `Symbol.toStringTag` 属性

> 默认情况下，qjspp 会将 `Symbol.toStringTag` 属性修改为 `define_->name_`，以方便调试  
> https://developer.mozilla.org/zh-CN/docs/Web/JavaScript/Reference/Global_Objects/Symbol/toStringTag

### `QJSPP_DONT_GENERATE_HELPER_EQLAUS_METHDO`

- 默认状态：关闭

启用后，qjspp 将不会对所有绑定的实例类生成 `$equals` 方法。

> 默认情况下，qjspp 会为所有绑定的实例类生成 `$equals` 方法，用于比较两个实例是否相等。  
> 其行为是：在有可调用的 `operator==` 时，使用 `operator==`，否则直接比较指针地址是否相等  
> 对于 `operator==` qjspp 要求其返回值必须为 `bool` 类型，参数仅支持指针和引用(即: `A&`、`A*`、`A const&`、`A const*`)
