#pragma once

#include <Std/Types.hpp>

constexpr usize KiB = 1024;
constexpr usize MiB = KiB * KiB;

#ifndef TEST
constexpr void* operator new(usize count, void* ptr)
{
    return ptr;
}
constexpr void* operator new[](usize count, void* ptr)
{
    return ptr;
}
void* operator new(usize);
void* operator new[](usize);
void operator delete(void*);
void operator delete[](void*);
#endif

template<typename T>
struct RemoveReference {
    using Type = T;
};
template<typename T>
struct RemoveReference<T&> {
    using Type = T;
};

template<typename T>
constexpr typename RemoveReference<T>::Type&& move(T&& value)
{
    return static_cast<typename RemoveReference<T>::Type&&>(value);
}

template<typename T>
constexpr T&& forward(typename RemoveReference<T>::Type& value)
{
    return static_cast<T&&>(value);
}
template<typename T>
constexpr T&& forward(typename RemoveReference<T>::Type&& value)
{
    return static_cast<T&&>(value);
}

template<typename T, typename U = T>
constexpr T exchange(T& obj, U&& new_value)
{
    T old_value = move(obj);
    obj = forward<U>(new_value);
    return move(old_value);
}

template<typename T>
constexpr void swap(T& lhs, T& rhs)
{
    T value = move(lhs);
    lhs = move(rhs);
    rhs = move(value);
}

extern "C"
inline void strlcpy(char *destination, const char *source, usize size) noexcept
{
    if (size >= 1) {
        __builtin_strncpy(destination, source, size - 1);
        destination[size - 1] = 0;
    }
}
extern "C"
void* memcpy(void *destination, const void *source, usize count) noexcept;

template<typename T>
constexpr T max(T a, T b)
{
    return a >= b ? a : b;
}

template<typename T>
constexpr T min(T a, T b)
{
    return a <= b ? a : b;
}

inline usize round_to_power_of_two(usize value)
{
    return 1 << (32 - __builtin_clz(value));
}

namespace Std {
    enum class IterationDecision {
        Continue,
        Break,
    };

    class Path;

    template<typename... Parameters>
    void dbgln(const char *fmtstr, const Parameters&...);

    [[noreturn]]
    void crash(const char *format, const char *condition, const char *file, usize line);
}

#define ASSERT(condition) ((condition) ? (void)0 : ::Std::crash("ASSERT(%condition)\n%file:%line\n", #condition, __FILE__, __LINE__))
#define VERIFY(condition) ((condition) ? (void)0 : ::Std::crash("VERIFY(%condition)\n%file:%line\n", #condition, __FILE__, __LINE__))

#define ASSERT_NOT_REACHED() ::Std::crash("ASSERT_NOT_REACHED():\n%file:%line\n", "", __FILE__, __LINE__)
#define VERIFY_NOT_REACHED() ::Std::crash("VERIFY_NOT_REACHED():\n%file:%line\n", "", __FILE__, __LINE__)

#define FIXME() ::Std::crash("FIXME()\n%file:%line\n", "", __FILE__, __LINE__)
#define FIXME_ASSERT(condition) ((condition) ? (void)0 : ::Std::crash("FIXME(%condition)\n%file:%line\n", #condition, __FILE__, __LINE__))
