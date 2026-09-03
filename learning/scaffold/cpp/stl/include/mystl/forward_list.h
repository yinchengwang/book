/*
 * forward_list.h - 单向链表容器
 *
 * 对标 std::forward_list<T>，纯手写实现
 * 数据结构：哨兵节点单向链表（before_begin 哨兵）
 * 注意：forward_list 没有 size() 方法
 * 参考：libstdc++ stl_forward_list.h
 */

#ifndef MYSTL_FORWARD_LIST_H
#define MYSTL_FORWARD_LIST_H

#include "type_traits.h"
#include "allocator.h"
#include "iterator.h"
#include "utility.h"
#include <initializer_list>
#include <cstddef>   // ptrdiff_t

MYSTL_NAMESPACE_BEGIN

// ============================================================
// forward_list_node_base — 基节点（不含数据，用于哨兵）
// ============================================================

struct forward_list_node_base {
    forward_list_node_base* next;

    forward_list_node_base() : next(nullptr) {}
};

// ============================================================
// forward_list_node — 数据节点
// ============================================================

template <typename T>
struct forward_list_node : forward_list_node_base {
    T data;

    forward_list_node() : forward_list_node_base(), data() {}
    explicit forward_list_node(const T& val) : forward_list_node_base(), data(val) {}
    explicit forward_list_node(T&& val) : forward_list_node_base(), data(mystl::move(val)) {}
};


// ============================================================
// forward_list_iterator / forward_list_const_iterator
// ============================================================

template <typename T>
class forward_list_iterator {
public:
    using iterator_category = forward_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    forward_list_node<T>* node_;

    forward_list_iterator() : node_(nullptr) {}
    explicit forward_list_iterator(forward_list_node<T>* n) : node_(n) {}

    reference operator*() const { return node_->data; }
    pointer operator->() const { return &(node_->data); }

    forward_list_iterator& operator++() { node_ = static_cast<forward_list_node<T>*>(node_->next); return *this; }
    forward_list_iterator operator++(int) { forward_list_iterator tmp = *this; node_ = static_cast<forward_list_node<T>*>(node_->next); return tmp; }

    bool operator==(const forward_list_iterator& o) const { return node_ == o.node_; }
    bool operator!=(const forward_list_iterator& o) const { return node_ != o.node_; }
};

template <typename T>
class forward_list_const_iterator {
public:
    using iterator_category = forward_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;

    const forward_list_node<T>* node_;

    forward_list_const_iterator() : node_(nullptr) {}
    explicit forward_list_const_iterator(const forward_list_node<T>* n) : node_(n) {}
    forward_list_const_iterator(const forward_list_iterator<T>& it) : node_(it.node_) {}

    reference operator*() const { return node_->data; }
    pointer operator->() const { return &(node_->data); }

    forward_list_const_iterator& operator++() { node_ = static_cast<const forward_list_node<T>*>(node_->next); return *this; }
    forward_list_const_iterator operator++(int) { forward_list_const_iterator tmp = *this; node_ = static_cast<const forward_list_node<T>*>(node_->next); return tmp; }

    bool operator==(const forward_list_const_iterator& o) const { return node_ == o.node_; }
    bool operator!=(const forward_list_const_iterator& o) const { return node_ != o.node_; }
};


// ============================================================
// forward_list 类模板
// ============================================================

template <typename T, typename Alloc = allocator<T>>
class forward_list {
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
    using iterator          = forward_list_iterator<T>;
    using const_iterator    = forward_list_const_iterator<T>;

    // 节点分配器
    using node_allocator = typename Alloc::template rebind<forward_list_node<T>>::other;

private:
    forward_list_node_base  before_head_;  // before_begin 哨兵
    size_type               size_;         // 仅用于 max_size，不对外暴露 size()
    node_allocator          node_alloc_;

    forward_list_node<T>* alloc_node() {
        return node_alloc_.allocate(1);
    }

    forward_list_node<T>* create_node(const T& val) {
        forward_list_node<T>* p = alloc_node();
        try {
            node_alloc_.construct(p, val);
        } catch (...) {
            node_alloc_.deallocate(p, 1);
            throw;
        }
        return p;
    }

    forward_list_node<T>* create_node(T&& val) {
        forward_list_node<T>* p = alloc_node();
        try {
            node_alloc_.construct(p, mystl::move(val));
        } catch (...) {
            node_alloc_.deallocate(p, 1);
            throw;
        }
        return p;
    }

    void destroy_node(forward_list_node<T>* p) {
        node_alloc_.destroy(p);
        node_alloc_.deallocate(p, 1);
    }

    // 在 pos 之后插入节点
    void insert_after_node(forward_list_node_base* pos, forward_list_node_base* n) {
        n->next = pos->next;
        pos->next = n;
    }

    // 获取 before_begin 迭代器
    iterator before_begin_impl() {
        return iterator(static_cast<forward_list_node<T>*>(&before_head_));
    }

    const_iterator before_begin_impl() const {
        return const_iterator(static_cast<const forward_list_node<T>*>(&before_head_));
    }

public:
    // ============================================================
    // 构造函数
    // ============================================================

    forward_list() : before_head_(), size_(0), node_alloc_() {}

    explicit forward_list(const Alloc& alloc) : before_head_(), size_(0), node_alloc_(alloc) {}

    explicit forward_list(size_type n, const T& value = T(), const Alloc& alloc = Alloc())
        : before_head_(), size_(0), node_alloc_(alloc) {
        for (size_type i = 0; i < n; ++i)
            push_front(value);
    }

    template <typename InputIterator>
    forward_list(InputIterator first, InputIterator last,
                 enable_if_t<!is_integral<InputIterator>::value, int> = 0)
        : before_head_(), size_(0), node_alloc_() {
        // 使用尾插法保持顺序
        forward_list_node_base* tail = &before_head_;
        for (; first != last; ++first) {
            forward_list_node<T>* n = create_node(*first);
            insert_after_node(tail, n);
            tail = n;
            ++size_;
        }
    }

    forward_list(const forward_list& other) : before_head_(), size_(0), node_alloc_(other.node_alloc_) {
        forward_list_node_base* tail = &before_head_;
        for (auto it = other.begin(); it != other.end(); ++it) {
            forward_list_node<T>* n = create_node(*it);
            insert_after_node(tail, n);
            tail = n;
            ++size_;
        }
    }

    forward_list(forward_list&& other) noexcept
        : before_head_(other.before_head_), size_(other.size_),
          node_alloc_(mystl::move(other.node_alloc_)) {
        other.before_head_.next = nullptr;
        other.size_ = 0;
    }

    forward_list(std::initializer_list<T> il, const Alloc& alloc = Alloc())
        : before_head_(), size_(0), node_alloc_(alloc) {
        forward_list_node_base* tail = &before_head_;
        for (const auto& x : il) {
            forward_list_node<T>* n = create_node(x);
            insert_after_node(tail, n);
            tail = n;
            ++size_;
        }
    }

    ~forward_list() {
        clear();
    }

    // ============================================================
    // 赋值
    // ============================================================

    forward_list& operator=(const forward_list& other) {
        if (this != &other) assign(other.begin(), other.end());
        return *this;
    }

    forward_list& operator=(forward_list&& other) noexcept {
        if (this != &other) {
            clear();
            before_head_.next = other.before_head_.next;
            size_ = other.size_;
            node_alloc_ = mystl::move(other.node_alloc_);
            other.before_head_.next = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    forward_list& operator=(std::initializer_list<T> il) {
        assign(il.begin(), il.end());
        return *this;
    }

    void assign(size_type count, const T& value) {
        clear();
        forward_list_node_base* tail = &before_head_;
        for (size_type i = 0; i < count; ++i) {
            forward_list_node<T>* n = create_node(value);
            insert_after_node(tail, n);
            tail = n;
            ++size_;
        }
    }

	    template <typename InputIterator,
	              typename = typename enable_if<!is_integral<InputIterator>::value>::type>
	    void assign(InputIterator first, InputIterator last) {
	        clear();
	        forward_list_node_base* tail = &before_head_;
	        for (; first != last; ++first) {
            forward_list_node<T>* n = create_node(*first);
            insert_after_node(tail, n);
            tail = n;
            ++size_;
        }
    }

    // ============================================================
    // 容量
    // ============================================================

    bool empty() const noexcept { return before_head_.next == nullptr; }
    size_type max_size() const noexcept { return node_alloc_.max_size(); }

    // ============================================================
    // 元素访问
    // ============================================================

    reference front() { return static_cast<forward_list_node<T>*>(before_head_.next)->data; }
    const_reference front() const { return static_cast<const forward_list_node<T>*>(before_head_.next)->data; }

    // ============================================================
    // 迭代器
    // ============================================================

    iterator before_begin() noexcept { return before_begin_impl(); }
    const_iterator before_begin() const noexcept { return before_begin_impl(); }
    const_iterator cbefore_begin() const noexcept { return before_begin_impl(); }

    iterator begin() noexcept { return iterator(static_cast<forward_list_node<T>*>(before_head_.next)); }
    const_iterator begin() const noexcept { return const_iterator(static_cast<const forward_list_node<T>*>(before_head_.next)); }
    const_iterator cbegin() const noexcept { return const_iterator(static_cast<const forward_list_node<T>*>(before_head_.next)); }
    iterator end() noexcept { return iterator(nullptr); }
    const_iterator end() const noexcept { return const_iterator(nullptr); }
    const_iterator cend() const noexcept { return const_iterator(nullptr); }

    // ============================================================
    // 修改器
    // ============================================================

    void push_front(const T& value) {
        insert_after_node(&before_head_, create_node(value));
        ++size_;
    }

    void push_front(T&& value) {
        insert_after_node(&before_head_, create_node(mystl::move(value)));
        ++size_;
    }

    void pop_front() {
        forward_list_node<T>* n = static_cast<forward_list_node<T>*>(before_head_.next);
        before_head_.next = n->next;
        destroy_node(n);
        --size_;
    }

    // insert_after
    iterator insert_after(const_iterator position, const T& value) {
        forward_list_node<T>* n = create_node(value);
        forward_list_node_base* pos = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(position.node_));
        insert_after_node(pos, n);
        ++size_;
        return iterator(n);
    }

    iterator insert_after(const_iterator position, T&& value) {
        forward_list_node<T>* n = create_node(mystl::move(value));
        forward_list_node_base* pos = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(position.node_));
        insert_after_node(pos, n);
        ++size_;
        return iterator(n);
    }

    iterator insert_after(const_iterator position, size_type count, const T& value) {
        if (count == 0) return iterator(const_cast<forward_list_node<T>*>(position.node_));
        iterator ret = insert_after(position, value);
        for (size_type i = 1; i < count; ++i)
            insert_after(position, value);
        return ret;
    }
    template <typename InputIterator,
              typename = typename enable_if<!is_integral<InputIterator>::value>::type>
    iterator insert_after(const_iterator position, InputIterator first, InputIterator last) {
        if (first == last) return iterator(const_cast<forward_list_node<T>*>(position.node_));

        forward_list_node_base* pos = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(position.node_));

        // 保存原始 next，因为我们要在 position 之后插入
        forward_list_node_base* original_next = pos->next;
        forward_list_node_base* tail = pos;

        // 逐个创建节点并链接
        for (; first != last; ++first) {
            forward_list_node<T>* n = create_node(*first);
            tail->next = n;
            tail = n;
            ++size_;
        }

        // 将最后一个新节点链接到原来的 next
        tail->next = original_next;

        // 返回最后插入的节点（如果有的话）
        return iterator(static_cast<forward_list_node<T>*>(tail));
    }

    // emplace_after
    template <typename... Args>
    iterator emplace_after(const_iterator position, Args&&... args) {
        forward_list_node<T>* n = alloc_node();
        try {
            node_alloc_.construct(n, T(mystl::forward<Args>(args)...));
        } catch (...) {
            node_alloc_.deallocate(n, 1);
            throw;
        }
        forward_list_node_base* pos = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(position.node_));
        insert_after_node(pos, n);
        ++size_;
        return iterator(n);
    }

    // erase_after
    iterator erase_after(const_iterator position) {
        forward_list_node_base* pos = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(position.node_));
        forward_list_node<T>* n = static_cast<forward_list_node<T>*>(pos->next);
        pos->next = n->next;
        destroy_node(n);
        --size_;
        return iterator(static_cast<forward_list_node<T>*>(pos->next));
    }

    iterator erase_after(const_iterator first, const_iterator last) {
        if (first == last) return iterator(const_cast<forward_list_node<T>*>(first.node_));
        forward_list_node_base* pos = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(first.node_));
        forward_list_node_base* end = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(last.node_));
        // 删除 (pos, end) 范围
        while (pos->next != end) {
            forward_list_node<T>* n = static_cast<forward_list_node<T>*>(pos->next);
            pos->next = n->next;
            destroy_node(n);
            --size_;
        }
        return iterator(static_cast<forward_list_node<T>*>(end));
    }

    void clear() noexcept {
        forward_list_node_base* cur = before_head_.next;
        while (cur) {
            forward_list_node_base* next = cur->next;
            destroy_node(static_cast<forward_list_node<T>*>(cur));
            cur = next;
        }
        before_head_.next = nullptr;
        size_ = 0;
    }

    // ============================================================
    // 列表操作
    // ============================================================

    void splice_after(const_iterator position, forward_list& other) {
        // 将 other 所有元素移到 position 之后
        if (other.empty()) return;
        forward_list_node_base* pos = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(position.node_));
        forward_list_node_base* last = &other.before_head_;
        while (last->next) last = last->next;

        // 接上
        forward_list_node_base* first = other.before_head_.next;
        other.before_head_.next = nullptr;
        last->next = pos->next;
        pos->next = first;
        size_ += other.size_;
        other.size_ = 0;
    }

    void splice_after(const_iterator position, forward_list&, const_iterator it) {
        // 将 it 之后的单个元素移到 position 之后
        forward_list_node_base* pos = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(position.node_));
        forward_list_node_base* before = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(it.node_));
        if (!before->next) return;
        forward_list_node_base* n = before->next;
        before->next = n->next;
        n->next = pos->next;
        pos->next = n;
        // size 不变（同一链表）或需要调整
    }

    void splice_after(const_iterator position, forward_list&,
                      const_iterator first, const_iterator last) {
        if (first == last) return;
        forward_list_node_base* pos = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(position.node_));
        forward_list_node_base* before = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(first.node_));
        forward_list_node_base* end = const_cast<forward_list_node_base*>(
            static_cast<const forward_list_node_base*>(last.node_));

        // 找到 (before, end) 的最后一个
        forward_list_node_base* last_node = before->next;
        while (last_node->next != end) last_node = last_node->next;

        // 摘下 (before, end) 段
        forward_list_node_base* first_node = before->next;
        before->next = end;

        // 插入到 pos 之后
        last_node->next = pos->next;
        pos->next = first_node;
    }

    void remove(const T& value) {
        forward_list_node_base* prev = &before_head_;
        while (prev->next) {
            forward_list_node<T>* cur = static_cast<forward_list_node<T>*>(prev->next);
            if (cur->data == value) {
                prev->next = cur->next;
                destroy_node(cur);
                --size_;
            } else {
                prev = prev->next;
            }
        }
    }

    template <typename Predicate>
    void remove_if(Predicate pred) {
        forward_list_node_base* prev = &before_head_;
        while (prev->next) {
            forward_list_node<T>* cur = static_cast<forward_list_node<T>*>(prev->next);
            if (pred(cur->data)) {
                prev->next = cur->next;
                destroy_node(cur);
                --size_;
            } else {
                prev = prev->next;
            }
        }
    }

    void unique() {
        if (!before_head_.next) return;
        forward_list_node<T>* cur = static_cast<forward_list_node<T>*>(before_head_.next);
        while (cur->next) {
            forward_list_node<T>* next = static_cast<forward_list_node<T>*>(cur->next);
            if (cur->data == next->data) {
                cur->next = next->next;
                destroy_node(next);
                --size_;
            } else {
                cur = next;
            }
        }
    }

    template <typename BinaryPredicate>
    void unique(BinaryPredicate pred) {
        if (!before_head_.next) return;
        forward_list_node<T>* cur = static_cast<forward_list_node<T>*>(before_head_.next);
        while (cur->next) {
            forward_list_node<T>* next = static_cast<forward_list_node<T>*>(cur->next);
            if (pred(cur->data, next->data)) {
                cur->next = next->next;
                destroy_node(next);
                --size_;
            } else {
                cur = next;
            }
        }
    }

    void merge(forward_list& other) {
        merge(other, mystl::less<T>());
    }

    template <typename Compare>
    void merge(forward_list& other, Compare comp) {
        if (this == &other) return;
        forward_list_node_base* prev = &before_head_;
        forward_list_node_base* other_prev = &other.before_head_;
        while (prev->next && other_prev->next) {
            forward_list_node<T>* a = static_cast<forward_list_node<T>*>(prev->next);
            forward_list_node<T>* b = static_cast<forward_list_node<T>*>(other_prev->next);
            if (comp(b->data, a->data)) {
                // 从 other 摘下 b
                other_prev->next = b->next;
                // 插入到 prev 之后
                b->next = prev->next;
                prev->next = b;
                ++size_;
                --other.size_;
            }
            prev = prev->next;
        }
        if (other_prev->next) {
            prev->next = other_prev->next;
            size_ += other.size_;
            other.before_head_.next = nullptr;
            other.size_ = 0;
        }
    }

    void sort() {
        sort(mystl::less<T>());
    }

    template <typename Compare>
    void sort(Compare comp) {
        if (!before_head_.next || !before_head_.next->next) return;
        // 归并排序
        forward_list carry;
        forward_list counter[64];
        int fill = 0;
        while (!empty()) {
            carry.splice_after(carry.cbefore_begin(), *this, cbefore_begin());
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

    void reverse() noexcept {
        forward_list_node_base* prev = nullptr;
        forward_list_node_base* cur = before_head_.next;
        forward_list_node_base* next = nullptr;
        while (cur) {
            next = cur->next;
            cur->next = prev;
            prev = cur;
            cur = next;
        }
        before_head_.next = prev;
    }

    void resize(size_type count, T value = T()) {
        // 先计数
        size_type sz = 0;
        forward_list_node_base* prev = &before_head_;
        while (prev->next && sz < count) {
            prev = prev->next;
            ++sz;
        }
        if (sz < count) {
            // 追加
            for (size_type i = sz; i < count; ++i) {
                insert_after_node(prev, create_node(value));
                prev = prev->next;
                ++size_;
            }
        } else if (prev->next) {
            // 删除多余
            forward_list_node_base* cur = prev->next;
            prev->next = nullptr;
            while (cur) {
                forward_list_node_base* next = cur->next;
                destroy_node(static_cast<forward_list_node<T>*>(cur));
                cur = next;
                --size_;
            }
        }
    }

    void swap(forward_list& other) noexcept {
        mystl::swap(before_head_.next, other.before_head_.next);
        mystl::swap(size_, other.size_);
        mystl::swap(node_alloc_, other.node_alloc_);
    }

    allocator_type get_allocator() const noexcept {
        return allocator_type();
    }
};

// 非成员比较运算符
template <typename T, typename Alloc>
bool operator==(const forward_list<T, Alloc>& a, const forward_list<T, Alloc>& b) {
    auto it1 = a.begin(), it2 = b.begin();
    for (; it1 != a.end() && it2 != b.end(); ++it1, ++it2)
        if (*it1 != *it2) return false;
    return it1 == a.end() && it2 == b.end();
}

template <typename T, typename Alloc>
bool operator!=(const forward_list<T, Alloc>& a, const forward_list<T, Alloc>& b) {
    return !(a == b);
}

template <typename T, typename Alloc>
bool operator<(const forward_list<T, Alloc>& a, const forward_list<T, Alloc>& b) {
    auto it1 = a.begin(), it2 = b.begin();
    for (; it1 != a.end() && it2 != b.end(); ++it1, ++it2) {
        if (*it1 < *it2) return true;
        if (*it2 < *it1) return false;
    }
    return it1 == a.end() && it2 != b.end();
}

template <typename T, typename Alloc>
bool operator>(const forward_list<T, Alloc>& a, const forward_list<T, Alloc>& b) {
    return b < a;
}
template <typename T, typename Alloc>
bool operator<=(const forward_list<T, Alloc>& a, const forward_list<T, Alloc>& b) {
    return !(b < a);
}
template <typename T, typename Alloc>
bool operator>=(const forward_list<T, Alloc>& a, const forward_list<T, Alloc>& b) {
    return !(a < b);
}

template <typename T, typename Alloc>
void swap(forward_list<T, Alloc>& a, forward_list<T, Alloc>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_FORWARD_LIST_H