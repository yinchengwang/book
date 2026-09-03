/*
 * list.h - 双向链表容器
 *
 * 对标 std::list<T>，纯手写实现
 * 数据结构：哨兵节点环形双向链表
 * 关键操作分离到 list.tcc（sort / merge / splice / unique）
 * 参考：libstdc++ stl_list.h
 */

#ifndef MYSTL_LIST_H
#define MYSTL_LIST_H

#include "type_traits.h"
#include "allocator.h"
#include "iterator.h"
#include "utility.h"
#include <initializer_list>
#include <cstddef>   // ptrdiff_t

MYSTL_NAMESPACE_BEGIN

// ============================================================
// list_node_base — 基节点（不含数据，用于哨兵）
// ============================================================

struct list_node_base {
    list_node_base* prev;
    list_node_base* next;

    list_node_base() : prev(this), next(this) {}
};

// ============================================================
// list_node — 数据节点
// ============================================================

template <typename T>
struct list_node : list_node_base {
    T data;

    list_node() : list_node_base(), data() {}
    explicit list_node(const T& val) : list_node_base(), data(val) {}
    explicit list_node(T&& val) : list_node_base(), data(mystl::move(val)) {}
};

// ============================================================
// list_iterator / list_const_iterator
// ============================================================

template <typename T>
class list_iterator {
public:
    using iterator_category = bidirectional_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    list_node<T>* node_;

    list_iterator() : node_(nullptr) {}
    explicit list_iterator(list_node<T>* n) : node_(n) {}

    reference operator*() const { return node_->data; }
    pointer operator->() const { return &(node_->data); }

    list_iterator& operator++() { node_ = static_cast<list_node<T>*>(node_->next); return *this; }
    list_iterator operator++(int) { list_iterator tmp = *this; node_ = static_cast<list_node<T>*>(node_->next); return tmp; }
    list_iterator& operator--() { node_ = static_cast<list_node<T>*>(node_->prev); return *this; }
    list_iterator operator--(int) { list_iterator tmp = *this; node_ = static_cast<list_node<T>*>(node_->prev); return tmp; }

    bool operator==(const list_iterator& o) const { return node_ == o.node_; }
    bool operator!=(const list_iterator& o) const { return node_ != o.node_; }
};

template <typename T>
class list_const_iterator {
public:
    using iterator_category = bidirectional_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;

    const list_node<T>* node_;

    list_const_iterator() : node_(nullptr) {}
    explicit list_const_iterator(const list_node<T>* n) : node_(n) {}
    list_const_iterator(const list_iterator<T>& it) : node_(it.node_) {}

    reference operator*() const { return node_->data; }
    pointer operator->() const { return &(node_->data); }

    list_const_iterator& operator++() { node_ = static_cast<const list_node<T>*>(node_->next); return *this; }
    list_const_iterator operator++(int) { list_const_iterator tmp = *this; node_ = static_cast<const list_node<T>*>(node_->next); return tmp; }
    list_const_iterator& operator--() { node_ = static_cast<const list_node<T>*>(node_->prev); return *this; }
    list_const_iterator operator--(int) { list_const_iterator tmp = *this; node_ = static_cast<const list_node<T>*>(node_->prev); return tmp; }

    bool operator==(const list_const_iterator& o) const { return node_ == o.node_; }
    bool operator!=(const list_const_iterator& o) const { return node_ != o.node_; }
};


// ============================================================
// list 类模板
// ============================================================

template <typename T, typename Alloc = allocator<T>>
class list {
public:
    // 类型定义
    using value_type        = T;
    using allocator_type    = Alloc;
    using reference         = T&;
    using const_reference   = const T&;
    using pointer           = T*;
    using const_pointer     = const T*;
    using size_type         = size_t;
    using difference_type   = ptrdiff_t;
    using iterator          = list_iterator<T>;
    using const_iterator    = list_const_iterator<T>;
    using reverse_iterator  = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

    // 节点分配器（rebind 到 list_node<T>）
    using node_allocator = typename Alloc::template rebind<list_node<T>>::other;

private:
    list_node_base  head_;   // 哨兵节点，不含数据
    size_type       size_;
    node_allocator  node_alloc_;

    // 分配一个节点
    list_node<T>* alloc_node() {
        return node_alloc_.allocate(1);
    }

    // 构造节点（拷贝构造）
    list_node<T>* create_node(const T& val) {
        list_node<T>* p = alloc_node();
        try {
            node_alloc_.construct(p, val);
        } catch (...) {
            node_alloc_.deallocate(p, 1);
            throw;
        }
        return p;
    }

    // 构造节点（移动构造）
    list_node<T>* create_node(T&& val) {
        list_node<T>* p = alloc_node();
        try {
            node_alloc_.construct(p, mystl::move(val));
        } catch (...) {
            node_alloc_.deallocate(p, 1);
            throw;
        }
        return p;
    }

    // 销毁节点
    void destroy_node(list_node<T>* p) {
        node_alloc_.destroy(p);
        node_alloc_.deallocate(p, 1);
    }

    // 在给定位置前插入原始节点
    void insert_node(list_node_base* pos, list_node_base* n) {
        n->next = pos;
        n->prev = pos->prev;
        pos->prev->next = n;
        pos->prev = n;
    }

    // 摘除节点（不销毁）
    void unlink_node(list_node_base* n) {
        n->prev->next = n->next;
        n->next->prev = n->prev;
    }

    // 初始化空链表
    void init_empty() {
        head_.next = &head_;
        head_.prev = &head_;
        size_ = 0;
    }

public:
    // ============================================================
    // 构造函数
    // ============================================================

    list() : head_(), size_(0), node_alloc_() {}

    explicit list(const Alloc& alloc) : head_(), size_(0), node_alloc_(alloc) {}

    explicit list(size_type n, const T& value = T(), const Alloc& alloc = Alloc())
        : head_(), size_(0), node_alloc_(alloc) {
        insert_node(&head_, &head_);  // 确保一致性
        for (size_type i = 0; i < n; ++i)
            push_back(value);
    }

    template <typename InputIterator>
    list(InputIterator first, InputIterator last,
         enable_if_t<!is_integral<InputIterator>::value, int> = 0)
        : head_(), size_(0), node_alloc_() {
        for (; first != last; ++first)
            push_back(*first);
    }

    list(const list& other) : head_(), size_(0), node_alloc_(other.node_alloc_) {
        for (const auto& x : other)
            push_back(x);
    }

    list(list&& other) noexcept
        : head_(), size_(0), node_alloc_(mystl::move(other.node_alloc_)) {
        if (other.head_.next != &other.head_) {
            // 接管链表
            head_.next = other.head_.next;
            head_.prev = other.head_.prev;
            head_.next->prev = &head_;
            head_.prev->next = &head_;
            size_ = other.size_;
            other.init_empty();
        }
    }

    list(std::initializer_list<T> il, const Alloc& alloc = Alloc())
        : head_(), size_(0), node_alloc_(alloc) {
        for (const auto& x : il)
            push_back(x);
    }

    ~list() {
        clear();
    }

    // ============================================================
    // 赋值运算符
    // ============================================================

    list& operator=(const list& other) {
        if (this != &other) assign(other.begin(), other.end());
        return *this;
    }

    list& operator=(list&& other) noexcept {
        if (this != &other) {
            clear();
            if (other.head_.next != &other.head_) {
                head_.next = other.head_.next;
                head_.prev = other.head_.prev;
                head_.next->prev = &head_;
                head_.prev->next = &head_;
                size_ = other.size_;
                other.init_empty();
            }
            node_alloc_ = mystl::move(other.node_alloc_);
        }
        return *this;
    }

    list& operator=(std::initializer_list<T> il) {
        assign(il.begin(), il.end());
        return *this;
    }

    void assign(size_type count, const T& value) {
        clear();
        for (size_type i = 0; i < count; ++i)
            push_back(value);
    }

	    template <typename InputIterator,
	              typename = typename enable_if<!is_integral<InputIterator>::value>::type>
	    void assign(InputIterator first, InputIterator last) {
	        clear();
	        for (; first != last; ++first)
	            push_back(*first);
	    }

    // ============================================================
    // 容量
    // ============================================================

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type max_size() const noexcept { return node_alloc_.max_size(); }

    // ============================================================
    // 元素访问
    // ============================================================

    reference front() { return static_cast<list_node<T>*>(head_.next)->data; }
    const_reference front() const { return static_cast<const list_node<T>*>(head_.next)->data; }
    reference back() { return static_cast<list_node<T>*>(head_.prev)->data; }
    const_reference back() const { return static_cast<const list_node<T>*>(head_.prev)->data; }

    // ============================================================
    // 迭代器
    // ============================================================

    iterator begin() noexcept { return iterator(static_cast<list_node<T>*>(head_.next)); }
    const_iterator begin() const noexcept { return const_iterator(static_cast<const list_node<T>*>(head_.next)); }
    const_iterator cbegin() const noexcept { return const_iterator(static_cast<const list_node<T>*>(head_.next)); }
    iterator end() noexcept { return iterator(static_cast<list_node<T>*>(&head_)); }
    const_iterator end() const noexcept { return const_iterator(static_cast<const list_node<T>*>(&head_)); }
    const_iterator cend() const noexcept { return const_iterator(static_cast<const list_node<T>*>(&head_)); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    // ============================================================
    // 修改器
    // ============================================================

    void push_front(const T& value) {
        insert_node(head_.next, create_node(value));
        ++size_;
    }

    void push_front(T&& value) {
        insert_node(head_.next, create_node(mystl::move(value)));
        ++size_;
    }

    void pop_front() {
        list_node<T>* n = static_cast<list_node<T>*>(head_.next);
        unlink_node(n);
        destroy_node(n);
        --size_;
    }

    void push_back(const T& value) {
        insert_node(&head_, create_node(value));
        ++size_;
    }

    void push_back(T&& value) {
        insert_node(&head_, create_node(mystl::move(value)));
        ++size_;
    }

    void pop_back() {
        list_node<T>* n = static_cast<list_node<T>*>(head_.prev);
        unlink_node(n);
        destroy_node(n);
        --size_;
    }

    // insert — 在 position 前插入
    iterator insert(const_iterator position, const T& value) {
        list_node<T>* n = create_node(value);
        insert_node(const_cast<list_node_base*>(static_cast<const list_node_base*>(position.node_)), n);
        ++size_;
        return iterator(n);
    }

    iterator insert(const_iterator position, T&& value) {
        list_node<T>* n = create_node(mystl::move(value));
        insert_node(const_cast<list_node_base*>(static_cast<const list_node_base*>(position.node_)), n);
        ++size_;
        return iterator(n);
    }

    iterator insert(const_iterator position, size_type count, const T& value) {
        if (count == 0) return iterator(const_cast<list_node<T>*>(position.node_));
        iterator ret = insert(position, value);
        for (size_type i = 1; i < count; ++i)
            insert(position, value);
        return ret;
    }

	    template <typename InputIterator,
	              typename = typename enable_if<!is_integral<InputIterator>::value>::type>
	    iterator insert(const_iterator position, InputIterator first, InputIterator last) {
	        if (first == last) return iterator(const_cast<list_node<T>*>(position.node_));
	        iterator ret = insert(position, *first);
	        ++first;
	        for (; first != last; ++first)
	            insert(position, *first);
	        return ret;
	    }

    // erase — 删除单个元素
    iterator erase(const_iterator position) {
        list_node<T>* n = const_cast<list_node<T>*>(position.node_);
        iterator next(static_cast<list_node<T>*>(n->next));
        unlink_node(n);
        destroy_node(n);
        --size_;
        return next;
    }

    // erase — 删除范围 [first, last)
    iterator erase(const_iterator first, const_iterator last) {
        while (first != last)
            first = erase(first);
        return iterator(const_cast<list_node<T>*>(first.node_));
    }

    void clear() noexcept {
        list_node_base* cur = head_.next;
        while (cur != &head_) {
            list_node_base* next = cur->next;
            destroy_node(static_cast<list_node<T>*>(cur));
            cur = next;
        }
        init_empty();
    }

    // ============================================================
    // 列表操作（splice / remove / unique / merge / sort / reverse）
    // ============================================================

    // splice — 移动整个链表到 position 前
    void splice(const_iterator position, list& other) {
        if (other.empty()) return;
        _splice_range(position, other.begin(), other.end(), other.size_);
        other.size_ = 0;
        other.head_.next = &other.head_;
        other.head_.prev = &other.head_;
    }

    // splice — 移动单个元素
    void splice(const_iterator position, list&, const_iterator it) {
        list_node<T>* n = const_cast<list_node<T>*>(it.node_);
        unlink_node(n);
        insert_node(const_cast<list_node_base*>(static_cast<const list_node_base*>(position.node_)), n);
        --size_;  // 从 other 移除，但 other 不追踪单个元素
        ++size_;
    }

    // splice — 移动范围
    void splice(const_iterator position, list&, const_iterator first, const_iterator last) {
        if (first == last) return;
        size_type count = 0;
        for (auto it = first; it != last; ++it) ++count;
        _splice_range(position, first, last, count);
    }

    void remove(const T& value) {
        for (iterator it = begin(); it != end(); ) {
            if (*it == value)
                it = erase(it);
            else
                ++it;
        }
    }

    template <typename Predicate>
    void remove_if(Predicate pred) {
        for (iterator it = begin(); it != end(); ) {
            if (pred(*it))
                it = erase(it);
            else
                ++it;
        }
    }

    void unique() {
        if (size_ <= 1) return;
        for (iterator it = begin(); ++it != end(); ) {
            iterator prev = it; --prev;
            if (*prev == *it) {
                erase(it);
                it = prev;
            }
        }
    }

    template <typename BinaryPredicate>
    void unique(BinaryPredicate pred) {
        if (size_ <= 1) return;
        for (iterator it = begin(); ++it != end(); ) {
            iterator prev = it; --prev;
            if (pred(*prev, *it)) {
                erase(it);
                it = prev;
            }
        }
    }

    void merge(list& other) {
        merge(other, mystl::less<T>());
    }

    template <typename Compare>
    void merge(list& other, Compare comp) {
        if (this == &other) return;
        iterator f1 = begin();
        iterator f2 = other.begin();
        while (f1 != end() && f2 != other.end()) {
            if (comp(*f2, *f1)) {
                iterator next = f2; ++next;
                splice(f1, other, f2);
                f2 = next;
            } else {
                ++f1;
            }
        }
        if (!other.empty())
            splice(end(), other);
    }

    void reverse() noexcept {
        if (size_ <= 1) return;
        list_node_base* cur = head_.next;
        while (cur != &head_) {
            list_node_base* next = cur->next;
            mystl::swap(cur->prev, cur->next);
            cur = next;
        }
        mystl::swap(head_.prev, head_.next);
    }

    // sort — 归并排序（O(n log n)）
    void sort() {
        sort(mystl::less<T>());
    }

    template <typename Compare>
    void sort(Compare comp) {
        if (size_ <= 1) return;
        // 归并排序：数组收集归并
        list carry;
        list counter[64];
        int fill = 0;
        while (!empty()) {
            carry.splice(carry.begin(), *this, begin());
            int i = 0;
            while (i < fill && !counter[i].empty()) {
                counter[i].merge(carry, comp);
                carry.swap(counter[i]);
                ++i;
            }
            carry.swap(counter[i]);
            if (i == fill) ++fill;
        }
        for (int i = 1; i < fill; ++i)
            counter[i].merge(counter[i - 1], comp);
        swap(counter[fill - 1]);
    }

    void resize(size_type count, T value = T()) {
        if (count < size_) {
            while (size_ > count) pop_back();
        } else if (count > size_) {
            while (size_ < count) push_back(value);
        }
    }

    void swap(list& other) noexcept {
        if (this == &other) return;
        // 交换哨兵，保持各链表完整
        if (empty() && other.empty()) {
            mystl::swap(size_, other.size_);
            return;
        }
        if (empty()) {
            _move_head_from(other);
            other.init_empty();
            return;
        }
        if (other.empty()) {
            other._move_head_from(*this);
            init_empty();
            return;
        }
        mystl::swap(head_.next->prev, other.head_.next->prev);
        mystl::swap(head_.prev->next, other.head_.prev->next);
        mystl::swap(head_.next, other.head_.next);
        mystl::swap(head_.prev, other.head_.prev);
        mystl::swap(size_, other.size_);
    }

    // 获取分配器
    allocator_type get_allocator() const noexcept {
        return allocator_type();
    }

private:
    // 辅助：将 other 的链表头移到 this
    void _move_head_from(list& other) {
        head_.next = other.head_.next;
        head_.prev = other.head_.prev;
        head_.next->prev = &head_;
        head_.prev->next = &head_;
        size_ = other.size_;
    }

    // 辅助：splice 范围
    void _splice_range(const_iterator position, const_iterator first, const_iterator last, size_type count) {
        if (first == last) return;
        // 摘下 [first, last)
        list_node_base* f = const_cast<list_node<T>*>(first.node_);
        list_node_base* l = const_cast<list_node<T>*>(last.node_);
        list_node_base* p = const_cast<list_node_base*>(static_cast<const list_node_base*>(position.node_));

        // 从原链表摘除
        f->prev->next = l;
        l->prev->next = f;
        // 记录原链表的 last 的前驱，用于恢复
        list_node_base* f_prev = f->prev;
        list_node_base* l_prev = l->prev;
        f->prev = l->prev;
        l->prev = f_prev;

        // 插入到目标位置
        p->prev->next = f;
        f->prev = p->prev;
        p->prev = l_prev;
        l_prev->next = p;

        // 修正 size，但注意：splice 从 other 移走，要从 other 减去
        // 这里调用方负责管理 size
    }
};

// ============================================================
// 非成员比较运算符
// ============================================================

template <typename T, typename Alloc>
bool operator==(const list<T, Alloc>& a, const list<T, Alloc>& b) {
    if (a.size() != b.size()) return false;
    auto it1 = a.begin(), it2 = b.begin();
    for (; it1 != a.end(); ++it1, ++it2)
        if (*it1 != *it2) return false;
    return true;
}

template <typename T, typename Alloc>
bool operator!=(const list<T, Alloc>& a, const list<T, Alloc>& b) {
    return !(a == b);
}

template <typename T, typename Alloc>
bool operator<(const list<T, Alloc>& a, const list<T, Alloc>& b) {
    auto it1 = a.begin(), it2 = b.begin();
    for (; it1 != a.end() && it2 != b.end(); ++it1, ++it2) {
        if (*it1 < *it2) return true;
        if (*it2 < *it1) return false;
    }
    return it1 == a.end() && it2 != b.end();
}

template <typename T, typename Alloc>
bool operator>(const list<T, Alloc>& a, const list<T, Alloc>& b) {
    return b < a;
}

template <typename T, typename Alloc>
bool operator<=(const list<T, Alloc>& a, const list<T, Alloc>& b) {
    return !(b < a);
}

template <typename T, typename Alloc>
bool operator>=(const list<T, Alloc>& a, const list<T, Alloc>& b) {
    return !(a < b);
}

template <typename T, typename Alloc>
void swap(list<T, Alloc>& a, list<T, Alloc>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_LIST_H