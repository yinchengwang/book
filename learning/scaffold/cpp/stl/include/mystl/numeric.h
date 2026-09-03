/*
 * numeric.h - 数值算法库（accumulate/iota 在 algorithm.h 中）
 *
 * 对标 <numeric>，纯手写实现
 * 包含：accumulate / inner_product / partial_sum / adjacent_difference / iota
 * 注：accumulate 和 iota 在 algorithm.h 也有定义（学习用途重合）
 */

#ifndef MYSTL_NUMERIC_H
#define MYSTL_NUMERIC_H

#include "type_traits.h"

MYSTL_NAMESPACE_BEGIN

// ============================================================
// accumulate — 累加 [first, last) 到 init
// ============================================================

template <typename InputIterator, typename T>
T accumulate(InputIterator first, InputIterator last, T init) {
    for (; first != last; ++first)
        init = init + *first;
    return init;
}

template <typename InputIterator, typename T, typename BinaryOperation>
T accumulate(InputIterator first, InputIterator last, T init, BinaryOperation op) {
    for (; first != last; ++first)
        init = op(init, *first);
    return init;
}

// ============================================================
// inner_product — 内积
// ============================================================

template <typename InputIterator1, typename InputIterator2, typename T>
T inner_product(InputIterator1 first1, InputIterator1 last1,
                InputIterator2 first2, T init) {
    for (; first1 != last1; ++first1, ++first2)
        init = init + (*first1 * *first2);
    return init;
}

template <typename InputIterator1, typename InputIterator2, typename T,
          typename BinaryOperation1, typename BinaryOperation2>
T inner_product(InputIterator1 first1, InputIterator1 last1,
                InputIterator2 first2, T init,
                BinaryOperation1 op1, BinaryOperation2 op2) {
    for (; first1 != last1; ++first1, ++first2)
        init = op1(init, op2(*first1, *first2));
    return init;
}

// ============================================================
// partial_sum — 部分和
// ============================================================

template <typename InputIterator, typename OutputIterator>
OutputIterator partial_sum(InputIterator first, InputIterator last,
                           OutputIterator result) {
    if (first == last) return result;
    typename iterator_traits<InputIterator>::value_type sum = *first;
    *result = sum;
    ++result;
    ++first;
    for (; first != last; ++first, ++result) {
        sum = sum + *first;
        *result = sum;
    }
    return result;
}

template <typename InputIterator, typename OutputIterator, typename BinaryOperation>
OutputIterator partial_sum(InputIterator first, InputIterator last,
                           OutputIterator result, BinaryOperation op) {
    if (first == last) return result;
    typename iterator_traits<InputIterator>::value_type sum = *first;
    *result = sum;
    ++result;
    ++first;
    for (; first != last; ++first, ++result) {
        sum = op(sum, *first);
        *result = sum;
    }
    return result;
}

// ============================================================
// adjacent_difference — 相邻差
// ============================================================

template <typename InputIterator, typename OutputIterator>
OutputIterator adjacent_difference(InputIterator first, InputIterator last,
                                    OutputIterator result) {
    if (first == last) return result;
    typename iterator_traits<InputIterator>::value_type prev = *first;
    *result = prev;
    ++result;
    ++first;
    for (; first != last; ++first, ++result) {
        typename iterator_traits<InputIterator>::value_type cur = *first;
        *result = cur - prev;
        prev = cur;
    }
    return result;
}

template <typename InputIterator, typename OutputIterator, typename BinaryOperation>
OutputIterator adjacent_difference(InputIterator first, InputIterator last,
                                    OutputIterator result, BinaryOperation op) {
    if (first == last) return result;
    typename iterator_traits<InputIterator>::value_type prev = *first;
    *result = prev;
    ++result;
    ++first;
    for (; first != last; ++first, ++result) {
        typename iterator_traits<InputIterator>::value_type cur = *first;
        *result = op(cur, prev);
        prev = cur;
    }
    return result;
}

// ============================================================
// iota — 递增填充 [first, last)
// ============================================================

template <typename ForwardIterator, typename T>
void iota(ForwardIterator first, ForwardIterator last, T value) {
    for (; first != last; ++first, ++value)
        *first = value;
}

MYSTL_NAMESPACE_END

#endif // MYSTL_NUMERIC_H