/*
 * set.h — 有序集合
 *
 * 对标 <set>，基于红黑树的纯手写实现
 * 特点：元素唯一、有序、O(log n) 查找/插入/删除
 */

#ifndef MYSTL_SET_H
#define MYSTL_SET_H

#include "rb_tree.h"
#include "functional.h"
#include "utility.h"
#include "iterator.h"
#include "algorithm.h"

MYSTL_NAMESPACE_BEGIN

// ============================================================
// _Identity — set 的键值提取器
// ============================================================

// set 中元素本身就是键，直接返回
struct _Identity {
    template <typename T>
    constexpr T&& operator()(T&& t) const noexcept {
        return mystl::forward<T>(t);
    }
};

// ============================================================
// set — 有序唯一键集合
// ============================================================

template <typename Key, typename Compare = less<Key>, typename Alloc = allocator<Key>>
class set {
public:
    // 类型定义
    using key_type        = Key;
    using value_type      = Key;
    using key_compare     = Compare;
    using value_compare   = Compare;

private:
    using rb_tree_type = rb_tree<key_type, value_type, _Identity, key_compare, Alloc>;
    rb_tree_type t_;

public:
    // 从 rb_tree 获取的类型
    using reference              = typename rb_tree_type::reference;
    using const_reference        = typename rb_tree_type::const_reference;
    using iterator               = typename rb_tree_type::const_iterator;  // set 迭代器是 const 的
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

    set() : t_() {}
    explicit set(const Compare& comp) : t_(comp) {}

    template <typename InputIterator>
    set(InputIterator first, InputIterator last) : t_() {
        t_.insert_unique(first, last);
    }

    set(std::initializer_list<value_type> il) : t_() {
        t_.insert_unique(il.begin(), il.end());
    }

    set(const set& other) : t_(other.t_) {}
    set(set&& other) noexcept : t_(mystl::move(other.t_)) {}

    // ============================================================
    // 赋值运算符
    // ============================================================

    set& operator=(const set& other) {
        t_ = other.t_;
        return *this;
    }

    set& operator=(set&& other) noexcept {
        t_ = mystl::move(other.t_);
        return *this;
    }

    set& operator=(std::initializer_list<value_type> il) {
        t_.clear();
        t_.insert_unique(il.begin(), il.end());
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

    // 单元素插入
    mystl::pair<iterator, bool> insert(const value_type& x) {
        auto p = t_.insert_unique(x);
        return mystl::pair<iterator, bool>(p.first, p.second);
    }

    mystl::pair<iterator, bool> insert(value_type&& x) {
        auto p = t_.insert_unique(mystl::move(x));
        return mystl::pair<iterator, bool>(p.first, p.second);
    }

    // 带提示的插入（忽略提示，直接插入）
    iterator insert(const_iterator hint, const value_type& x) {
        return t_.insert_unique(x).first;
    }

    iterator insert(const_iterator hint, value_type&& x) {
        return t_.insert_unique(mystl::move(x)).first;
    }

    // 范围插入
    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last) {
        while (first != last) {
            t_.insert_unique(*first++);
        }
    }

    void insert(std::initializer_list<value_type> il) {
        for (auto& x : il) {
            t_.insert_unique(x);
        }
    }

    // 删除
    size_type erase(const key_type& x) {
        iterator it = t_.find(x);
        if (it == t_.end()) return 0;
        t_.erase(it);
        return 1;
    }
    // erase by iterator (since iterator == const_iterator, just one method)
    iterator erase(iterator position) { t_.erase(position); return position; }
    iterator erase(iterator first, iterator last) { t_.erase(first, last); return first; }

    void clear() noexcept { t_.clear(); }

    // 交换
    void swap(set& other) noexcept { t_.swap(other.t_); }

    // ============================================================
    // 查找操作
    // ============================================================

    size_type count(const key_type& x) const {
        return t_.find(x) != t_.end() ? 1 : 0;
    }

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
bool operator==(const set<Key, Compare, Alloc>& a, const set<Key, Compare, Alloc>& b) {
    return a.size() == b.size() && mystl::equal(a.begin(), a.end(), b.begin());
}

template <typename Key, typename Compare, typename Alloc>
bool operator!=(const set<Key, Compare, Alloc>& a, const set<Key, Compare, Alloc>& b) {
    return !(a == b);
}

template <typename Key, typename Compare, typename Alloc>
bool operator<(const set<Key, Compare, Alloc>& a, const set<Key, Compare, Alloc>& b) {
    return mystl::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

template <typename Key, typename Compare, typename Alloc>
bool operator<=(const set<Key, Compare, Alloc>& a, const set<Key, Compare, Alloc>& b) {
    return !(b < a);
}

template <typename Key, typename Compare, typename Alloc>
bool operator>(const set<Key, Compare, Alloc>& a, const set<Key, Compare, Alloc>& b) {
    return b < a;
}

template <typename Key, typename Compare, typename Alloc>
bool operator>=(const set<Key, Compare, Alloc>& a, const set<Key, Compare, Alloc>& b) {
    return !(a < b);
}

// ============================================================
// 交换函数
// ============================================================

template <typename Key, typename Compare, typename Alloc>
void swap(set<Key, Compare, Alloc>& a, set<Key, Compare, Alloc>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_SET_H
