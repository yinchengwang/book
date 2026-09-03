/*
 * deque.h - 双端队列容器
 *
 * 对标 std::deque<T>，纯手写实现
 * 数据结构：map-of-blocks（二级指针）
 * 核心：deque_iterator 携带 4 个指针（cur / first / last / node）
 * 大操作（insert/erase range）分离到 deque.tcc
 * 参考：libstdc++ stl_deque.h
 */

#ifndef MYSTL_DEQUE_H
#define MYSTL_DEQUE_H

#include "type_traits.h"
#include "allocator.h"
#include "iterator.h"
#include "utility.h"
#include <initializer_list>
#include <cstddef>   // ptrdiff_t
#include <stdexcept> // out_of_range

MYSTL_NAMESPACE_BEGIN

// ============================================================
// deque 默认参数
// ============================================================

// 每个 block 的元素数量 = 512 / sizeof(T)，最少 16
inline constexpr size_t _deque_block_size(size_t sz) {
    return sz < 512 ? 512 / sz : size_t(16);
}

// ============================================================
// deque_iterator
// ============================================================

template <typename T>
class deque_iterator {
public:
    using iterator_category = random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    T* cur;      // 当前元素
    T* first;    // block 起始
    T* last;     // block 末尾（最后一个元素之后）
    T** node;    // 指向 map 中当前 block 的指针

    deque_iterator() : cur(nullptr), first(nullptr), last(nullptr), node(nullptr) {}
    deque_iterator(T* c, T* f, T* l, T** n) : cur(c), first(f), last(l), node(n) {}

    reference operator*() const { return *cur; }
    pointer operator->() const { return cur; }

    // 跳转到下一个 block
    void _set_node(T** new_node) {
        node = new_node;
        first = *new_node;
        last = first + difference_type(_deque_block_size(sizeof(T)));
    }

    // 前缀 ++
    deque_iterator& operator++() {
        ++cur;
        if (cur == last) {
            _set_node(node + 1);
            cur = first;
        }
        return *this;
    }
    deque_iterator operator++(int) {
        deque_iterator tmp = *this;
        ++*this;
        return tmp;
    }

    // 前缀 --
    deque_iterator& operator--() {
        if (cur == first) {
            _set_node(node - 1);
            cur = last;
        }
        --cur;
        return *this;
    }
    deque_iterator operator--(int) {
        deque_iterator tmp = *this;
        --*this;
        return tmp;
    }

    // 随机访问
    deque_iterator& operator+=(difference_type n) {
        difference_type offset = n + (cur - first);
        if (offset >= 0 && offset < difference_type(_deque_block_size(sizeof(T))))
            cur += n;
        else {
            difference_type node_offset = offset > 0
                ? offset / difference_type(_deque_block_size(sizeof(T)))
                : -difference_type((-offset - 1) / _deque_block_size(sizeof(T)));
            _set_node(node + node_offset);
            cur = first + (offset - node_offset * difference_type(_deque_block_size(sizeof(T))));
        }
        return *this;
    }

    deque_iterator operator+(difference_type n) const {
        deque_iterator tmp = *this;
        return tmp += n;
    }

    deque_iterator& operator-=(difference_type n) {
        return *this += -n;
    }

    deque_iterator operator-(difference_type n) const {
        deque_iterator tmp = *this;
        return tmp += -n;
    }

    difference_type operator-(const deque_iterator& o) const {
        return difference_type(_deque_block_size(sizeof(T))) * (node - o.node)
             + (cur - first) - (o.cur - o.first);
    }

    reference operator[](difference_type n) const {
        return *(*this + n);
    }

    bool operator==(const deque_iterator& o) const { return cur == o.cur; }
    bool operator!=(const deque_iterator& o) const { return cur != o.cur; }
    bool operator<(const deque_iterator& o) const {
        return node == o.node ? cur < o.cur : node < o.node;
    }
    bool operator>(const deque_iterator& o) const { return o < *this; }
    bool operator<=(const deque_iterator& o) const { return !(o < *this); }
    bool operator>=(const deque_iterator& o) const { return !(*this < o); }
};

// 非成员 operator+ (left-hand side)
template <typename T>
deque_iterator<T> operator+(ptrdiff_t n, const deque_iterator<T>& it) {
    return it + n;
}

// const 迭代器
template <typename T>
class deque_const_iterator {
public:
    using iterator_category = random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;

    const T* cur;
    const T* first;
    const T* last;
    const T* const* node;

    deque_const_iterator() : cur(nullptr), first(nullptr), last(nullptr), node(nullptr) {}
    deque_const_iterator(const T* c, const T* f, const T* l, const T* const* n)
        : cur(c), first(f), last(l), node(n) {}
    deque_const_iterator(const deque_iterator<T>& it)
        : cur(it.cur), first(it.first), last(it.last),
          node(const_cast<const T* const*>(it.node)) {}

    reference operator*() const { return *cur; }
    pointer operator->() const { return cur; }

    void _set_node(const T* const* new_node) {
        node = new_node;
        first = *new_node;
        last = first + difference_type(_deque_block_size(sizeof(T)));
    }

    deque_const_iterator& operator++() {
        ++cur;
        if (cur == last) {
            _set_node(node + 1);
            cur = first;
        }
        return *this;
    }
    deque_const_iterator operator++(int) {
        deque_const_iterator tmp = *this;
        ++*this;
        return tmp;
    }
    deque_const_iterator& operator--() {
        if (cur == first) {
            _set_node(node - 1);
            cur = last;
        }
        --cur;
        return *this;
    }
    deque_const_iterator operator--(int) {
        deque_const_iterator tmp = *this;
        --*this;
        return tmp;
    }
    deque_const_iterator& operator+=(difference_type n) {
        difference_type offset = n + (cur - first);
        if (offset >= 0 && offset < difference_type(_deque_block_size(sizeof(T))))
            cur += n;
        else {
            difference_type node_offset = offset > 0
                ? offset / difference_type(_deque_block_size(sizeof(T)))
                : -difference_type((-offset - 1) / _deque_block_size(sizeof(T)));
            _set_node(node + node_offset);
            cur = first + (offset - node_offset * difference_type(_deque_block_size(sizeof(T))));
        }
        return *this;
    }
    deque_const_iterator operator+(difference_type n) const {
        deque_const_iterator tmp = *this;
        return tmp += n;
    }
    deque_const_iterator& operator-=(difference_type n) { return *this += -n; }
    deque_const_iterator operator-(difference_type n) const {
        deque_const_iterator tmp = *this;
        return tmp += -n;
    }
    difference_type operator-(const deque_const_iterator& o) const {
        return difference_type(_deque_block_size(sizeof(T))) * (node - o.node)
             + (cur - first) - (o.cur - o.first);
    }
    reference operator[](difference_type n) const { return *(*this + n); }

    bool operator==(const deque_const_iterator& o) const { return cur == o.cur; }
    bool operator!=(const deque_const_iterator& o) const { return cur != o.cur; }
    bool operator<(const deque_const_iterator& o) const {
        return node == o.node ? cur < o.cur : node < o.node;
    }
    bool operator>(const deque_const_iterator& o) const { return o < *this; }
    bool operator<=(const deque_const_iterator& o) const { return !(o < *this); }
    bool operator>=(const deque_const_iterator& o) const { return !(*this < o); }
};

template <typename T>
deque_const_iterator<T> operator+(ptrdiff_t n, const deque_const_iterator<T>& it) {
    return it + n;
}


// ============================================================
// deque 类模板（前向声明）
// ============================================================

template <typename T, typename Alloc = allocator<T>>
class deque {
public:
    // 类型定义
    using value_type             = T;
    using allocator_type         = Alloc;
    using reference              = T&;
    using const_reference        = const T&;
    using pointer                = T*;
    using const_pointer          = const T*;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;
    using iterator               = deque_iterator<T>;
    using const_iterator         = deque_const_iterator<T>;
    using reverse_iterator       = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;

    // block 大小
    static constexpr size_type _block_size = _deque_block_size(sizeof(T));

    // 节点分配器（rebind 到 T）
    using map_allocator = typename Alloc::template rebind<T*>::other;

private:
    // 数据成员
    iterator   start_;         // 起始迭代器
    iterator   finish_;        // 结束迭代器
    T**        map_;           // map 数组
    size_type  map_size_;      // map 容量
    Alloc      alloc_;

    // 分配一个 block
    T* _allocate_block() {
        return alloc_.allocate(_block_size);
    }

    // 释放一个 block
    void _deallocate_block(T* p) {
        alloc_.deallocate(p, _block_size);
    }

    // 初始化 map
    void _init_map(size_type num_elements) {
        // 需要的 block 数
        size_type num_blocks = (num_elements + _block_size - 1) / _block_size;
        if (num_blocks == 0) num_blocks = 1;

        // map 大小 = num_blocks + 2（前后各留一个冗余）
        map_size_ = num_blocks + 2;
        map_ = map_allocator().allocate(map_size_);

        // 初始化 map 指针
        for (size_type i = 0; i < map_size_; ++i)
            map_[i] = nullptr;

        // 中间位置开始分配 block
        T** start_block = map_ + (map_size_ - num_blocks) / 2;
        for (size_type i = 0; i < num_blocks; ++i)
            start_block[i] = _allocate_block();

        // 设置迭代器
        start_.first = start_block[0];
        start_.last  = start_block[0] + _block_size;
        start_.cur   = start_block[0];
        start_.node  = start_block;

        finish_.first = start_block[num_blocks - 1];
        finish_.last  = start_block[num_blocks - 1] + _block_size;
        finish_.cur   = start_block[num_blocks - 1] + (num_elements % _block_size);
        finish_.node  = start_block + num_blocks - 1;

        // 如果正好是 block 边界
        if (finish_.cur == finish_.last) {
            // 实际上不需要调整，迭代器语义正确
        }
    }

    // 重新分配 map（扩容或收缩）
    void _reallocate_map(size_type new_map_size, bool keep_front) {
        T** new_map = map_allocator().allocate(new_map_size);
        // 初始化
        for (size_type i = 0; i < new_map_size; ++i)
            new_map[i] = nullptr;

        // 复制旧的 block 指针
        size_type old_num_blocks = (finish_.node - start_.node) + 1;
        size_type new_start = keep_front ? 0 : (new_map_size - old_num_blocks) / 2;
        if (keep_front) new_start = (new_map_size - old_num_blocks) / 2;

        for (size_type i = 0; i < old_num_blocks; ++i)
            new_map[new_start + i] = map_[start_.node - map_ + i];

        // 更新迭代器
        difference_type start_node_offset = start_.node - map_;
        difference_type finish_node_offset = finish_.node - map_;

        // 释放旧 map
        map_allocator().deallocate(map_, map_size_);

        map_ = new_map;
        map_size_ = new_map_size;

        start_.node = map_ + new_start;
        finish_.node = start_.node + (finish_node_offset - start_node_offset);
    }

    // 在 front 预留空间
    void _reserve_map_at_front(size_type nodes_needed = 1) {
        if (nodes_needed > size_type(start_.node - map_))
            _reallocate_map(map_size_ + nodes_needed + 1, false);
    }

    // 在 back 预留空间
    void _reserve_map_at_back(size_type nodes_needed = 1) {
        if (nodes_needed > size_type((map_ + map_size_) - (finish_.node + 1)))
            _reallocate_map(map_size_ + nodes_needed + 1, true);
    }

    // 在 front 添加新 block
    void _push_front_aux(const T& value) {
        _reserve_map_at_front(1);
        T* new_block = _allocate_block();
        start_.node -= 1;
        *start_.node = new_block;
        start_.first = new_block;
        start_.last  = new_block + _block_size;
        start_.cur   = start_.last - 1;  // 最后一个位置
        alloc_.construct(start_.cur, value);
    }

    void _push_front_aux(T&& value) {
        _reserve_map_at_front(1);
        T* new_block = _allocate_block();
        start_.node -= 1;
        *start_.node = new_block;
        start_.first = new_block;
        start_.last  = new_block + _block_size;
        start_.cur   = start_.last - 1;
        alloc_.construct(start_.cur, mystl::move(value));
    }

    // 在 back 添加新 block
    void _push_back_aux(const T& value) {
        _reserve_map_at_back(1);
        T* new_block = _allocate_block();
        finish_.node += 1;
        *finish_.node = new_block;
        finish_.first = new_block;
        finish_.last  = new_block + _block_size;
        finish_.cur   = new_block;
        alloc_.construct(finish_.cur, value);
    }

    void _push_back_aux(T&& value) {
        _reserve_map_at_back(1);
        T* new_block = _allocate_block();
        finish_.node += 1;
        *finish_.node = new_block;
        finish_.first = new_block;
        finish_.last  = new_block + _block_size;
        finish_.cur   = new_block;
        alloc_.construct(finish_.cur, mystl::move(value));
    }

    // 销毁 [first, last) 范围内元素并释放 block
    void _destroy_blocks(iterator first, iterator last) {
        iterator cur = first;
        while (cur.node != last.node) {
            // 销毁当前 block 所有元素
            for (T* p = cur.cur; p != cur.last; ++p)
                alloc_.destroy(p);
            // 释放 block
            _deallocate_block(*cur.node);
            cur._set_node(cur.node + 1);
            cur.cur = cur.first;
        }
        // 销毁最后一个 block
        for (T* p = cur.cur; p != last.cur; ++p)
            alloc_.destroy(p);
        if (cur.node != first.node || cur.cur != cur.first) {
            // 不释放最后一个 block（留给调用方）
        }
    }

public:
    // ============================================================
    // 构造函数
    // ============================================================

    deque() : map_(nullptr), map_size_(0) {
        _init_map(0);
    }

    explicit deque(const Alloc& alloc) : map_(nullptr), map_size_(0), alloc_(alloc) {
        _init_map(0);
    }

    explicit deque(size_type n, const T& value = T(), const Alloc& alloc = Alloc())
        : alloc_(alloc) {
        if (n == 0) {
            map_ = nullptr; map_size_ = 0;
            _init_map(0);
            return;
        }
        _init_map(n);
        // 填充
        for (iterator it = begin(); it != end(); ++it)
            alloc_.construct(it.cur, value);
    }

    template <typename InputIterator>
    deque(InputIterator first, InputIterator last,
          enable_if_t<!is_integral<InputIterator>::value, int> = 0)
        : map_(nullptr), map_size_(0) {
        _init_map(0);
        for (; first != last; ++first)
            push_back(*first);
    }

    deque(const deque& other) : map_(nullptr), map_size_(0), alloc_(other.alloc_) {
        _init_map(0);
        for (const auto& x : other)
            push_back(x);
    }

    deque(deque&& other) noexcept
        : start_(other.start_), finish_(other.finish_),
          map_(other.map_), map_size_(other.map_size_),
          alloc_(mystl::move(other.alloc_)) {
        other.map_ = nullptr;
        other.map_size_ = 0;
        other._init_map(0);
    }

    deque(std::initializer_list<T> il, const Alloc& alloc = Alloc())
        : map_(nullptr), map_size_(0), alloc_(alloc) {
        _init_map(0);
        for (const auto& x : il)
            push_back(x);
    }

    ~deque() {
        if (map_) {
            // 销毁所有元素
            for (iterator it = begin(); it != end(); ++it)
                alloc_.destroy(it.cur);
            // 释放所有 block
            for (T** p = start_.node; p <= finish_.node; ++p)
                _deallocate_block(*p);
            // 释放 map
            map_allocator().deallocate(map_, map_size_);
        }
    }

    // ============================================================
    // 赋值
    // ============================================================

    deque& operator=(const deque& other) {
        if (this != &other) assign(other.begin(), other.end());
        return *this;
    }

    deque& operator=(deque&& other) noexcept {
        if (this != &other) {
            // 释放旧资源
            this->~deque();
            // 接管
            start_ = other.start_;
            finish_ = other.finish_;
            map_ = other.map_;
            map_size_ = other.map_size_;
            alloc_ = mystl::move(other.alloc_);
            other.map_ = nullptr;
            other.map_size_ = 0;
            other._init_map(0);
        }
        return *this;
    }

    deque& operator=(std::initializer_list<T> il) {
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

    bool empty() const noexcept { return start_.cur == finish_.cur; }
    size_type size() const noexcept { return size_type(finish_ - start_); }
    size_type max_size() const noexcept { return alloc_.max_size(); }

    // ============================================================
    // 元素访问
    // ============================================================

    reference operator[](size_type n) { return start_[difference_type(n)]; }
    const_reference operator[](size_type n) const { return start_[difference_type(n)]; }

    reference at(size_type n) {
        if (n >= size()) throw std::out_of_range("deque::at");
        return (*this)[n];
    }
    const_reference at(size_type n) const {
        if (n >= size()) throw std::out_of_range("deque::at");
        return (*this)[n];
    }

    reference front() { return *start_; }
    const_reference front() const { return *start_; }
    reference back() { iterator tmp = finish_; --tmp; return *tmp; }
    const_reference back() const { const_iterator tmp = finish_; --tmp; return *tmp; }

    // ============================================================
    // 迭代器
    // ============================================================

    iterator begin() noexcept { return start_; }
    const_iterator begin() const noexcept { return const_iterator(start_); }
    const_iterator cbegin() const noexcept { return const_iterator(start_); }
    iterator end() noexcept { return finish_; }
    const_iterator end() const noexcept { return const_iterator(finish_); }
    const_iterator cend() const noexcept { return const_iterator(finish_); }

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
        if (start_.cur != start_.first) {
            // 当前 block 还有空间
            --start_.cur;
            alloc_.construct(start_.cur, value);
        } else {
            _push_front_aux(value);
        }
    }

    void push_front(T&& value) {
        if (start_.cur != start_.first) {
            --start_.cur;
            alloc_.construct(start_.cur, mystl::move(value));
        } else {
            _push_front_aux(mystl::move(value));
        }
    }

    template <typename... Args>
    reference emplace_front(Args&&... args) {
        if (start_.cur != start_.first) {
            --start_.cur;
            alloc_.construct(start_.cur, T(mystl::forward<Args>(args)...));
        } else {
            _reserve_map_at_front(1);
            T* new_block = _allocate_block();
            start_.node -= 1;
            *start_.node = new_block;
            start_.first = new_block;
            start_.last  = new_block + _block_size;
            start_.cur   = start_.last - 1;
            alloc_.construct(start_.cur, T(mystl::forward<Args>(args)...));
        }
        return *start_;
    }

    void pop_front() {
        alloc_.destroy(start_.cur);
        ++start_.cur;
        if (start_.cur == start_.last) {
            // 当前 block 用完了，释放它
            _deallocate_block(*start_.node);
            start_._set_node(start_.node + 1);
            start_.cur = start_.first;
        }
    }

    void push_back(const T& value) {
        if (finish_.cur != finish_.last) {
            alloc_.construct(finish_.cur, value);
            ++finish_.cur;
        } else {
            _push_back_aux(value);
            ++finish_.cur;  // _push_back_aux 构造了元素但没移动 cur
        }
    }

    void push_back(T&& value) {
        if (finish_.cur != finish_.last) {
            alloc_.construct(finish_.cur, mystl::move(value));
            ++finish_.cur;
        } else {
            _push_back_aux(mystl::move(value));
            ++finish_.cur;
        }
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        if (finish_.cur != finish_.last) {
            alloc_.construct(finish_.cur, T(mystl::forward<Args>(args)...));
            ++finish_.cur;
        } else {
            _reserve_map_at_back(1);
            T* new_block = _allocate_block();
            finish_.node += 1;
            *finish_.node = new_block;
            finish_.first = new_block;
            finish_.last  = new_block + _block_size;
            finish_.cur   = new_block;
            alloc_.construct(finish_.cur, T(mystl::forward<Args>(args)...));
            ++finish_.cur;
        }
        iterator tmp = finish_; --tmp;
        return *tmp;
    }

    void pop_back() {
        if (finish_.cur != finish_.first) {
            --finish_.cur;
            alloc_.destroy(finish_.cur);
        } else {
            // 退到前一个 block
            _deallocate_block(*finish_.node);
            finish_._set_node(finish_.node - 1);
            finish_.cur = finish_.last;
            --finish_.cur;
            alloc_.destroy(finish_.cur);
        }
    }

    // insert — 在 position 前插入
    iterator insert(const_iterator position, const T& value) {
        if (position == begin()) {
            push_front(value);
            return begin();
        }
        if (position == end()) {
            push_back(value);
            iterator tmp = end(); --tmp;
            return tmp;
        }
        // 通用插入：从后往前移动
        return _insert_aux(position, value);
    }

    iterator insert(const_iterator position, T&& value) {
        if (position == begin()) {
            push_front(mystl::move(value));
            return begin();
        }
        if (position == end()) {
            push_back(mystl::move(value));
            iterator tmp = end(); --tmp;
            return tmp;
        }
        return _insert_aux(position, mystl::move(value));
    }

    iterator insert(const_iterator position, size_type count, const T& value) {
        if (count == 0) return begin() + (position - begin());
        // 从 position 开始，逐个插入（简单实现，大 count 时可优化）
        iterator ret = insert(position, value);
        for (size_type i = 1; i < count; ++i)
            insert(position, value);
        return ret;
    }

    template <typename InputIterator,
              typename = typename enable_if<!is_integral<InputIterator>::value>::type>
    iterator insert(const_iterator position, InputIterator first, InputIterator last) {
        if (first == last) return begin() + (position - begin());
        iterator ret = insert(position, *first);
        ++first;
        for (; first != last; ++first)
            insert(position, *first);
        return ret;
    }

    // erase — 删除单个元素
    iterator erase(const_iterator position) {
        if (position == begin()) {
            pop_front();
            return begin();
        }
        if (position + 1 == end()) {
            pop_back();
            return end();
        }
        return _erase_aux(position);
    }

    // erase — 删除范围 [first, last)
    iterator erase(const_iterator first, const_iterator last) {
        if (first == last) return begin() + (first - begin());
        if (first == begin() && last == end()) {
            clear();
            return begin();
        }

        // 简单实现：从前往后逐个删除
        // 每次删除后，后面的元素前移，但 last 也会相应变化
        // 所以先计算删除个数，再循环删除
        size_type n = last - first;
        size_type offset = first - begin();

        for (size_type i = 0; i < n; ++i) {
            erase(begin() + offset);
        }

        return begin() + offset;
    }

    void clear() noexcept {
        // 销毁所有元素
        for (iterator it = begin(); it != end(); ++it)
            alloc_.destroy(it.cur);
        // 释放除首尾 block 外的所有 block（保留一个 block 给空 deque）
        T** first_block = start_.node;
        T** last_block = finish_.node;
        if (finish_.cur == finish_.first) {
            // finish_ 在 block 开头，最后一个 block 为空
            last_block = finish_.node - 1;
        }
        for (T** p = first_block; p <= last_block; ++p)
            _deallocate_block(*p);

        // 重置到空状态
        map_allocator().deallocate(map_, map_size_);
        map_ = nullptr;
        map_size_ = 0;
        _init_map(0);
    }

    void resize(size_type count, T value = T()) {
        if (count < size()) {
            while (size() > count) pop_back();
        } else if (count > size()) {
            while (size() < count) push_back(value);
        }
    }

    void swap(deque& other) noexcept {
        mystl::swap(start_, other.start_);
        mystl::swap(finish_, other.finish_);
        mystl::swap(map_, other.map_);
        mystl::swap(map_size_, other.map_size_);
        mystl::swap(alloc_, other.alloc_);
    }

    allocator_type get_allocator() const noexcept {
        return alloc_;
    }

private:
    // 通用 insert 辅助
    template <typename ValT>
    iterator _insert_aux(const_iterator position, ValT&& value) {
        difference_type idx = position - begin();
        difference_type n = finish_ - start_;

        if (idx < n / 2) {
            // 靠近 front：前移前面的元素
            push_front(mystl::move(front()));
            iterator front_it = begin();
            iterator pos_it = begin() + idx;
            for (iterator it = front_it + 1; it <= pos_it; ++it) {
                *const_cast<T*>(&*(it - 1)) = mystl::move(*it);
            }
            *(begin() + idx) = mystl::forward<ValT>(value);
        } else {
            // 靠近 back：后移后面的元素
            push_back(mystl::move(back()));
            iterator pos_it = begin() + idx;
            for (iterator it = end() - 2; it >= pos_it; --it) {
                *const_cast<T*>(&*(it + 1)) = mystl::move(*it);
            }
            *(begin() + idx) = mystl::forward<ValT>(value);
        }
        return begin() + idx;
    }

    // 通用 erase 辅助
    iterator _erase_aux(const_iterator position) {
        difference_type idx = position - begin();
        difference_type n = finish_ - start_;

        if (idx < n / 2) {
            // 靠近 front：前移前面的元素
            iterator pos_it = begin() + idx;
            for (iterator it = pos_it; it != begin(); --it) {
                *const_cast<T*>(&*it) = mystl::move(*(it - 1));
            }
            pop_front();
        } else {
            // 靠近 back：后移后面的元素
            iterator pos_it = begin() + idx;
            for (iterator it = pos_it; it != end() - 1; ++it) {
                *const_cast<T*>(&*it) = mystl::move(*(it + 1));
            }
            pop_back();
        }
        return begin() + idx;
    }
};


// ============================================================
// 非成员比较运算符
// ============================================================

template <typename T, typename Alloc>
bool operator==(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
    if (a.size() != b.size()) return false;
    auto it1 = a.begin(), it2 = b.begin();
    for (; it1 != a.end(); ++it1, ++it2)
        if (*it1 != *it2) return false;
    return true;
}

template <typename T, typename Alloc>
bool operator!=(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
    return !(a == b);
}

template <typename T, typename Alloc>
bool operator<(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
    auto it1 = a.begin(), it2 = b.begin();
    for (; it1 != a.end() && it2 != b.end(); ++it1, ++it2) {
        if (*it1 < *it2) return true;
        if (*it2 < *it1) return false;
    }
    return it1 == a.end() && it2 != b.end();
}

template <typename T, typename Alloc>
bool operator>(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
    return b < a;
}

template <typename T, typename Alloc>
bool operator<=(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
    return !(b < a);
}

template <typename T, typename Alloc>
bool operator>=(const deque<T, Alloc>& a, const deque<T, Alloc>& b) {
    return !(a < b);
}

template <typename T, typename Alloc>
void swap(deque<T, Alloc>& a, deque<T, Alloc>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_DEQUE_H