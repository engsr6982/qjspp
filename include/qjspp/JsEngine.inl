#pragma once
#include "JsEngine.hpp"

namespace qjspp {


template <typename T>
std::shared_ptr<T> JsEngine::getData() const {
    return std::static_pointer_cast<T>(userData_);
}


} // namespace qjspp