#pragma once
#include "ValueBase.hpp"

namespace qjspp {

class BigInt final {
public:
    QJSPP_DEFINE_VALUE_COMMON(BigInt);
    explicit BigInt(int64_t i64);
    explicit BigInt(uint64_t u64);

    [[nodiscard]] int64_t  getInt64() const;
    [[nodiscard]] uint64_t getUInt64() const;

    [[nodiscard]] static BigInt newBigInt(int64_t i64);
    [[nodiscard]] static BigInt newBigInt(uint64_t u64);
};

} // namespace qjspp