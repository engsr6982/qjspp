#pragma once
#include <exception>

namespace qjspp {


#define QJSPP_DISABLE_COPY(T)                                                                                          \
    T(const T&)            = delete;                                                                                   \
    T& operator=(const T&) = delete

#define QJSPP_DISABLE_MOVE(T)                                                                                          \
    T(T&&)            = delete;                                                                                        \
    T& operator=(T&&) = delete

#define QJSPP_DISABLE_COPY_MOVE(T)                                                                                     \
    QJSPP_DISABLE_COPY(T);                                                                                             \
    QJSPP_DISABLE_MOVE(T)

#define QJSPP_DISABLE_NEW()                                                                                            \
    static void* operator new(std::size_t)                          = delete;                                          \
    static void* operator new(std::size_t, const std::nothrow_t&)   = delete;                                          \
    static void* operator new[](std::size_t)                        = delete;                                          \
    static void* operator new[](std::size_t, const std::nothrow_t&) = delete


} // namespace qjspp
