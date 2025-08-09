#pragma once

namespace qjspp {


#define QJSPP_DISALLOW_COPY(T)                                                                                         \
    T(const T&)            = delete;                                                                                   \
    T& operator=(const T&) = delete;

#define QJSPP_DISALLOW_MOVE(T)                                                                                         \
    T(T&&)            = delete;                                                                                        \
    T& operator=(T&&) = delete;

#define QJSPP_DISALLOW_COPY_AND_MOVE(T)                                                                                \
    QJSPP_DISALLOW_COPY(T);                                                                                            \
    QJSPP_DISALLOW_MOVE(T);

#define QJSPP_DISALLOW_NEW()                                                                                           \
    static void* operator new(std::size_t)                          = delete;                                          \
    static void* operator new(std::size_t, const std::nothrow_t&)   = delete;                                          \
    static void* operator new[](std::size_t)                        = delete;                                          \
    static void* operator new[](std::size_t, const std::nothrow_t&) = delete;


} // namespace qjspp
