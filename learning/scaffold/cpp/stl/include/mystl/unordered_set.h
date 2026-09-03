/*
 * unordered_set.h - 无序集合容器
 *
 * 对标 std::unordered_set<Key>，纯手写实现
 * 底层：hash_table（链式哈希表）
 * 所有插入走 insert_unique（键唯一）
 */

#ifndef MYSTL_UNORDERED_SET_H
#define MYSTL_UNORDERED_SET_H

#include "hash_table.h"
#include "functional.h"
#include "utility.h"

MYSTL_NAMESPACE_BEGIN

// ============================================================
// unordered_set — 基于哈希表的无序集合
// ============================================================

template <typename Key, typename Hash = hash<Key>, typename Eq = equal_to<Key>,
          typename Alloc = allocator<Key>>
class unordered_set {
public:
    using key_type        = Key;
    using value_type      = Key;
    using hasher          = Hash;
    using key_equal       = Eq;
    using allocator_type  = Alloc;

private:
    using ht_type = hashtable<value_type, key_type, hasher,
                              _Identity, key_equal, allocator_type>;
    ht_type ht_;

public:
    using reference       = value_type&;
    using const_reference = const value_type&;
    using iterator        = typename ht_type::const_iterator;
    using const_iterator  = typename ht_type::const_iterator;
    using size_type       = typename ht_type::size_type;
    using difference_type = typename ht_type::difference_type;

    // 构造
    unordered_set() : ht_(100, hasher(), key_equal()) {}
    explicit unordered_set(size_type bucket_count, const hasher& hf = hasher(),
                           const key_equal& eql = key_equal())
        : ht_(bucket_count, hf, eql) {}

    template <typename InputIterator>
    unordered_set(InputIterator first, InputIterator last,
                  size_type bucket_count = 100, const hasher& hf = hasher(),
                  const key_equal& eql = key_equal())
        : ht_(bucket_count, hf, eql) {
        insert(first, last);
    }

    unordered_set(std::initializer_list<value_type> il,
                  size_type bucket_count = 100, const hasher& hf = hasher(),
                  const key_equal& eql = key_equal())
        : ht_(bucket_count, hf, eql) {
        insert(il.begin(), il.end());
    }

    unordered_set(const unordered_set& other) : ht_(other.ht_) {}
    unordered_set(unordered_set&& other) noexcept : ht_(mystl::move(other.ht_)) {}

    unordered_set& operator=(const unordered_set& other) {
        ht_ = other.ht_;
        return *this;
    }
    unordered_set& operator=(unordered_set&& other) noexcept {
        ht_ = mystl::move(other.ht_);
        return *this;
    }
    unordered_set& operator=(std::initializer_list<value_type> il) {
        ht_.clear();
        insert(il.begin(), il.end());
        return *this;
    }

    // 迭代器
    iterator begin() const noexcept { return ht_.begin(); }
    iterator end() const noexcept { return ht_.end(); }

    // 容量
    bool empty() const noexcept { return ht_.empty(); }
    size_type size() const noexcept { return ht_.size(); }
    size_type max_size() const noexcept { return ht_.max_size(); }

    // 修改器
    mystl::pair<iterator, bool> insert(const value_type& x) {
        auto p = ht_.insert_unique(x);
        return mystl::pair<iterator, bool>(p.first, p.second);
    }

    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last) {
        for (; first != last; ++first) ht_.insert_unique(*first);
    }

    void erase(iterator position) { ht_.erase(position); }
    size_type erase(const key_type& k) { return ht_.erase(k); }

    void clear() { ht_.clear(); }

    // 查找
    iterator find(const key_type& k) const { return ht_.find(k); }
    size_type count(const key_type& k) const { return ht_.count(k); }
    mystl::pair<iterator, iterator> equal_range(const key_type& k) const {
        return ht_.equal_range(k);
    }

    // 桶接口
    size_type bucket_count() const { return ht_.bucket_count(); }
    size_type bucket(const key_type& k) const { return ht_.bucket(k); }
    float load_factor() const { return ht_.load_factor(); }
    float max_load_factor() const { return ht_.max_load_factor(); }
    void max_load_factor(float ml) { ht_.max_load_factor(ml); }
    void rehash(size_type n) { ht_.rehash(n); }
    void reserve(size_type n) { ht_.reserve(n); }

    // 观察器
    hasher hash_function() const { return ht_.hash_funct(); }
    key_equal key_eq() const { return ht_.key_eq(); }

    void swap(unordered_set& other) noexcept { ht_.swap(other.ht_); }
};

// 比较运算符
template <typename Key, typename Hash, typename Eq, typename Alloc>
bool operator==(const unordered_set<Key, Hash, Eq, Alloc>& a,
                const unordered_set<Key, Hash, Eq, Alloc>& b) {
    if (a.size() != b.size()) return false;
    for (auto it = a.begin(); it != a.end(); ++it) {
        if (b.count(*it) == 0) return false;
    }
    return true;
}

template <typename Key, typename Hash, typename Eq, typename Alloc>
bool operator!=(const unordered_set<Key, Hash, Eq, Alloc>& a,
                const unordered_set<Key, Hash, Eq, Alloc>& b) {
    return !(a == b);
}

template <typename Key, typename Hash, typename Eq, typename Alloc>
void swap(unordered_set<Key, Hash, Eq, Alloc>& a,
          unordered_set<Key, Hash, Eq, Alloc>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_UNORDERED_SET_H