#pragma once
#include "ValueBase.hpp"

#include "qjspp/concepts/BasicConcepts.hpp"

namespace qjspp {

class Number final {
public:
    QJSPP_DEFINE_VALUE_COMMON(Number);
    explicit Number(double d);
    explicit Number(float f);
    explicit Number(int i32);
    explicit Number(int64_t i64);

    [[nodiscard]] float   getFloat() const;
    [[nodiscard]] double  getDouble() const;
    [[nodiscard]] int     getInt32() const;
    [[nodiscard]] int64_t getInt64() const;

    template <concepts::NumberLike T>
    [[nodiscard]] static Number newNumber(T num) {
        return Number{static_cast<double>(num)};
    }
};

} // namespace qjspp