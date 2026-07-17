/*
 * hash_table.h - 哈希表实现（链地址法）
 *
 * 对标 libstdc++ hashtable.h，纯手写实现
 * 数据结构：桶数组 + 单链表（chaining）
 * 支持：insert_unique / insert_equal / rehash / prime sizing
 */

#ifndef MYSTL_HASH_TABLE_H
#define MYSTL_HASH_TABLE_H

#include "type_traits.h"
#include "allocator.h"
#include "iterator.h"
#include "utility.h"
#include "functional.h"
#include "vector.h"
#include <cstddef>
#include <cmath>

MYSTL_NAMESPACE_BEGIN

// ============================================================
// 1. 哈希节点
// ============================================================

template <typename T>
struct _hash_node {
    _hash_node* next;
    T data;
    _hash_node(const T& v) : next(nullptr), data(v) {}
    _hash_node(T&& v) : next(nullptr), data(mystl::move(v)) {}
};

// ============================================================
// 2. 哈希表迭代器（前向迭代器）
// ============================================================

template <typename T, typename Val, typename Ref, typename Ptr>
struct _hash_iterator {
    using iterator_category = forward_iterator_tag;
    using value_type        = Val;
    using difference_type   = ptrdiff_t;
    using pointer           = Ptr;
    using reference         = Ref;

    using node_type = _hash_node<T>;
    using iterator  = _hash_iterator<T, Val, Ref, Ptr>;
    using self      = _hash_iterator<T, Val, Val&, Val*>;

    node_type* node_;
    vector<node_type*>* buckets_;

    _hash_iterator(node_type* n, vector<node_type*>* b) : node_(n), buckets_(b) {}
    _hash_iterator() : node_(nullptr), buckets_(nullptr) {}

    reference operator*() const { return node_->data; }
    pointer operator->() const { return &node_->data; }

    iterator& operator++() {
        node_type* old = node_;
        node_ = node_->next;
        if (!node_) {
            // 找下一个非空桶
            size_t idx = 0;
            for (size_t i = 0; i < buckets_->size(); ++i) {
                if (buckets_->operator[](i) == old) {
                    idx = i + 1;
                    break;
                }
            }
            for (; idx < buckets_->size(); ++idx) {
                if (buckets_->operator[](idx)) {
                    node_ = buckets_->operator[](idx);
                    break;
                }
            }
        }
        return *this;
    }

    iterator operator++(int) {
        iterator tmp = *this;
        ++*this;
        return tmp;
    }

    bool operator==(const iterator& other) const { return node_ == other.node_; }
    bool operator!=(const iterator& other) const { return node_ != other.node_; }
};

// ============================================================
// 3. 哈希表常量迭代器
// ============================================================

template <typename T, typename Val, typename Ref, typename Ptr>
struct _hash_const_iterator {
    using iterator_category = forward_iterator_tag;
    using value_type        = Val;
    using difference_type   = ptrdiff_t;
    using pointer           = Ptr;
    using reference         = Ref;

    using node_type = _hash_node<T>;
    using const_iterator = _hash_const_iterator<T, Val, const Val&, const Val*>;

    node_type* node_;
    const vector<node_type*>* buckets_;

    _hash_const_iterator(node_type* n, const vector<node_type*>* b) : node_(n), buckets_(b) {}
    _hash_const_iterator() : node_(nullptr), buckets_(nullptr) {}
    _hash_const_iterator(const _hash_iterator<T, Val, Val&, Val*>& it)
        : node_(it.node_), buckets_(it.buckets_) {}

    reference operator*() const { return node_->data; }
    pointer operator->() const { return &node_->data; }

    const_iterator& operator++() {
        node_type* old = node_;
        node_ = node_->next;
        if (!node_) {
            size_t idx = 0;
            for (size_t i = 0; i < buckets_->size(); ++i) {
                if (buckets_->operator[](i) == old) {
                    idx = i + 1;
                    break;
                }
            }
            for (; idx < buckets_->size(); ++idx) {
                if (buckets_->operator[](idx)) {
                    node_ = buckets_->operator[](idx);
                    break;
                }
            }
        }
        return *this;
    }

    const_iterator operator++(int) {
        const_iterator tmp = *this;
        ++*this;
        return tmp;
    }

    bool operator==(const const_iterator& other) const { return node_ == other.node_; }
    bool operator!=(const const_iterator& other) const { return node_ != other.node_; }
};

// ============================================================
// 4. 素数表（28 个素数，从 53 到 ~2^32）
// ============================================================

static const size_t _prime_list[] = {
    53ul,         97ul,         193ul,        389ul,        769ul,
    1543ul,       3079ul,       6151ul,       12289ul,      24593ul,
    49157ul,      98317ul,      196613ul,     393241ul,     786433ul,
    1572869ul,    3145739ul,    6291469ul,    12582917ul,   25165843ul,
    50331653ul,   100663319ul,  201326611ul,  402653189ul,  805306457ul,
    1610612741ul, 3221225473ul, 4294967291ul
};

static const size_t _num_primes = sizeof(_prime_list) / sizeof(_prime_list[0]);

// 返回 >= n 的最小素数
inline size_t _next_prime(size_t n) {
    for (size_t i = 0; i < _num_primes; ++i) {
        if (_prime_list[i] >= n) return _prime_list[i];
    }
    return _prime_list[_num_primes - 1];
}

// ============================================================
// 5. hashtable 类模板
// ============================================================

template <typename Value, typename Key, typename HashFcn,
          typename ExtractKey, typename EqualKey, typename Alloc = allocator<Value>>
class hashtable {
public:
    using value_type      = Value;
    using key_type        = Key;
    using hasher          = HashFcn;
    using key_equal       = EqualKey;
    using allocator_type  = Alloc;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using pointer         = value_type*;
    using const_pointer   = const value_type*;
    using reference       = value_type&;
    using const_reference = const value_type&;

    using node_type       = _hash_node<value_type>;
    using node_allocator  = allocator<node_type>;

    using iterator = _hash_iterator<value_type, value_type, reference, pointer>;
    using const_iterator = _hash_const_iterator<value_type, value_type, const_reference, const_pointer>;

private:
    hasher hash_;
    key_equal equals_;
    ExtractKey get_key_;
    node_allocator alloc_;
    vector<node_type*> buckets_;
    size_type num_elements_;
    float max_load_factor_;

    // 分配节点
    node_type* _allocate_node(const value_type& v) {
        node_type* n = alloc_.allocate(1);
        alloc_.construct(n, v);
        return n;
    }

    node_type* _allocate_node(value_type&& v) {
        node_type* n = alloc_.allocate(1);
        alloc_.construct(n, mystl::move(v));
        return n;
    }

    // 释放节点
    void _deallocate_node(node_type* n) {
        alloc_.destroy(n);
        alloc_.deallocate(n, 1);
    }

    // 计算 bucket 索引
    size_type _bkt_num(const value_type& v) const {
        return _bkt_num_key(get_key_(v));
    }

    size_type _bkt_num_key(const key_type& k) const {
        return hash_(k) % buckets_.size();
    }

    size_type _bkt_num_key(const key_type& k, size_type n) const {
        return hash_(k) % n;
    }

public:
    // ============================================================
    // 构造函数
    // ============================================================

    hashtable(size_type n = 0, const hasher& hf = hasher(),
              const key_equal& eql = key_equal(),
              const ExtractKey& ext = ExtractKey())
        : hash_(hf), equals_(eql), get_key_(ext), num_elements_(0), max_load_factor_(1.0f) {
        size_type bkt_cnt = _next_prime(n > 0 ? n : 1);
        buckets_.resize(bkt_cnt, nullptr);
    }

    hashtable(const hashtable& other)
        : hash_(other.hash_), equals_(other.equals_), get_key_(other.get_key_),
          num_elements_(0), max_load_factor_(other.max_load_factor_) {
        buckets_.resize(other.buckets_.size(), nullptr);
        for (size_type i = 0; i < other.buckets_.size(); ++i) {
            node_type* cur = other.buckets_[i];
            while (cur) {
                _insert_unique_noresize(cur->data);
                cur = cur->next;
            }
        }
    }

    hashtable(hashtable&& other) noexcept
        : hash_(mystl::move(other.hash_)), equals_(mystl::move(other.equals_)),
          get_key_(mystl::move(other.get_key_)), buckets_(mystl::move(other.buckets_)),
          num_elements_(other.num_elements_), max_load_factor_(other.max_load_factor_) {
        other.num_elements_ = 0;
    }

    ~hashtable() { clear(); }

    hashtable& operator=(const hashtable& other) {
        if (this != &other) {
            clear();
            hash_ = other.hash_;
            equals_ = other.equals_;
            get_key_ = other.get_key_;
            max_load_factor_ = other.max_load_factor_;
            buckets_.resize(other.buckets_.size(), nullptr);
            for (size_type i = 0; i < other.buckets_.size(); ++i) {
                node_type* cur = other.buckets_[i];
                while (cur) {
                    _insert_unique_noresize(cur->data);
                    cur = cur->next;
                }
            }
        }
        return *this;
    }

    hashtable& operator=(hashtable&& other) noexcept {
        if (this != &other) {
            clear();
            hash_ = mystl::move(other.hash_);
            equals_ = mystl::move(other.equals_);
            get_key_ = mystl::move(other.get_key_);
            buckets_ = mystl::move(other.buckets_);
            num_elements_ = other.num_elements_;
            max_load_factor_ = other.max_load_factor_;
            other.num_elements_ = 0;
        }
        return *this;
    }

    // ============================================================
    // 迭代器
    // ============================================================

    iterator begin() {
        for (size_type i = 0; i < buckets_.size(); ++i) {
            if (buckets_[i]) return iterator(buckets_[i], &buckets_);
        }
        return end();
    }

    iterator end() { return iterator(nullptr, &buckets_); }

    const_iterator begin() const {
        for (size_type i = 0; i < buckets_.size(); ++i) {
            if (buckets_[i]) return const_iterator(buckets_[i], &buckets_);
        }
        return end();
    }

    const_iterator end() const { return const_iterator(nullptr, &buckets_); }

    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

    // ============================================================
    // 容量
    // ============================================================

    bool empty() const { return num_elements_ == 0; }
    size_type size() const { return num_elements_; }
    size_type max_size() const { return size_type(-1); }

    size_type bucket_count() const { return buckets_.size(); }
    size_type max_bucket_count() const { return _prime_list[_num_primes - 1]; }

    size_type bucket_size(size_type n) const {
        size_type cnt = 0;
        node_type* cur = buckets_[n];
        while (cur) { ++cnt; cur = cur->next; }
        return cnt;
    }

    // 返回 key 所在的桶索引
    size_type bucket(const key_type& k) const {
        return _bkt_num_key(k);
    }

    // ============================================================
    // 负载因子
    // ============================================================

    float load_factor() const {
        return static_cast<float>(num_elements_) / static_cast<float>(buckets_.size());
    }

    float max_load_factor() const { return max_load_factor_; }
    void max_load_factor(float z) { max_load_factor_ = z; }

    // ============================================================
    // 查找
    // ============================================================

    iterator find(const key_type& k) {
        size_type n = _bkt_num_key(k);
        node_type* cur = buckets_[n];
        while (cur) {
            if (equals_(get_key_(cur->data), k))
                return iterator(cur, &buckets_);
            cur = cur->next;
        }
        return end();
    }

    const_iterator find(const key_type& k) const {
        size_type n = _bkt_num_key(k);
        node_type* cur = buckets_[n];
        while (cur) {
            if (equals_(get_key_(cur->data), k))
                return const_iterator(cur, &buckets_);
            cur = cur->next;
        }
        return end();
    }

    size_type count(const key_type& k) const {
        size_type n = _bkt_num_key(k);
        size_type cnt = 0;
        node_type* cur = buckets_[n];
        while (cur) {
            if (equals_(get_key_(cur->data), k)) ++cnt;
            cur = cur->next;
        }
        return cnt;
    }

    // ============================================================
    // 插入（唯一键）
    // ============================================================

    pair<iterator, bool> insert_unique(const value_type& v) {
        _rehash_if_needed(num_elements_ + 1);
        return _insert_unique_noresize(v);
    }

    pair<iterator, bool> insert_unique(value_type&& v) {
        _rehash_if_needed(num_elements_ + 1);
        return _insert_unique_noresize(mystl::move(v));
    }

    pair<iterator, bool> _insert_unique_noresize(const value_type& v) {
        size_type n = _bkt_num(v);
        node_type* cur = buckets_[n];
        while (cur) {
            if (equals_(get_key_(cur->data), get_key_(v)))
                return pair<iterator, bool>(iterator(cur, &buckets_), false);
            cur = cur->next;
        }
        node_type* node = _allocate_node(v);
        node->next = buckets_[n];
        buckets_[n] = node;
        ++num_elements_;
        return pair<iterator, bool>(iterator(node, &buckets_), true);
    }

    pair<iterator, bool> _insert_unique_noresize(value_type&& v) {
        size_type n = _bkt_num(v);
        node_type* cur = buckets_[n];
        while (cur) {
            if (equals_(get_key_(cur->data), get_key_(v)))
                return pair<iterator, bool>(iterator(cur, &buckets_), false);
            cur = cur->next;
        }
        node_type* node = _allocate_node(mystl::move(v));
        node->next = buckets_[n];
        buckets_[n] = node;
        ++num_elements_;
        return pair<iterator, bool>(iterator(node, &buckets_), true);
    }

    // ============================================================
    // 插入（允许重复键）
    // ============================================================

    iterator insert_equal(const value_type& v) {
        _rehash_if_needed(num_elements_ + 1);
        return _insert_equal_noresize(v);
    }

    iterator insert_equal(value_type&& v) {
        _rehash_if_needed(num_elements_ + 1);
        return _insert_equal_noresize(mystl::move(v));
    }

    iterator _insert_equal_noresize(const value_type& v) {
        size_type n = _bkt_num(v);
        node_type* cur = buckets_[n];
        while (cur) {
            if (equals_(get_key_(cur->data), get_key_(v))) {
                node_type* node = _allocate_node(v);
                node->next = cur->next;
                cur->next = node;
                ++num_elements_;
                return iterator(node, &buckets_);
            }
            cur = cur->next;
        }
        node_type* node = _allocate_node(v);
        node->next = buckets_[n];
        buckets_[n] = node;
        ++num_elements_;
        return iterator(node, &buckets_);
    }

    iterator _insert_equal_noresize(value_type&& v) {
        size_type n = _bkt_num(v);
        node_type* cur = buckets_[n];
        while (cur) {
            if (equals_(get_key_(cur->data), get_key_(v))) {
                node_type* node = _allocate_node(mystl::move(v));
                node->next = cur->next;
                cur->next = node;
                ++num_elements_;
                return iterator(node, &buckets_);
            }
            cur = cur->next;
        }
        node_type* node = _allocate_node(mystl::move(v));
        node->next = buckets_[n];
        buckets_[n] = node;
        ++num_elements_;
        return iterator(node, &buckets_);
    }

    // ============================================================
    // 删除
    // ============================================================

    size_type erase(const key_type& k) {
        size_type n = _bkt_num_key(k);
        node_type* cur = buckets_[n];
        node_type* prev = nullptr;
        size_type erased = 0;
        while (cur) {
            if (equals_(get_key_(cur->data), k)) {
                if (prev) prev->next = cur->next;
                else buckets_[n] = cur->next;
                node_type* tmp = cur;
                cur = cur->next;
                _deallocate_node(tmp);
                ++erased;
                --num_elements_;
            } else {
                prev = cur;
                cur = cur->next;
            }
        }
        return erased;
    }

    void erase(iterator it) {
        if (it.node_) {
            size_type n = _bkt_num(*it);
            node_type* cur = buckets_[n];
            node_type* prev = nullptr;
            while (cur && cur != it.node_) {
                prev = cur;
                cur = cur->next;
            }
            if (cur) {
                if (prev) prev->next = cur->next;
                else buckets_[n] = cur->next;
                _deallocate_node(cur);
                --num_elements_;
            }
        }
    }

    void erase(iterator first, iterator last) {
        while (first != last) {
            iterator next = first;
            ++next;
            erase(first);
            first = next;
        }
    }

    // ============================================================
    // 清空
    // ============================================================

    void clear() {
        for (size_type i = 0; i < buckets_.size(); ++i) {
            node_type* cur = buckets_[i];
            while (cur) {
                node_type* tmp = cur;
                cur = cur->next;
                _deallocate_node(tmp);
            }
            buckets_[i] = nullptr;
        }
        num_elements_ = 0;
    }

    // ============================================================
    // 重哈希
    // ============================================================

    void reserve(size_type n) {
        size_type bkt_cnt = _next_prime(n);
        if (bkt_cnt > buckets_.size()) _rehash(bkt_cnt);
    }

    void rehash(size_type n) {
        size_type bkt_cnt = _next_prime(n);
        if (bkt_cnt != buckets_.size()) _rehash(bkt_cnt);
    }

    void _rehash_if_needed(size_type new_count) {
        if (static_cast<float>(new_count) > max_load_factor_ * static_cast<float>(buckets_.size())) {
            size_type new_bkt_cnt = _next_prime(static_cast<size_type>(
                std::floor(static_cast<float>(new_count) / max_load_factor_) + 1));
            _rehash(new_bkt_cnt);
        }
    }

    void _rehash(size_type new_bkt_cnt) {
        vector<node_type*> new_buckets(new_bkt_cnt, nullptr);
        for (size_type i = 0; i < buckets_.size(); ++i) {
            node_type* cur = buckets_[i];
            while (cur) {
                node_type* next = cur->next;
                size_type new_idx = _bkt_num_key(get_key_(cur->data), new_bkt_cnt);
                cur->next = new_buckets[new_idx];
                new_buckets[new_idx] = cur;
                cur = next;
            }
        }
        buckets_.swap(new_buckets);
    }

    // ============================================================
    // 交换
    // ============================================================

    void swap(hashtable& other) noexcept {
        mystl::swap(hash_, other.hash_);
        mystl::swap(equals_, other.equals_);
        mystl::swap(get_key_, other.get_key_);
        buckets_.swap(other.buckets_);
        mystl::swap(num_elements_, other.num_elements_);
        mystl::swap(max_load_factor_, other.max_load_factor_);
    }
};

template <typename Value, typename Key, typename HashFcn,
          typename ExtractKey, typename EqualKey, typename Alloc>
void swap(hashtable<Value, Key, HashFcn, ExtractKey, EqualKey, Alloc>& a,
          hashtable<Value, Key, HashFcn, ExtractKey, EqualKey, Alloc>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_HASH_TABLE_H