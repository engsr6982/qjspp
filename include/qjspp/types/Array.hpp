#pragma once
#include "ValueBase.hpp"

namespace qjspp {

class Array final {
public:
    QJSPP_DEFINE_VALUE_COMMON(Array);
    explicit Array(size_t size = 0);

    [[nodiscard]] size_t length() const;

    [[nodiscard]] Value get(size_t index) const;

    [[nodiscard]] Value operator[](size_t index) const;

    void set(size_t index, Value const& value);

    void push(Value const& value);

    void clear();
};


} // namespace qjspp