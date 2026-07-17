/*
 * multimap.h — 有序关联容器（键值映射，允许重复键）
 *
 * 对标 std::multimap，纯手写实现
 * 基于 rb_tree 实现，零依赖 std:: 容器
 */

#ifndef MYSTL_MULTIMAP_H
#define MYSTL_MULTIMAP_H

#include "rb_tree.h"
#include "functional.h"
#include "utility.h"
#include "algorithm.h"

MYSTL_NAMESPACE_BEGIN

// ============================================================
// 2. multimap 类模板
// ============================================================

template <typename Key, typename T, typename Compare = less<Key>,
          typename Alloc = allocator<mystl::pair<const Key, T>>>
class multimap {
public:
    using key_type        = Key;
    using mapped_type     = T;
    using value_type      = mystl::pair<const Key, T>;
    using key_compare     = Compare;

    // value_compare — 嵌套类，按 Key 比较 value_type
    class value_compare {
        friend class multimap;
    protected:
        Compare comp;
        value_compare(Compare c) : comp(c) {}
    public:
        using result_type         = bool;
        using first_argument_type  = value_type;
        using second_argument_type = value_type;
        bool operator()(const value_type& a, const value_type& b) const {
            return comp(a.first, b.first);
        }
    };

private:
    using rb_tree_type = rb_tree<key_type, value_type, _Select1st<value_type>,
                                  key_compare, Alloc>;
    rb_tree_type t_;

public:
    // 类型定义
    using reference             = value_type&;
    using const_reference       = const value_type&;
    using iterator              = typename rb_tree_type::iterator;
    using const_iterator        = typename rb_tree_type::const_iterator;
    using size_type             = typename rb_tree_type::size_type;
    using difference_type       = typename rb_tree_type::difference_type;
    using pointer               = typename rb_tree_type::pointer;
    using const_pointer         = typename rb_tree_type::const_pointer;
    using reverse_iterator      = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

    // ============================================================
    // 构造函数
    // ============================================================

    multimap() : t_() {}
    explicit multimap(const Compare& comp) : t_(comp) {}

    template <typename InputIterator>
    multimap(InputIterator first, InputIterator last) : t_() {
        for (; first != last; ++first)
            t_.insert_equal(*first);
    }

    multimap(std::initializer_list<value_type> il) : t_() {
        for (auto& x : il)
            t_.insert_equal(x);
    }

    multimap(const multimap& other) : t_(other.t_) {}
    multimap(multimap&& other) noexcept : t_(mystl::move(other.t_)) {}

    // ============================================================
    // 赋值运算符
    // ============================================================

    multimap& operator=(const multimap& other) {
        t_ = other.t_;
        return *this;
    }

    multimap& operator=(multimap&& other) noexcept {
        t_ = mystl::move(other.t_);
        return *this;
    }

    multimap& operator=(std::initializer_list<value_type> il) {
        t_.clear();
        for (auto& x : il)
            t_.insert_equal(x);
        return *this;
    }

    // ============================================================
    // 迭代器
    // ============================================================

    iterator begin() noexcept { return t_.begin(); }
    const_iterator begin() const noexcept { return t_.begin(); }
    const_iterator cbegin() const noexcept { return t_.begin(); }

    iterator end() noexcept { return t_.end(); }
    const_iterator end() const noexcept { return t_.end(); }
    const_iterator cend() const noexcept { return t_.end(); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    // ============================================================
    // 容量
    // ============================================================

    bool empty() const noexcept { return t_.empty(); }
    size_type size() const noexcept { return t_.size(); }
    size_type max_size() const noexcept { return size_type(-1) / sizeof(value_type); }

    // ============================================================
    // 修改器（使用 insert_equal）
    // ============================================================

    iterator insert(const value_type& x) {
        return t_.insert_equal(x);
    }

    iterator insert(value_type&& x) {
        return t_.insert_equal(mystl::move(x));
    }

    iterator insert(const_iterator hint, const value_type& x) {
        return t_.insert_equal(x);
    }

    iterator insert(const_iterator hint, value_type&& x) {
        return t_.insert_equal(mystl::move(x));
    }

    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last) {
        for (; first != last; ++first)
            t_.insert_equal(*first);
    }

    void insert(std::initializer_list<value_type> il) {
        for (auto& x : il)
            t_.insert_equal(x);
    }

    template <typename... Args>
    iterator emplace(Args&&... args) {
        return t_.insert_equal(value_type(mystl::forward<Args>(args)...));
    }

    template <typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        return t_.insert_equal(value_type(mystl::forward<Args>(args)...));
    }

    void erase(iterator position) { t_.erase(position); }
    void erase(const_iterator position) { t_.erase(position); }

    size_type erase(const key_type& x) {
        size_type count = 0;
        pair<iterator, iterator> range = t_.equal_range(x);
        iterator cur = range.first;
        while (cur != range.second) {
            iterator next = cur;
            ++next;
            t_.erase(cur);
            cur = next;
            ++count;
        }
        return count;
    }

    iterator erase(const_iterator first, const_iterator last) {
        const_iterator cur = first;
        while (cur != last) {
            const_iterator next = cur;
            ++next;
            t_.erase(cur);
            cur = next;
        }
        return iterator(last.node_);
    }

    void clear() noexcept { t_.clear(); }

    void swap(multimap& other) noexcept { t_.swap(other.t_); }

    // ============================================================
    // 查找
    // ============================================================

    iterator find(const key_type& x) { return t_.find(x); }
    const_iterator find(const key_type& x) const { return t_.find(x); }

    size_type count(const key_type& x) const { return t_.count(x); }

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
    // 比较器访问
    // ============================================================

    key_compare key_comp() const { return t_.comp_; }
    value_compare value_comp() const { return value_compare(t_.comp_); }
};

// ============================================================
// 3. 比较运算符
// ============================================================

template <typename Key, typename T, typename Compare, typename Alloc>
bool operator==(const multimap<Key, T, Compare, Alloc>& a,
                const multimap<Key, T, Compare, Alloc>& b) {
    return a.size() == b.size() && mystl::equal(a.begin(), a.end(), b.begin());
}

template <typename Key, typename T, typename Compare, typename Alloc>
bool operator!=(const multimap<Key, T, Compare, Alloc>& a,
                const multimap<Key, T, Compare, Alloc>& b) {
    return !(a == b);
}

template <typename Key, typename T, typename Compare, typename Alloc>
bool operator<(const multimap<Key, T, Compare, Alloc>& a,
               const multimap<Key, T, Compare, Alloc>& b) {
    return mystl::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

template <typename Key, typename T, typename Compare, typename Alloc>
bool operator<=(const multimap<Key, T, Compare, Alloc>& a,
                const multimap<Key, T, Compare, Alloc>& b) {
    return !(b < a);
}

template <typename Key, typename T, typename Compare, typename Alloc>
bool operator>(const multimap<Key, T, Compare, Alloc>& a,
               const multimap<Key, T, Compare, Alloc>& b) {
    return b < a;
}

template <typename Key, typename T, typename Compare, typename Alloc>
bool operator>=(const multimap<Key, T, Compare, Alloc>& a,
                const multimap<Key, T, Compare, Alloc>& b) {
    return !(a < b);
}

// ============================================================
// 4. swap 特化
// ============================================================

template <typename Key, typename T, typename Compare, typename Alloc>
void swap(multimap<Key, T, Compare, Alloc>& a,
          multimap<Key, T, Compare, Alloc>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_MULTIMAP_H