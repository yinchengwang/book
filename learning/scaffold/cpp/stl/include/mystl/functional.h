/*
 * functional.h - 函数对象和哈希特化
 *
 * 对标 <functional>，纯手写实现
 * 包含：比较/算术/逻辑函数对象、hash 特化、identity、reference_wrapper
 */

#ifndef MYSTL_FUNCTIONAL_H
#define MYSTL_FUNCTIONAL_H

#include "type_traits.h"
#include <cstddef>  // size_t
#include <cstdint>  // uint64_t

MYSTL_NAMESPACE_BEGIN

// ============================================================
// 1. 比较函数对象
// ============================================================

template <typename T = void>
struct less {
    constexpr bool operator()(const T& a, const T& b) const { return a < b; }
};
template <typename T = void>
struct greater {
    constexpr bool operator()(const T& a, const T& b) const { return a > b; }
};
template <typename T = void>
struct equal_to {
    constexpr bool operator()(const T& a, const T& b) const { return a == b; }
};
template <typename T = void>
struct not_equal_to {
    constexpr bool operator()(const T& a, const T& b) const { return a != b; }
};
template <typename T = void>
struct less_equal {
    constexpr bool operator()(const T& a, const T& b) const { return a <= b; }
};
template <typename T = void>
struct greater_equal {
    constexpr bool operator()(const T& a, const T& b) const { return a >= b; }
};

// void 特化（C++14 透明比较器）
template <>
struct less<void> {
    template <typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const -> decltype(mystl::forward<T>(a) < mystl::forward<U>(b)) {
        return mystl::forward<T>(a) < mystl::forward<U>(b);
    }
};
template <>
struct greater<void> {
    template <typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const -> decltype(mystl::forward<T>(a) > mystl::forward<U>(b)) {
        return mystl::forward<T>(a) > mystl::forward<U>(b);
    }
};
template <>
struct equal_to<void> {
    template <typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const -> decltype(mystl::forward<T>(a) == mystl::forward<U>(b)) {
        return mystl::forward<T>(a) == mystl::forward<U>(b);
    }
};


// ============================================================
// 2. 算术函数对象
// ============================================================

template <typename T = void>
struct plus {
    constexpr T operator()(const T& a, const T& b) const { return a + b; }
};
template <typename T = void>
struct minus {
    constexpr T operator()(const T& a, const T& b) const { return a - b; }
};
template <typename T = void>
struct multiplies {
    constexpr T operator()(const T& a, const T& b) const { return a * b; }
};
template <typename T = void>
struct divides {
    constexpr T operator()(const T& a, const T& b) const { return a / b; }
};
template <typename T = void>
struct modulus {
    constexpr T operator()(const T& a, const T& b) const { return a % b; }
};
template <typename T = void>
struct negate {
    constexpr T operator()(const T& a) const { return -a; }
};


// ============================================================
// 3. 逻辑函数对象
// ============================================================

template <typename T = void>
struct logical_and {
    constexpr bool operator()(const T& a, const T& b) const { return a && b; }
};
template <typename T = void>
struct logical_or {
    constexpr bool operator()(const T& a, const T& b) const { return a || b; }
};
template <typename T = void>
struct logical_not {
    constexpr bool operator()(const T& a) const { return !a; }
};


// ============================================================
// 4. 身份和选择函数对象
// ============================================================

// identity (C++20 但很多实现需要)
struct identity {
    template <typename T>
    constexpr T&& operator()(T&& t) const noexcept {
        return mystl::forward<T>(t);
    }
};

// select1st — 从 pair 中提取 first（libstdc++ 内部使用）
struct select1st {
    template <typename Pair>
    constexpr typename Pair::first_type operator()(const Pair& p) const {
        return p.first;
    }
};


// ============================================================
// 5. Hash 函数对象
// ============================================================

template <typename T>
struct hash {};

// 整数哈希：直接转换
#define MYSTL_HASH_INTEGRAL(type) \
    template <> struct hash<type> { \
        size_t operator()(type val) const noexcept { \
            return static_cast<size_t>(val); \
        } \
    };

MYSTL_HASH_INTEGRAL(bool)
MYSTL_HASH_INTEGRAL(char)
MYSTL_HASH_INTEGRAL(signed char)
MYSTL_HASH_INTEGRAL(unsigned char)
MYSTL_HASH_INTEGRAL(short)
MYSTL_HASH_INTEGRAL(unsigned short)
MYSTL_HASH_INTEGRAL(int)
MYSTL_HASH_INTEGRAL(unsigned int)
MYSTL_HASH_INTEGRAL(long)
MYSTL_HASH_INTEGRAL(unsigned long)
MYSTL_HASH_INTEGRAL(long long)
MYSTL_HASH_INTEGRAL(unsigned long long)
MYSTL_HASH_INTEGRAL(char16_t)
MYSTL_HASH_INTEGRAL(char32_t)
MYSTL_HASH_INTEGRAL(wchar_t)

#undef MYSTL_HASH_INTEGRAL

// 浮点哈希
template <>
struct hash<float> {
    size_t operator()(float val) const noexcept {
        return val == 0.0f ? 0 : *reinterpret_cast<unsigned*>(&val);
    }
};
template <>
struct hash<double> {
    size_t operator()(double val) const noexcept {
        return val == 0.0 ? 0 : *reinterpret_cast<unsigned long long*>(&val);
    }
};

// 指针哈希
template <typename T>
struct hash<T*> {
    size_t operator()(T* ptr) const noexcept {
        return reinterpret_cast<size_t>(ptr);
    }
};

// const char* 哈希（DJB2 算法）
template <>
struct hash<const char*> {
    size_t operator()(const char* s) const noexcept {
        size_t h = 5381;
        while (*s) {
            h = ((h << 5) + h) + static_cast<unsigned char>(*s++);
        }
        return h;
    }
};


// ============================================================
// 6. reference_wrapper
// ============================================================

template <typename T>
class reference_wrapper {
public:
    using type = T;

    reference_wrapper(T& ref) noexcept : ptr_(&ref) {}
    reference_wrapper(const reference_wrapper&) noexcept = default;
    reference_wrapper& operator=(const reference_wrapper&) noexcept = default;

    operator T&() const noexcept { return *ptr_; }
    T& get() const noexcept { return *ptr_; }

    // 透明调用（如果 T 是个可调用对象）
    template <typename... Args>
    auto operator()(Args&&... args) const noexcept(noexcept(
        (*(T*)(0))(mystl::forward<Args>(args)...)))
        -> decltype((*(T*)(0))(mystl::forward<Args>(args)...))
    {
        T* p = this->ptr_;
        return (*p)(mystl::forward<Args>(args)...);
    }

private:
    T* ptr_;
};

template <typename T>
reference_wrapper<T> ref(T& t) noexcept {
    return reference_wrapper<T>(t);
}
template <typename T>
reference_wrapper<T> ref(reference_wrapper<T> t) noexcept {
    return t;
}
template <typename T>
void ref(const T&&) = delete;

template <typename T>
reference_wrapper<const T> cref(const T& t) noexcept {
    return reference_wrapper<const T>(t);
}
template <typename T>
reference_wrapper<const T> cref(reference_wrapper<T> t) noexcept {
    return reference_wrapper<const T>(t.get());
}
template <typename T>
void cref(const T&&) = delete;

MYSTL_NAMESPACE_END

#endif // MYSTL_FUNCTIONAL_H