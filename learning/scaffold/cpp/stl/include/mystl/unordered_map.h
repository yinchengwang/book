/*
 * unordered_map.h - 无序映射容器
 *
 * 对标 std::unordered_map<Key, T>，纯手写实现
 * 底层：hash_table（链式哈希表）
 * 所有插入走 insert_unique（键唯一）
 * 提供 operator[] 和 at() 键值访问
 */

#ifndef MYSTL_UNORDERED_MAP_H
#define MYSTL_UNORDERED_MAP_H

#include "hash_table.h"
#include "functional.h"
#include "utility.h"

MYSTL_NAMESPACE_BEGIN

// ============================================================
// unordered_map — 基于哈希表的无序键值映射
// ============================================================

template <typename Key, typename T, typename Hash = hash<Key>,
          typename Eq = equal_to<Key>, typename Alloc = allocator<mystl::pair<const Key, T>>>
class unordered_map {
public:
    using key_type        = Key;
    using mapped_type     = T;
    using value_type      = mystl::pair<const Key, T>;
    using hasher          = Hash;
    using key_equal       = Eq;
    using allocator_type  = Alloc;

private:
    using ht_type = hashtable<value_type, key_type, hasher,
                              _Select1st<value_type>, key_equal, allocator_type>;
    ht_type ht_;

public:
    using reference       = value_type&;
    using const_reference = const value_type&;
    using iterator        = typename ht_type::iterator;
    using const_iterator  = typename ht_type::const_iterator;
    using size_type       = typename ht_type::size_type;
    using difference_type = typename ht_type::difference_type;

    // 构造
    unordered_map() : ht_(100, hasher(), key_equal()) {}
    explicit unordered_map(size_type bucket_count, const hasher& hf = hasher(),
                           const key_equal& eql = key_equal())
        : ht_(bucket_count, hf, eql) {}

    template <typename InputIterator>
    unordered_map(InputIterator first, InputIterator last,
                  size_type bucket_count = 100, const hasher& hf = hasher(),
                  const key_equal& eql = key_equal())
        : ht_(bucket_count, hf, eql) {
        insert(first, last);
    }

    unordered_map(std::initializer_list<value_type> il,
                  size_type bucket_count = 100, const hasher& hf = hasher(),
                  const key_equal& eql = key_equal())
        : ht_(bucket_count, hf, eql) {
        insert(il.begin(), il.end());
    }

    unordered_map(const unordered_map& other) : ht_(other.ht_) {}
    unordered_map(unordered_map&& other) noexcept : ht_(mystl::move(other.ht_)) {}

    unordered_map& operator=(const unordered_map& other) {
        ht_ = other.ht_; return *this;
    }
    unordered_map& operator=(unordered_map&& other) noexcept {
        ht_ = mystl::move(other.ht_); return *this;
    }
    unordered_map& operator=(std::initializer_list<value_type> il) {
        ht_.clear();
        insert(il.begin(), il.end());
        return *this;
    }

    // 元素访问
    mapped_type& operator[](const key_type& k) {
        iterator it = find(k);
        if (it == end()) {
            it = ht_.insert_unique(value_type(k, mapped_type())).first;
        }
        return it->second;
    }

    mapped_type& at(const key_type& k) {
        iterator it = find(k);
        return it->second;
    }
    const mapped_type& at(const key_type& k) const {
        const_iterator it = find(k);
        return it->second;
    }

    // 迭代器
    iterator begin() noexcept { return ht_.begin(); }
    const_iterator begin() const noexcept { return ht_.begin(); }
    iterator end() noexcept { return ht_.end(); }
    const_iterator end() const noexcept { return ht_.end(); }

    // 容量
    bool empty() const noexcept { return ht_.empty(); }
    size_type size() const noexcept { return ht_.size(); }
    size_type max_size() const noexcept { return ht_.max_size(); }

    // 修改器
    mystl::pair<iterator, bool> insert(const value_type& x) {
        return ht_.insert_unique(x);
    }

    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last) {
        for (; first != last; ++first) ht_.insert_unique(*first);
    }

    void erase(iterator position) { ht_.erase(position); }
    size_type erase(const key_type& k) { return ht_.erase(k); }
    void clear() { ht_.clear(); }

    // 查找
    iterator find(const key_type& k) { return ht_.find(k); }
    const_iterator find(const key_type& k) const { return ht_.find(k); }
    size_type count(const key_type& k) const { return ht_.count(k); }
    mystl::pair<iterator, iterator> equal_range(const key_type& k) {
        return ht_.equal_range(k);
    }
    mystl::pair<const_iterator, const_iterator> equal_range(const key_type& k) const {
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

    hasher hash_function() const { return ht_.hash_funct(); }
    key_equal key_eq() const { return ht_.key_eq(); }

    void swap(unordered_map& other) noexcept { ht_.swap(other.ht_); }
};

// 比较运算符
template <typename Key, typename T, typename Hash, typename Eq, typename Alloc>
bool operator==(const unordered_map<Key, T, Hash, Eq, Alloc>& a,
                const unordered_map<Key, T, Hash, Eq, Alloc>& b) {
    if (a.size() != b.size()) return false;
    for (auto it = a.begin(); it != a.end(); ++it) {
        auto b_it = b.find(it->first);
        if (b_it == b.end() || !(it->second == b_it->second)) return false;
    }
    return true;
}

template <typename Key, typename T, typename Hash, typename Eq, typename Alloc>
bool operator!=(const unordered_map<Key, T, Hash, Eq, Alloc>& a,
                const unordered_map<Key, T, Hash, Eq, Alloc>& b) {
    return !(a == b);
}

template <typename Key, typename T, typename Hash, typename Eq, typename Alloc>
void swap(unordered_map<Key, T, Hash, Eq, Alloc>& a,
          unordered_map<Key, T, Hash, Eq, Alloc>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_UNORDERED_MAP_H