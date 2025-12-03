#pragma once
#include "qjspp/types/Object.hpp"
#include "qjspp/types/Value.hpp"

#include <string_view>

namespace qjspp {


template <concepts::StringLike T>
bool Object::has(T const& key) const {
    return this->has(std::string_view{key});
}

template <concepts::StringLike T>
Value Object::get(T const& key) const {
    return this->get(std::string_view{key});
}

template <concepts::StringLike T>
void Object::set(T const& key, Value const& value) {
    this->set(std::string_view{key}, value);
}

template <concepts::StringLike T>
void Object::remove(T const& key) {
    this->remove(std::string_view{key});
}


} // namespace qjspp