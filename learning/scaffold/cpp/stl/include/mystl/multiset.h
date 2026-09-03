/*
 * multiset.h — 有序多重集合
 *
 * 对标 <multiset>，基于红黑树的纯手写实现
 * 特点：元素可重复、有序、O(log n) 查找/插入/删除
 */

#ifndef MYSTL_MULTISET_H
#define MYSTL_MULTISET_H

#include "rb_tree.h"
#include "functional.h"
#include "utility.h"
#include "iterator.h"

MYSTL_NAMESPACE_BEGIN

// ============================================================
// _Identity — multiset 的键值提取器
// ============================================================

// multiset 中元素本身就是键，直接返回
struct _Identity_multiset {
    template <typename T>
    constexpr T&& operator()(T&& t) const noexcept {
        return mystl::forward<T>(t);
    }
};

// ============================================================
// multiset — 有序可重复键集合
// ============================================================

template <typename Key, typename Compare = less<Key>, typename Alloc = allocator<Key>>
class multiset {
public:
    // 类型定义
    using key_type        = Key;
    using value_type      = Key;
    using key_compare     = Compare;
    using value_compare   = Compare;

private:
    using rb_tree_type = rb_tree<key_type, value_type, _Identity_multiset, key_compare, Alloc>;
    rb_tree_type t_;

public:
    // 从 rb_tree 获取的类型
    using reference              = typename rb_tree_type::reference;
    using const_reference        = typename rb_tree_type::const_reference;
    using iterator               = typename rb_tree_type::const_iterator;  // multiset 迭代器是 const 的
    using const_iterator         = typename rb_tree_type::const_iterator;
    using size_type              = typename rb_tree_type::size_type;
    using difference_type        = typename rb_tree_type::difference_type;
    using pointer                = typename rb_tree_type::const_pointer;
    using const_pointer          = typename rb_tree_type::const_pointer;
    using reverse_iterator       = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

    // ============================================================
    // 构造函数
    // ============================================================

    multiset() : t_() {}
    explicit multiset(const Compare& comp) : t_(comp) {}

    template <typename InputIterator>
    multiset(InputIterator first, InputIterator last) : t_() {
        t_.insert_equal(first, last);
    }

    multiset(std::initializer_list<value_type> il) : t_() {
        t_.insert_equal(il.begin(), il.end());
    }

    multiset(const multiset& other) : t_(other.t_) {}
    multiset(multiset&& other) noexcept : t_(mystl::move(other.t_)) {}

    // ============================================================
    // 赋值运算符
    // ============================================================

    multiset& operator=(const multiset& other) {
        t_ = other.t_;
        return *this;
    }

    multiset& operator=(multiset&& other) noexcept {
        t_ = mystl::move(other.t_);
        return *this;
    }

    multiset& operator=(std::initializer_list<value_type> il) {
        t_.clear();
        t_.insert_equal(il.begin(), il.end());
        return *this;
    }

    // ============================================================
    // 迭代器
    // ============================================================

    iterator begin() const noexcept { return t_.begin(); }
    iterator end() const noexcept { return t_.end(); }
    const_iterator cbegin() const noexcept { return t_.begin(); }
    const_iterator cend() const noexcept { return t_.end(); }

    reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    // ============================================================
    // 容量
    // ============================================================

    bool empty() const noexcept { return t_.empty(); }
    size_type size() const noexcept { return t_.size(); }
    size_type max_size() const noexcept { return t_.max_size(); }

    // ============================================================
    // 修改器
    // ============================================================

    // 单元素插入（允许重复）
    iterator insert(const value_type& x) {
        return t_.insert_equal(x);
    }

    iterator insert(value_type&& x) {
        return t_.insert_equal(mystl::move(x));
    }

    // 带提示的插入（忽略提示，直接插入）
    iterator insert(const_iterator hint, const value_type& x) {
        return t_.insert_equal(x);
    }

    iterator insert(const_iterator hint, value_type&& x) {
        return t_.insert_equal(mystl::move(x));
    }

    // 范围插入
    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last) {
        while (first != last) {
            t_.insert_equal(*first++);
        }
    }

    void insert(std::initializer_list<value_type> il) {
        for (auto& x : il) {
            t_.insert_equal(x);
        }
    }

    // 删除
    iterator erase(iterator position) { return t_.erase(position); }
    iterator erase(iterator first, iterator last) { return t_.erase(first, last); }

    size_type erase(const key_type& x) {
        pair<iterator, iterator> range = t_.equal_range(x);
        size_type count = mystl::distance(range.first, range.second);
        while (range.first != range.second) {
            t_.erase(range.first++);
        }
        return count;
    }

    void clear() noexcept { t_.clear(); }

    // 交换
    void swap(multiset& other) noexcept { t_.swap(other.t_); }

    // ============================================================
    // 查找操作
    // ============================================================

    size_type count(const key_type& x) const { return t_.count(x); }

    iterator find(const key_type& x) { return t_.find(x); }
    const_iterator find(const key_type& x) const { return t_.find(x); }

    bool contains(const key_type& x) const { return t_.find(x) != t_.end(); }

    iterator lower_bound(const key_type& x) { return t_.lower_bound(x); }
    const_iterator lower_bound(const key_type& x) const { return t_.lower_bound(x); }

    iterator upper_bound(const key_type& x) { return t_.upper_bound(x); }
    const_iterator upper_bound(const key_type& x) const { return t_.upper_bound(x); }

    mystl::pair<iterator, iterator> equal_range(const key_type& x) {
        return t_.equal_range(x);
    }

    mystl::pair<const_iterator, const_iterator> equal_range(const key_type& x) const {
        return t_.equal_range(x);
    }

    // ============================================================
    // 观察器
    // ============================================================

    key_compare key_comp() const { return t_.key_comp(); }
    value_compare value_comp() const { return t_.key_comp(); }
};

// ============================================================
// 比较运算符
// ============================================================

template <typename Key, typename Compare, typename Alloc>
bool operator==(const multiset<Key, Compare, Alloc>& a, const multiset<Key, Compare, Alloc>& b) {
    return a.size() == b.size() && mystl::equal(a.begin(), a.end(), b.begin());
}

template <typename Key, typename Compare, typename Alloc>
bool operator!=(const multiset<Key, Compare, Alloc>& a, const multiset<Key, Compare, Alloc>& b) {
    return !(a == b);
}

template <typename Key, typename Compare, typename Alloc>
bool operator<(const multiset<Key, Compare, Alloc>& a, const multiset<Key, Compare, Alloc>& b) {
    return mystl::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

template <typename Key, typename Compare, typename Alloc>
bool operator<=(const multiset<Key, Compare, Alloc>& a, const multiset<Key, Compare, Alloc>& b) {
    return !(b < a);
}

template <typename Key, typename Compare, typename Alloc>
bool operator>(const multiset<Key, Compare, Alloc>& a, const multiset<Key, Compare, Alloc>& b) {
    return b < a;
}

template <typename Key, typename Compare, typename Alloc>
bool operator>=(const multiset<Key, Compare, Alloc>& a, const multiset<Key, Compare, Alloc>& b) {
    return !(a < b);
}

// ============================================================
// 交换函数
// ============================================================

template <typename Key, typename Compare, typename Alloc>
void swap(multiset<Key, Compare, Alloc>& a, multiset<Key, Compare, Alloc>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_MULTISET_H