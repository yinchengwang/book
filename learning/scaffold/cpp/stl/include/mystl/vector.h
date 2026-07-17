/*
 * vector.h - 动态数组容器
 *
 * 对标 std::vector<T>，纯手写实现
 * 数据结构：三指针模型（start_ / finish_ / end_of_storage_）
 * 扩容策略：2 倍增长
 * 参考：libstdc++ stl_vector.h
 */

#ifndef MYSTL_VECTOR_H
#define MYSTL_VECTOR_H

#include "type_traits.h"
#include "allocator.h"
#include "iterator.h"
#include "utility.h"
#include <initializer_list>
#include <stdexcept>   // out_of_range, length_error
#include <new>         // placement new, bad_alloc

MYSTL_NAMESPACE_BEGIN

// ============================================================
// vector 类模板
// ============================================================

template <typename T, typename Alloc = allocator<T>>
class vector {
public:
    // 类型定义
    using value_type        = T;
    using allocator_type    = Alloc;
    using iterator          = T*;
    using const_iterator    = const T*;
    using reverse_iterator  = mystl::reverse_iterator<iterator>;
    using const_reverse_iterator = mystl::reverse_iterator<const_iterator>;
    using size_type         = size_t;
    using difference_type   = ptrdiff_t;
    using pointer           = T*;
    using const_pointer     = const T*;
    using reference         = T&;
    using const_reference   = const T&;

private:
    pointer start_;          // 起始位置
    pointer finish_;         // 结束位置（最后一个元素之后）
    pointer end_of_storage_; // 存储末尾
    Alloc alloc_;

    // 析构 [first, last) 范围内的对象
    void destroy_range(pointer first, pointer last) {
        for (; first != last; ++first) first->~T();
    }

    // 分配新空间并移动数据（扩容核心）
    void reallocate_and_move(size_type new_capacity) {
        pointer new_start = alloc_.allocate(new_capacity);
        pointer new_finish = new_start;
        try {
            for (pointer p = start_; p != finish_; ++p, ++new_finish)
                alloc_.construct(new_finish, mystl::move(*p));
        } catch (...) {
            destroy_range(new_start, new_finish);
            alloc_.deallocate(new_start, new_capacity);
            throw;
        }
        destroy_range(start_, finish_);
        alloc_.deallocate(start_, capacity());
        start_ = new_start;
        finish_ = new_finish;
        end_of_storage_ = start_ + new_capacity;
    }

    // 计算扩容目标大小
    size_type recommend_size(size_type new_size) const {
        const size_type max_sz = max_size();
        if (new_size > max_sz) throw std::length_error("vector::reserve");
        size_type cap = capacity();
        if (cap >= max_sz / 2) return max_sz;
        return cap * 2 > new_size ? cap * 2 : new_size;
    }

public:
    // ============================================================
    // 构造函数
    // ============================================================

    vector() noexcept : start_(nullptr), finish_(nullptr), end_of_storage_(nullptr) {}

    explicit vector(size_type n, const T& value = T()) {
        start_ = alloc_.allocate(n);
        finish_ = start_;
        end_of_storage_ = start_ + n;
        try {
            for (size_type i = 0; i < n; ++i, ++finish_)
                alloc_.construct(finish_, value);
        } catch (...) {
            destroy_range(start_, finish_);
            alloc_.deallocate(start_, n);
            throw;
        }
    }

    template <typename InputIterator>
    vector(InputIterator first, InputIterator last,
           enable_if_t<!is_integral<InputIterator>::value, int> = 0) {
        size_type n = 0;
        for (InputIterator it = first; it != last; ++it) ++n;
        start_ = alloc_.allocate(n);
        finish_ = start_;
        end_of_storage_ = start_ + n;
        try {
            for (; first != last; ++first, ++finish_)
                alloc_.construct(finish_, *first);
        } catch (...) {
            destroy_range(start_, finish_);
            alloc_.deallocate(start_, n);
            throw;
        }
    }

    vector(const vector& other) {
        size_type n = other.size();
        start_ = alloc_.allocate(n);
        finish_ = start_;
        end_of_storage_ = start_ + n;
        try {
            for (size_type i = 0; i < n; ++i, ++finish_)
                alloc_.construct(finish_, other.start_[i]);
        } catch (...) {
            destroy_range(start_, finish_);
            alloc_.deallocate(start_, n);
            throw;
        }
    }

    vector(vector&& other) noexcept
        : start_(other.start_), finish_(other.finish_),
          end_of_storage_(other.end_of_storage_) {
        other.start_ = nullptr;
        other.finish_ = nullptr;
        other.end_of_storage_ = nullptr;
    }

    vector(std::initializer_list<T> il) {
        size_type n = il.size();
        start_ = alloc_.allocate(n);
        finish_ = start_;
        end_of_storage_ = start_ + n;
        try {
            for (const auto& x : il) {
                alloc_.construct(finish_, x);
                ++finish_;
            }
        } catch (...) {
            destroy_range(start_, finish_);
            alloc_.deallocate(start_, n);
            throw;
        }
    }

    ~vector() {
        destroy_range(start_, finish_);
        alloc_.deallocate(start_, capacity());
    }

    // ============================================================
    // 赋值运算符
    // ============================================================

    vector& operator=(const vector& other) {
        if (this != &other) assign(other.begin(), other.end());
        return *this;
    }

    vector& operator=(vector&& other) noexcept {
        if (this != &other) {
            clear();
            alloc_.deallocate(start_, capacity());
            start_ = other.start_;
            finish_ = other.finish_;
            end_of_storage_ = other.end_of_storage_;
            other.start_ = nullptr;
            other.finish_ = nullptr;
            other.end_of_storage_ = nullptr;
        }
        return *this;
    }

    vector& operator=(std::initializer_list<T> il) {
        assign(il.begin(), il.end());
        return *this;
    }

    // ============================================================
    // 容量操作
    // ============================================================

    bool empty() const noexcept { return start_ == finish_; }
    size_type size() const noexcept { return static_cast<size_type>(finish_ - start_); }
    size_type capacity() const noexcept { return static_cast<size_type>(end_of_storage_ - start_); }
    size_type max_size() const noexcept { return alloc_.max_size(); }

    void reserve(size_type new_cap) {
        if (new_cap > capacity()) reallocate_and_move(new_cap);
    }

    void shrink_to_fit() {
        if (capacity() > size()) {
            vector<T> tmp(begin(), end());
            tmp.swap(*this);
        }
    }

    // ============================================================
    // 元素访问
    // ============================================================

    reference operator[](size_type n) { return start_[n]; }
    const_reference operator[](size_type n) const { return start_[n]; }

    reference at(size_type n) {
        if (n >= size()) throw std::out_of_range("vector::at");
        return (*this)[n];
    }
    const_reference at(size_type n) const {
        if (n >= size()) throw std::out_of_range("vector::at");
        return (*this)[n];
    }

    reference front() { return *start_; }
    const_reference front() const { return *start_; }
    reference back() { return *(finish_ - 1); }
    const_reference back() const { return *(finish_ - 1); }
    pointer data() noexcept { return start_; }
    const_pointer data() const noexcept { return start_; }

    // ============================================================
    // 迭代器
    // ============================================================

    iterator begin() noexcept { return start_; }
    const_iterator begin() const noexcept { return start_; }
    const_iterator cbegin() const noexcept { return start_; }
    iterator end() noexcept { return finish_; }
    const_iterator end() const noexcept { return finish_; }
    const_iterator cend() const noexcept { return finish_; }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    // ============================================================
    // 修改器
    // ============================================================

    void assign(size_type count, const T& value) {
        clear();
        for (size_type i = 0; i < count; ++i) push_back(value);
    }

    template <typename InputIterator>
    void assign(InputIterator first, InputIterator last) {
        clear();
        for (; first != last; ++first) push_back(*first);
    }

    void push_back(const T& value) {
        if (finish_ != end_of_storage_) {
            alloc_.construct(finish_, value);
            ++finish_;
        } else {
            reallocate_and_move(recommend_size(size() + 1));
            alloc_.construct(finish_, value);
            ++finish_;
        }
    }

    void push_back(T&& value) {
        if (finish_ != end_of_storage_) {
            alloc_.construct(finish_, mystl::move(value));
            ++finish_;
        } else {
            reallocate_and_move(recommend_size(size() + 1));
            alloc_.construct(finish_, mystl::move(value));
            ++finish_;
        }
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        if (finish_ != end_of_storage_) {
            alloc_.construct(finish_, mystl::forward<Args>(args)...);
            ++finish_;
        } else {
            reallocate_and_move(recommend_size(size() + 1));
            alloc_.construct(finish_, mystl::forward<Args>(args)...);
            ++finish_;
        }
        return back();
    }

    void pop_back() {
        --finish_;
        alloc_.destroy(finish_);
    }

    // insert — 在指定位置插入
    iterator insert(const_iterator position, const T& value) {
        const size_type pos_idx = position - begin();
        if (finish_ != end_of_storage_ && position == finish_) {
            push_back(value);
        } else if (finish_ != end_of_storage_) {
            alloc_.construct(finish_, mystl::move(*(finish_ - 1)));
            ++finish_;
            for (size_type i = size() - 1; i > pos_idx; --i)
                start_[i] = mystl::move(start_[i - 1]);
            const_cast<T&>(*position) = value;
        } else {
            const size_type old_size = size();
            const size_type new_sz = recommend_size(old_size + 1);
            pointer new_start = alloc_.allocate(new_sz);
            pointer new_finish = new_start;
            try {
                for (size_type i = 0; i < pos_idx; ++i, ++new_finish)
                    alloc_.construct(new_finish, mystl::move(start_[i]));
                alloc_.construct(new_finish, value);
                ++new_finish;
                for (size_type i = pos_idx; i < old_size; ++i, ++new_finish)
                    alloc_.construct(new_finish, mystl::move(start_[i]));
            } catch (...) {
                destroy_range(new_start, new_finish);
                alloc_.deallocate(new_start, new_sz);
                throw;
            }
            destroy_range(start_, finish_);
            alloc_.deallocate(start_, capacity());
            start_ = new_start;
            finish_ = new_finish;
            end_of_storage_ = start_ + new_sz;
        }
        return begin() + pos_idx;
    }

    iterator insert(const_iterator position, T&& value) {
        return insert(position, value);
    }

    // insert — 范围插入 [first, last)，仅当 InputIterator 不是整数类型时匹配
    template <typename InputIterator,
              typename = typename enable_if<!is_integral<InputIterator>::value>::type>
    iterator insert(const_iterator position, InputIterator first, InputIterator last) {
        using iter_cat = typename iterator_traits<InputIterator>::iterator_category;
        return insert_range(position, first, last, iter_cat());
    }

    iterator insert(const_iterator position, size_type count, const T& value) {
        if (count == 0) return begin() + (position - begin());
        const size_type pos_idx = position - begin();
        const size_type old_size = size();
        if (capacity() >= old_size + count) {
            const size_type elems_after = old_size - pos_idx;
            for (size_type i = 0; i < count; ++i)
                alloc_.construct(finish_ + i, mystl::move(*(finish_ - count + i)));
            finish_ += count;
            for (size_type i = count; i < elems_after + count; ++i)
                start_[old_size + i - count] = mystl::move(start_[old_size - count + i]);
            for (size_type i = 0; i < count; ++i)
                start_[pos_idx + i] = value;
        } else {
            const size_type new_sz = recommend_size(old_size + count);
            pointer new_start = alloc_.allocate(new_sz);
            pointer new_finish = new_start;
            try {
                for (size_type i = 0; i < pos_idx; ++i, ++new_finish)
                    alloc_.construct(new_finish, mystl::move(start_[i]));
                for (size_type i = 0; i < count; ++i, ++new_finish)
                    alloc_.construct(new_finish, value);
                for (size_type i = pos_idx; i < old_size; ++i, ++new_finish)
                    alloc_.construct(new_finish, mystl::move(start_[i]));
            } catch (...) {
                destroy_range(new_start, new_finish);
                alloc_.deallocate(new_start, new_sz);
                throw;
            }
            destroy_range(start_, finish_);
            alloc_.deallocate(start_, capacity());
            start_ = new_start;
            finish_ = new_finish;
            end_of_storage_ = start_ + new_sz;
        }
        return begin() + pos_idx;
    }

    // insert_range — 范围插入辅助（input_iterator_tag）
    template <typename InputIterator>
    iterator insert_range(const_iterator position, InputIterator first, InputIterator last,
                          input_iterator_tag) {
        const size_type pos_idx = position - begin();
        while (first != last) {
            insert(begin() + pos_idx, *first);
            ++first;
        }
        return begin() + pos_idx;
    }

    // insert_range — 范围插入辅助（forward_iterator_tag 及以上）
    template <typename ForwardIterator>
    iterator insert_range(const_iterator position, ForwardIterator first, ForwardIterator last,
                          forward_iterator_tag) {
        const size_type count = static_cast<size_type>(mystl::distance(first, last));
        if (count == 0) return begin() + (position - begin());

        const size_type pos_idx = position - begin();
        const size_type old_size = size();
        if (capacity() >= old_size + count) {
            const size_type elems_after = old_size - pos_idx;
            // 移动后半部分到新位置
            for (size_type i = 0; i < elems_after; ++i) {
                if (i < count) {
                    alloc_.construct(finish_ + i, mystl::move(*(finish_ - count + i)));
                }
            }
            finish_ += count;
            // 反向移动剩余元素
            for (size_type i = elems_after; i > 0; --i) {
                start_[pos_idx + i + count - 1] = mystl::move(start_[pos_idx + i - 1]);
            }
            // 复制新元素
            ForwardIterator it = first;
            for (size_type i = 0; i < count; ++i, ++it) {
                start_[pos_idx + i] = *it;
            }
        } else {
            const size_type new_sz = recommend_size(old_size + count);
            pointer new_start = alloc_.allocate(new_sz);
            pointer new_finish = new_start;
            try {
                // 复制 pos_idx 前的元素
                for (size_type i = 0; i < pos_idx; ++i, ++new_finish)
                    alloc_.construct(new_finish, mystl::move(start_[i]));
                // 复制新范围
                for (; first != last; ++first, ++new_finish)
                    alloc_.construct(new_finish, *first);
                // 复制剩余元素
                for (size_type i = pos_idx; i < old_size; ++i, ++new_finish)
                    alloc_.construct(new_finish, mystl::move(start_[i]));
            } catch (...) {
                destroy_range(new_start, new_finish);
                alloc_.deallocate(new_start, new_sz);
                throw;
            }
            destroy_range(start_, finish_);
            alloc_.deallocate(start_, capacity());
            start_ = new_start;
            finish_ = new_finish;
            end_of_storage_ = start_ + new_sz;
        }
        return begin() + pos_idx;
    }

    template <typename... Args>
    iterator emplace(const_iterator position, Args&&... args) {
        const size_type pos_idx = position - begin();
        if (finish_ != end_of_storage_ && position == finish_) {
            emplace_back(mystl::forward<Args>(args)...);
        } else {
            insert(position, T(mystl::forward<Args>(args)...));
        }
        return begin() + pos_idx;
    }

    iterator erase(const_iterator position) {
        return erase(position, position + 1);
    }

    iterator erase(const_iterator first, const_iterator last) {
        if (first != last) {
            const size_type count = last - first;
            pointer dst = const_cast<T*>(first);
            pointer src = const_cast<T*>(last);
            for (; src != finish_; ++dst, ++src)
                *dst = mystl::move(*src);
            destroy_range(finish_ - count, finish_);
            finish_ -= count;
        }
        return iterator(const_cast<T*>(first));
    }

    void clear() noexcept {
        destroy_range(start_, finish_);
        finish_ = start_;
    }

    void resize(size_type count, T value = T()) {
        if (count < size()) {
            destroy_range(start_ + count, finish_);
            finish_ = start_ + count;
        } else if (count > size()) {
            if (count > capacity()) reserve(count);
            while (size() < count) push_back(value);
        }
    }

    void swap(vector& other) noexcept {
        mystl::swap(start_, other.start_);
        mystl::swap(finish_, other.finish_);
        mystl::swap(end_of_storage_, other.end_of_storage_);
    }
};

// ============================================================
// 非成员函数
// ============================================================

template <typename T>
bool operator==(const vector<T>& a, const vector<T>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}
template <typename T>
bool operator!=(const vector<T>& a, const vector<T>& b) { return !(a == b); }
template <typename T>
bool operator<(const vector<T>& a, const vector<T>& b) {
    size_t min_sz = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < min_sz; ++i) {
        if (a[i] < b[i]) return true;
        if (b[i] < a[i]) return false;
    }
    return a.size() < b.size();
}
template <typename T>
bool operator>(const vector<T>& a, const vector<T>& b) { return b < a; }
template <typename T>
bool operator<=(const vector<T>& a, const vector<T>& b) { return !(b < a); }
template <typename T>
bool operator>=(const vector<T>& a, const vector<T>& b) { return !(a < b); }
template <typename T>
void swap(vector<T>& a, vector<T>& b) noexcept { a.swap(b); }

MYSTL_NAMESPACE_END

#endif // MYSTL_VECTOR_H