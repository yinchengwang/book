/*
 * algorithm.h - STL 算法库
 *
 * 提供标准 STL 算法实现
 */

#ifndef MYSTL_ALGORITHM_H
#define MYSTL_ALGORITHM_H

#include "type_traits.h"
#include "iterator.h"
#include <cstring>
#include "utility.h"

MYSTL_NAMESPACE_BEGIN

// ============================================================
// 非修改序列操作
// ============================================================

// find
template <typename InputIterator, typename T>
InputIterator find(InputIterator first, InputIterator last, const T& value) {
    for (; first != last; ++first) {
        if (*first == value) {
            return first;
        }
    }
    return last;
}

template <typename InputIterator, typename Predicate>
InputIterator find_if(InputIterator first, InputIterator last, Predicate pred) {
    for (; first != last; ++first) {
        if (pred(*first)) {
            return first;
        }
    }
    return last;
}

template <typename InputIterator, typename Predicate>
InputIterator find_if_not(InputIterator first, InputIterator last, Predicate pred) {
    for (; first != last; ++first) {
        if (!pred(*first)) {
            return first;
        }
    }
    return last;
}

// count
template <typename InputIterator, typename T>
typename iterator_traits<InputIterator>::difference_type
count(InputIterator first, InputIterator last, const T& value) {
    typename iterator_traits<InputIterator>::difference_type n = 0;
    for (; first != last; ++first) {
        if (*first == value) {
            ++n;
        }
    }
    return n;
}

template <typename InputIterator, typename Predicate>
typename iterator_traits<InputIterator>::difference_type
count_if(InputIterator first, InputIterator last, Predicate pred) {
    typename iterator_traits<InputIterator>::difference_type n = 0;
    for (; first != last; ++first) {
        if (pred(*first)) {
            ++n;
        }
    }
    return n;
}

// equal
template <typename InputIterator1, typename InputIterator2>
bool equal(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2) {
    for (; first1 != last1; ++first1, ++first2) {
        if (!(*first1 == *first2)) {
            return false;
        }
    }
    return true;
}

template <typename InputIterator1, typename InputIterator2, typename BinaryPredicate>
bool equal(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2, BinaryPredicate pred) {
    for (; first1 != last1; ++first1, ++first2) {
        if (!pred(*first1, *first2)) {
            return false;
        }
    }
    return true;
}


// ============================================================
// 修改序列操作
// ============================================================

// copy
template <typename InputIterator, typename OutputIterator>
OutputIterator copy(InputIterator first, InputIterator last, OutputIterator dest) {
    for (; first != last; ++first, ++dest) {
        *dest = *first;
    }
    return dest;
}

// copy_if
template <typename InputIterator, typename OutputIterator, typename Predicate>
OutputIterator copy_if(InputIterator first, InputIterator last, OutputIterator dest, Predicate pred) {
    for (; first != last; ++first) {
        if (pred(*first)) {
            *dest = *first;
            ++dest;
        }
    }
    return dest;
}

// copy_n
template <typename InputIterator, typename Size, typename OutputIterator>
OutputIterator copy_n(InputIterator first, Size n, OutputIterator result) {
    for (Size i = 0; i < n; ++i, ++first, ++result) {
        *result = *first;
    }
    return result;
}

// move
template <typename InputIterator, typename OutputIterator>
OutputIterator move(InputIterator first, InputIterator last, OutputIterator dest) {
    for (; first != last; ++first, ++dest) {
        *dest = mystl::move(*first);
    }
    return dest;
}

// fill
template <typename ForwardIterator, typename T>
void fill(ForwardIterator first, ForwardIterator last, const T& value) {
    for (; first != last; ++first) {
        *first = value;
    }
}

template <typename OutputIterator, typename Size, typename T>
OutputIterator fill_n(OutputIterator first, Size n, const T& value) {
    for (Size i = 0; i < n; ++i, ++first) {
        *first = value;
    }
    return first;
}

// transform
template <typename InputIterator, typename OutputIterator, typename UnaryOperation>
OutputIterator transform(InputIterator first, InputIterator last, OutputIterator dest, UnaryOperation op) {
    for (; first != last; ++first, ++dest) {
        *dest = op(*first);
    }
    return dest;
}

template <typename InputIterator1, typename InputIterator2, typename OutputIterator, typename BinaryOperation>
OutputIterator transform(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2,
                         OutputIterator dest, BinaryOperation op) {
    for (; first1 != last1; ++first1, ++first2, ++dest) {
        *dest = op(*first1, *first2);
    }
    return dest;
}

// replace
template <typename ForwardIterator, typename T>
void replace(ForwardIterator first, ForwardIterator last, const T& old_value, const T& new_value) {
    for (; first != last; ++first) {
        if (*first == old_value) {
            *first = new_value;
        }
    }
}

template <typename ForwardIterator, typename Predicate, typename T>
void replace_if(ForwardIterator first, ForwardIterator last, Predicate pred, const T& new_value) {
    for (; first != last; ++first) {
        if (pred(*first)) {
            *first = new_value;
        }
    }
}

// swap_ranges
template <typename InputIterator1, typename InputIterator2>
InputIterator2 swap_ranges(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2) {
    for (; first1 != last1; ++first1, ++first2) {
        mystl::swap(*first1, *first2);
    }
    return first2;
}

// reverse — 原地翻转
template <typename BidirectionalIterator>
void reverse(BidirectionalIterator first, BidirectionalIterator last) {
    while (first != last && first != --last) {
        mystl::swap(*first, *last);
        ++first;
    }
}

// remove
template <typename ForwardIterator, typename T>
ForwardIterator remove(ForwardIterator first, ForwardIterator last, const T& value) {
    ForwardIterator result = first;
    for (; first != last; ++first) {
        if (!(*first == value)) {
            *result = *first;
            ++result;
        }
    }
    return result;
}

template <typename ForwardIterator, typename Predicate>
ForwardIterator remove_if(ForwardIterator first, ForwardIterator last, Predicate pred) {
    ForwardIterator result = first;
    for (; first != last; ++first) {
        if (!pred(*first)) {
            *result = *first;
            ++result;
        }
    }
    return result;
}

// unique
template <typename ForwardIterator>
ForwardIterator unique(ForwardIterator first, ForwardIterator last) {
    if (first == last) return last;

    ForwardIterator result = first;
    ++first;
    for (; first != last; ++first) {
        if (!(*result == *first)) {
            ++result;
            *result = *first;
        }
    }
    return ++result;
}

template <typename ForwardIterator, typename BinaryPredicate>
ForwardIterator unique(ForwardIterator first, ForwardIterator last, BinaryPredicate pred) {
    if (first == last) return last;

    ForwardIterator result = first;
    ++first;
    for (; first != last; ++first) {
        if (!pred(*result, *first)) {
            ++result;
            *result = *first;
        }
    }
    return ++result;
}


// ============================================================
// 排序操作
// ============================================================

// is_sorted
template <typename ForwardIterator>
bool is_sorted(ForwardIterator first, ForwardIterator last) {
    if (first == last) return true;
    for (ForwardIterator next = first; ++next != last; first = next) {
        if (*next < *first) {
            return false;
        }
    }
    return true;
}

template <typename ForwardIterator, typename Compare>
bool is_sorted(ForwardIterator first, ForwardIterator last, Compare comp) {
    if (first == last) return true;
    for (ForwardIterator next = first; ++next != last; first = next) {
        if (comp(*next, *first)) {
            return false;
        }
    }
    return true;
}

// sort (内省排序简化版，使用快速排序 + 插入排序)
template <typename RandomAccessIterator>
void sort(RandomAccessIterator first, RandomAccessIterator last) {
    typedef typename iterator_traits<RandomAccessIterator>::value_type T;
    typedef typename iterator_traits<RandomAccessIterator>::difference_type diff_type;

    diff_type n = last - first;
    if (n <= 1) return;

    // 小数组用插入排序
    if (n < 16) {
        for (RandomAccessIterator i = first + 1; i != last; ++i) {
            T key = *i;
            RandomAccessIterator j = i;
            while (j != first && *(j - 1) > key) {
                *j = *(j - 1);
                --j;
            }
            *j = key;
        }
        return;
    }

    // 三数取中
    RandomAccessIterator m = first + n / 2;
    RandomAccessIterator l = first;
    RandomAccessIterator r = last - 1;

    if (*m < *l) mystl::swap(*m, *l);
    if (*r < *m) mystl::swap(*r, *m);
    if (*m < *l) mystl::swap(*m, *l);

    T pivot = *m;
    RandomAccessIterator i = first;
    RandomAccessIterator j = last - 1;

    while (true) {
        do { ++i; } while (*i < pivot);
        do { --j; } while (pivot < *j);
        if (i < j) {
            mystl::swap(*i, *j);
        } else {
            break;
        }
    }

    sort(first, j + 1);
    sort(j + 1, last);
}

template <typename RandomAccessIterator, typename Compare>
void sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
    typedef typename iterator_traits<RandomAccessIterator>::value_type T;
    typedef typename iterator_traits<RandomAccessIterator>::difference_type diff_type;

    diff_type n = last - first;
    if (n <= 1) return;

    if (n < 16) {
        for (RandomAccessIterator i = first + 1; i != last; ++i) {
            T key = *i;
            RandomAccessIterator j = i;
            while (j != first && comp(key, *(j - 1))) {
                *j = *(j - 1);
                --j;
            }
            *j = key;
        }
        return;
    }

    RandomAccessIterator m = first + n / 2;
    if (comp(*m, *first)) mystl::swap(*m, *first);
    if (comp(*(last - 1), *m)) mystl::swap(*(last - 1), *m);
    if (comp(*m, *first)) mystl::swap(*m, *first);

    T pivot = *m;
    RandomAccessIterator i = first;
    RandomAccessIterator j = last - 1;

    while (true) {
        do { ++i; } while (comp(*i, pivot));
        do { --j; } while (comp(pivot, *j));
        if (i < j) {
            mystl::swap(*i, *j);
        } else {
            break;
        }
    }

    sort(first, j + 1, comp);
    sort(j + 1, last, comp);
}


// ============================================================
// 二分搜索
// ============================================================

// lower_bound
template <typename ForwardIterator, typename T>
ForwardIterator lower_bound(ForwardIterator first, ForwardIterator last, const T& value) {
    typename iterator_traits<ForwardIterator>::difference_type count = last - first;
    while (count > 0) {
        typename iterator_traits<ForwardIterator>::difference_type step = count / 2;
        ForwardIterator mid = first + step;
        if (*mid < value) {
            first = mid + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

template <typename ForwardIterator, typename T, typename Compare>
ForwardIterator lower_bound(ForwardIterator first, ForwardIterator last, const T& value, Compare comp) {
    typename iterator_traits<ForwardIterator>::difference_type count = last - first;
    while (count > 0) {
        typename iterator_traits<ForwardIterator>::difference_type step = count / 2;
        ForwardIterator mid = first + step;
        if (comp(*mid, value)) {
            first = mid + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

// upper_bound
template <typename ForwardIterator, typename T>
ForwardIterator upper_bound(ForwardIterator first, ForwardIterator last, const T& value) {
    typename iterator_traits<ForwardIterator>::difference_type count = last - first;
    while (count > 0) {
        typename iterator_traits<ForwardIterator>::difference_type step = count / 2;
        ForwardIterator mid = first + step;
        if (!(value < *mid)) {
            first = mid + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

template <typename ForwardIterator, typename T, typename Compare>
ForwardIterator upper_bound(ForwardIterator first, ForwardIterator last, const T& value, Compare comp) {
    typename iterator_traits<ForwardIterator>::difference_type count = last - first;
    while (count > 0) {
        typename iterator_traits<ForwardIterator>::difference_type step = count / 2;
        ForwardIterator mid = first + step;
        if (!comp(value, *mid)) {
            first = mid + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

// binary_search
template <typename ForwardIterator, typename T>
bool binary_search(ForwardIterator first, ForwardIterator last, const T& value) {
    ForwardIterator it = lower_bound(first, last, value);
    return it != last && !(value < *it);
}


// ============================================================
// 极值操作
// ============================================================

// min
template <typename T>
const T& min(const T& a, const T& b) {
    return (a < b) ? a : b;
}

template <typename T, typename Compare>
const T& min(const T& a, const T& b, Compare comp) {
    return comp(a, b) ? a : b;
}

// max
template <typename T>
const T& max(const T& a, const T& b) {
    return (a > b) ? a : b;
}

template <typename T, typename Compare>
const T& max(const T& a, const T& b, Compare comp) {
    return comp(a, b) ? b : a;
}

// minmax
template <typename T>
mystl::pair<const T&, const T&> minmax(const T& a, const T& b) {
    return (a < b) ? mystl::pair<const T&, const T&>(a, b) : mystl::pair<const T&, const T&>(b, a);
}

// min_element
template <typename ForwardIterator>
ForwardIterator min_element(ForwardIterator first, ForwardIterator last) {
    if (first == last) return last;
    ForwardIterator smallest = first;
    ++first;
    for (; first != last; ++first) {
        if (*first < *smallest) {
            smallest = first;
        }
    }
    return smallest;
}

// max_element
template <typename ForwardIterator>
ForwardIterator max_element(ForwardIterator first, ForwardIterator last) {
    if (first == last) return last;
    ForwardIterator largest = first;
    ++first;
    for (; first != last; ++first) {
        if (*largest < *first) {
            largest = first;
        }
    }
    return largest;
}


// ============================================================
// 堆操作
// ============================================================

// is_heap
template <typename RandomAccessIterator>
bool is_heap(RandomAccessIterator first, RandomAccessIterator last) {
    typename iterator_traits<RandomAccessIterator>::difference_type n = last - first;
    for (typename iterator_traits<RandomAccessIterator>::difference_type i = 1; i < n; ++i) {
        if (*(first + i) > *((first + (i - 1) / 2))) {
            return false;
        }
    }
    return true;
}

// make_heap
template <typename RandomAccessIterator>
void make_heap(RandomAccessIterator first, RandomAccessIterator last) {
    typedef typename iterator_traits<RandomAccessIterator>::difference_type diff_type;
    diff_type n = last - first;
    for (diff_type i = n / 2 - 1; i >= 0; --i) {
        // 向下调整
        diff_type largest = i;
        while (true) {
            diff_type left = 2 * largest + 1;
            diff_type right = 2 * largest + 2;
            if (left < n && *(first + left) > *(first + largest)) {
                largest = left;
            }
            if (right < n && *(first + right) > *(first + largest)) {
                largest = right;
            }
            if (largest != i) {
                mystl::swap(*(first + i), *(first + largest));
                i = largest;
            } else {
                break;
            }
        }
    }
}

// pop_heap
template <typename RandomAccessIterator>
void pop_heap(RandomAccessIterator first, RandomAccessIterator last) {
    --last;
    mystl::swap(*first, *last);
    make_heap(first, last);
}

// push_heap
template <typename RandomAccessIterator>
void push_heap(RandomAccessIterator first, RandomAccessIterator last) {
    typedef typename iterator_traits<RandomAccessIterator>::difference_type diff_type;
    diff_type n = last - first;

    diff_type child = n - 1;
    diff_type parent = (child - 1) / 2;

    while (child > 0 && *(first + child) > *(first + parent)) {
        mystl::swap(*(first + child), *(first + parent));
        child = parent;
        parent = (child - 1) / 2;
    }
}

// sort_heap
template <typename RandomAccessIterator>
void sort_heap(RandomAccessIterator first, RandomAccessIterator last) {
    while (last - first > 1) {
        pop_heap(first, last);
        --last;
    }
}


// ============================================================
// 字典序比较
// ============================================================

// lexicographical_compare
template <typename InputIterator1, typename InputIterator2>
bool lexicographical_compare(InputIterator1 first1, InputIterator1 last1,
                              InputIterator2 first2, InputIterator2 last2) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (*first1 < *first2) return true;
        if (*first2 < *first1) return false;
    }
    return first1 == last1 && first2 != last2;
}

template <typename InputIterator1, typename InputIterator2, typename Compare>
bool lexicographical_compare(InputIterator1 first1, InputIterator1 last1,
                              InputIterator2 first2, InputIterator2 last2,
                              Compare comp) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (comp(*first1, *first2)) return true;
        if (comp(*first2, *first1)) return false;
    }
    return first1 == last1 && first2 != last2;
}

MYSTL_NAMESPACE_END

#endif // MYSTL_ALGORITHM_H
