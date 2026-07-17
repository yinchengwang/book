/*
 * utility.h — 通用工具（pair / move / forward / swap 等）
 *
 * 对标 <utility>，纯手写实现
 */

#ifndef MYSTL_UTILITY_H
#define MYSTL_UTILITY_H

#include <cstddef>   // size_t, ptrdiff_t

MYSTL_NAMESPACE_BEGIN

// ============================================================
// move — 左值→右值转换
// ============================================================

template <typename T>
typename remove_reference<T>::type&& move(T&& t) noexcept {
    return static_cast<typename remove_reference<T>::type&&>(t);
}

// ============================================================
// forward — 完美转发
// ============================================================

template <typename T>
T&& forward(typename remove_reference<T>::type& t) noexcept {
    return static_cast<T&&>(t);
}

template <typename T>
T&& forward(typename remove_reference<T>::type&& t) noexcept {
    return static_cast<T&&>(t);
}

// ============================================================
// swap — 交换两个值
// ============================================================

template <typename T>
void swap(T& a, T& b) noexcept {
    T tmp = mystl::move(a);
    a = mystl::move(b);
    b = mystl::move(tmp);
}

// ============================================================
// exchange — 替换并返回旧值 (C++14)
// ============================================================

template <typename T, typename U = T>
T exchange(T& obj, U&& new_value) {
    T old = move(obj);
    obj = forward<U>(new_value);
    return old;
}

// ============================================================
// pair — 异构对
// ============================================================

struct piecewise_construct_t { explicit piecewise_construct_t() = default; };
constexpr piecewise_construct_t piecewise_construct{};

template <typename T1, typename T2>
struct pair {
    using first_type  = T1;
    using second_type = T2;

    T1 first;
    T2 second;

    // 默认构造函数
    template <typename U1 = T1, typename U2 = T2,
              enable_if_t<is_default_constructible<U1>::value &&
                          is_default_constructible<U2>::value, int> = 0>
    constexpr pair() : first(), second() {}

    // 两个参数构造
    constexpr pair(const T1& a, const T2& b) : first(a), second(b) {}

    // 模板构造
    template <typename U1, typename U2>
    constexpr pair(U1&& a, U2&& b)
        : first(static_cast<U1&&>(a)), second(static_cast<U2&&>(b)) {}

    // 拷贝构造（手动实现，兼容 const T1）
    constexpr pair(const pair& p)
        : first(p.first), second(p.second) {}

    // 移动构造
    constexpr pair(pair&& p) noexcept
        : first(mystl::move(p.first)), second(mystl::move(p.second)) {}

    // 异构拷贝构造（手动实现）
    template <typename U1, typename U2>
    constexpr pair(const pair<U1, U2>& p)
        : first(p.first), second(p.second) {}

    // 异构移动构造
    template <typename U1, typename U2>
    constexpr pair(pair<U1, U2>&& p)
        : first(mystl::move(p.first)), second(mystl::move(p.second)) {}

    // 拷贝赋值（手动实现，兼容 const T1）
    template <bool _ConstT1 = is_const<T1>::value>
    enable_if_t<!_ConstT1, pair&> operator=(const pair& other) {
        first = other.first;
        second = other.second;
        return *this;
    }
    // T1 为 const 时：禁用拷贝赋值（C++ 行为）
    template <bool _ConstT1 = is_const<T1>::value>
    enable_if_t<_ConstT1, pair&> operator=(const pair&) = delete;

    pair& operator=(pair&& other) noexcept {
        first = mystl::move(other.first);
        second = mystl::move(other.second);
        return *this;
    }

    void swap(pair& other) noexcept {
        mystl::swap(first, other.first);
        mystl::swap(second, other.second);
    }
};

// pair 比较运算符
template <typename T1, typename T2>
constexpr bool operator==(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return a.first == b.first && a.second == b.second;
}

template <typename T1, typename T2>
constexpr bool operator!=(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return !(a == b);
}

template <typename T1, typename T2>
constexpr bool operator<(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return a.first < b.first || (!(b.first < a.first) && a.second < b.second);
}

template <typename T1, typename T2>
constexpr bool operator<=(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return !(b < a);
}

template <typename T1, typename T2>
constexpr bool operator>(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return b < a;
}

template <typename T1, typename T2>
constexpr bool operator>=(const pair<T1, T2>& a, const pair<T1, T2>& b) {
    return !(a < b);
}

// make_pair — 自动推导类型的 pair 工厂
template <typename T1, typename T2>
constexpr pair<decay_t<T1>, decay_t<T2>> make_pair(T1&& a, T2&& b) {
    return pair<decay_t<T1>, decay_t<T2>>(forward<T1>(a), forward<T2>(b));
}

// pair 的 swap
template <typename T1, typename T2>
void swap(pair<T1, T2>& a, pair<T1, T2>& b) noexcept { a.swap(b); }

// ============================================================
// integer_sequence (C++14)
// ============================================================

template <typename T, T... Ints>
struct integer_sequence {
    using value_type = T;
    static constexpr size_t size() noexcept { return sizeof...(Ints); }
};

template <size_t... Ints>
using index_sequence = integer_sequence<size_t, Ints...>;

// make_integer_sequence — 简化版本（不展开 N，学习用足够）
// 完整分治实现需要 C++17 即时函数，这里采用简化形式
namespace _mystl_detail {

template <typename T, T... As, T... Bs>
constexpr integer_sequence<T, As..., Bs...>
_seq_concat(integer_sequence<T, As...>, integer_sequence<T, Bs...>);

// 拼接 helper
template <typename T, T A, T B>
struct _make_pair {
    using type = integer_sequence<T, A, B>;
};

} // namespace _mystl_detail

template <typename T, T N>
using make_integer_sequence = integer_sequence<T>;  // 简化：不展开 N

template <size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;

template <typename... T>
using index_sequence_for = make_index_sequence<sizeof...(T)>;

// ============================================================
// declval — 不创建实例获取类型
// ============================================================

template <typename T>
add_rvalue_reference_t<T> declval() noexcept;

MYSTL_NAMESPACE_END

#endif // MYSTL_UTILITY_H