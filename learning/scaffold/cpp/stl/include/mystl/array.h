/*
 * array.h - 定长数组容器
 *
 * 对标 std::array<T, N>，纯手写实现
 * 注意：array 是聚合类型，支持聚合初始化
 */

#ifndef MYSTL_ARRAY_H
#define MYSTL_ARRAY_H

#include "type_traits.h"
#include "iterator.h"
#include <cstddef>  // size_t

MYSTL_NAMESPACE_BEGIN

// ============================================================
// array — 定长数组
// ============================================================

template <typename T, size_t N>
struct array {
    // 类型定义
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

    // 唯一数据成员（聚合类型）
    T _data[N];

    // 元素访问
    reference at(size_type pos) {
        if (pos >= N) throw std::out_of_range("array::at");
        return _data[pos];
    }
    const_reference at(size_type pos) const {
        if (pos >= N) throw std::out_of_range("array::at");
        return _data[pos];
    }

    reference operator[](size_type pos) { return _data[pos]; }
    const_reference operator[](size_type pos) const { return _data[pos]; }

    reference front() { return _data[0]; }
    const_reference front() const { return _data[0]; }
    reference back() { return _data[N - 1]; }
    const_reference back() const { return _data[N - 1]; }

    pointer data() noexcept { return N == 0 ? nullptr : _data; }
    const_pointer data() const noexcept { return N == 0 ? nullptr : _data; }

    // 迭代器
    iterator begin() noexcept { return _data; }
    const_iterator begin() const noexcept { return _data; }
    const_iterator cbegin() const noexcept { return _data; }
    iterator end() noexcept { return _data + N; }
    const_iterator end() const noexcept { return _data + N; }
    const_iterator cend() const noexcept { return _data + N; }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    // 容量
    constexpr bool empty() const noexcept { return N == 0; }
    constexpr size_type size() const noexcept { return N; }
    constexpr size_type max_size() const noexcept { return N; }

    // 修改
    void fill(const T& value) {
        for (size_type i = 0; i < N; ++i) _data[i] = value;
    }
    void swap(array& other) noexcept(noexcept(mystl::swap(declval<T&>(), declval<T&>()))) {
        for (size_type i = 0; i < N; ++i)
            mystl::swap(_data[i], other._data[i]);
    }
};

// 比较运算符
template <typename T, size_t N>
bool operator==(const array<T, N>& a, const array<T, N>& b) {
    for (size_t i = 0; i < N; ++i)
        if (a[i] != b[i]) return false;
    return true;
}
template <typename T, size_t N>
bool operator!=(const array<T, N>& a, const array<T, N>& b) { return !(a == b); }
template <typename T, size_t N>
bool operator<(const array<T, N>& a, const array<T, N>& b) {
    for (size_t i = 0; i < N; ++i) {
        if (a[i] < b[i]) return true;
        if (b[i] < a[i]) return false;
    }
    return false;
}
template <typename T, size_t N>
bool operator>(const array<T, N>& a, const array<T, N>& b) { return b < a; }
template <typename T, size_t N>
bool operator<=(const array<T, N>& a, const array<T, N>& b) { return !(b < a); }
template <typename T, size_t N>
bool operator>=(const array<T, N>& a, const array<T, N>& b) { return !(a < b); }

template <typename T, size_t N>
void swap(array<T, N>& a, array<T, N>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_ARRAY_H