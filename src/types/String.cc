#include "qjspp/types/String.hpp"
#include "qjspp/Forward.hpp"
#include "qjspp/runtime/JsException.hpp"
#include "qjspp/runtime/Locker.hpp"
#include "qjspp/types/Value.hpp"


namespace qjspp {

IMPL_QJSPP_DEFINE_VALUE_COMMON(String);
String::String(std::string_view utf8) {
    auto cstr = JS_NewStringLen(Locker::currentContextChecked(), utf8.data(), utf8.size());
    JsException::check(cstr);
    val_ = cstr;
}
String::String(std::string const& value) : String{std::string_view{value}} {}
String::String(char const* value) : String{std::string_view{value}} {}

std::string String::value() const {
    auto   ctx = Locker::currentContextChecked();
    size_t len{};
    auto   cstr = JS_ToCStringLen(ctx, &len, val_);
    if (cstr == nullptr) [[unlikely]] {
        throw JsException{JsException::Type::InternalError, "Failed to convert String to std::string"};
    }
    std::string copy{cstr, len};
    JS_FreeCString(ctx, cstr);
    return copy;
}


} // namespace qjspp