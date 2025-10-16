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


// clang-format off
#if defined(_WIN32) && defined(QJSPP_SHARED) // Windows DLL
  #ifdef QJSPP_EXPORTS
    #define QJSPP_EXTERN __declspec(dllexport)
  #else
    #define QJSPP_EXTERN __declspec(dllimport)
  #endif
#elif defined(__GNUC__) && defined(QJSPP_SHARED) // POSIX shared library
  #define QJSPP_EXTERN __attribute__((visibility("default")))
#else // static library
  #define QJSPP_EXTERN
#endif
// clang-format on
