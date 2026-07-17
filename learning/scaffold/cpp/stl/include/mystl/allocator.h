/*
 * allocator.h - 内存分配器
 *
 * 对标 std::allocator，纯手写实现
 * 参考：libstdc++ allocator
 */

#ifndef MYSTL_ALLOCATOR_H
#define MYSTL_ALLOCATOR_H

#include "type_traits.h"
#include "iterator.h"
#include <new>       // placement new
#include <cstddef>   // size_t, ptrdiff_t

MYSTL_NAMESPACE_BEGIN

// ============================================================
// 1. allocator — 默认内存分配器
// ============================================================

template <typename T>
class allocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    using propagate_on_container_move_assignment = true_type;
    using is_always_equal = true_type;

    // rebind — 让容器能为不同类型分配内存
    template <typename U>
    struct rebind { using other = allocator<U>; };

    allocator() noexcept {}
    allocator(const allocator&) noexcept {}
    template <typename U>
    allocator(const allocator<U>&) noexcept {}
    ~allocator() noexcept {}

    pointer address(reference x) const noexcept {
        return static_cast<pointer>(&const_cast<char&>(reinterpret_cast<const volatile char&>(x)));
    }
    const_pointer address(const_reference x) const noexcept {
        return &x;
    }

    // 分配 n 个 T 的内存（不构造）
    pointer allocate(size_type n, const void* = nullptr) {
        if (n > max_size()) throw std::bad_alloc();
        return static_cast<pointer>(::operator new(n * sizeof(T)));
    }

    // 释放内存（不析构）
    void deallocate(pointer p, size_type) noexcept {
        ::operator delete(p);
    }

    size_type max_size() const noexcept {
        return size_type(-1) / sizeof(T);
    }

    // 构造对象
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new (static_cast<void*>(p)) U(mystl::forward<Args>(args)...);
    }

    // 销毁对象
    template <typename U>
    void destroy(U* p) {
        p->~U();
    }
};

// allocator<void> 特化
template <>
class allocator<void> {
public:
    using value_type = void;
    using pointer = void*;
    using const_pointer = const void*;

    allocator() noexcept {}
    allocator(const allocator&) noexcept {}
    template <typename U>
    allocator(const allocator<U>&) noexcept {}
    ~allocator() noexcept {}
};

// 比较：所有 allocator 实例都相等
template <typename T1, typename T2>
bool operator==(const allocator<T1>&, const allocator<T2>&) noexcept { return true; }
template <typename T1, typename T2>
bool operator!=(const allocator<T1>&, const allocator<T2>&) noexcept { return false; }


// ============================================================
// 2. allocator_traits — 分配器特性萃取
// ============================================================

template <typename Alloc>
struct allocator_traits {
    using allocator_type = Alloc;
    using value_type = typename Alloc::value_type;
    using pointer = typename Alloc::pointer;
    using const_pointer = typename Alloc::const_pointer;
    using size_type = typename Alloc::size_type;
    using difference_type = typename Alloc::difference_type;

    // rebind
    template <typename U>
    using rebind_alloc = typename Alloc::template rebind<U>::other;
    template <typename U>
    struct rebind { using other = rebind_alloc<U>; };

    static pointer allocate(Alloc& a, size_type n) { return a.allocate(n); }
    static void deallocate(Alloc& a, pointer p, size_type n) { a.deallocate(p, n); }

    template <typename U, typename... Args>
    static void construct(Alloc& a, U* p, Args&&... args) {
        a.construct(p, mystl::forward<Args>(args)...);
    }

    template <typename U>
    static void destroy(Alloc& a, U* p) { a.destroy(p); }

    static size_type max_size(const Alloc& a) noexcept { return a.max_size(); }
};


// ============================================================
// 3. 未初始化内存操作
// ============================================================

// destroy — 销毁单个对象
template <typename T>
void destroy(T* p) { p->~T(); }

// uninitialized_copy — 从 [first, last) 拷贝到 dest
template <typename InputIterator, typename ForwardIterator>
ForwardIterator uninitialized_copy(InputIterator first, InputIterator last,
                                   ForwardIterator dest) {
    ForwardIterator cur = dest;
    try {
        for (; first != last; ++first, ++cur) {
            ::new (static_cast<void*>(&*cur)) typename iterator_traits<ForwardIterator>::value_type(*first);
        }
        return cur;
    } catch (...) {
        // 失败时回滚已构造的元素
        for (ForwardIterator rollback = dest; rollback != cur; ++rollback) {
            destroy(&*rollback);
        }
        throw;
    }
}

// uninitialized_fill_n — 填充 n 个元素
template <typename ForwardIterator, typename Size, typename T>
ForwardIterator uninitialized_fill_n(ForwardIterator first, Size n, const T& val) {
    ForwardIterator cur = first;
    try {
        for (; n > 0; --n, ++cur) {
            ::new (static_cast<void*>(&*cur)) typename iterator_traits<ForwardIterator>::value_type(val);
        }
        return cur;
    } catch (...) {
        for (ForwardIterator rollback = first; rollback != cur; ++rollback) {
            destroy(&*rollback);
        }
        throw;
    }
}

// uninitialized_fill — 在 [first, last) 填充 val
template <typename ForwardIterator, typename T>
void uninitialized_fill(ForwardIterator first, ForwardIterator last, const T& val) {
    ForwardIterator cur = first;
    try {
        for (; cur != last; ++cur) {
            ::new (static_cast<void*>(&*cur)) typename iterator_traits<ForwardIterator>::value_type(val);
        }
    } catch (...) {
        for (ForwardIterator rollback = first; rollback != cur; ++rollback) {
            destroy(&*rollback);
        }
        throw;
    }
}

// uninitialized_move — 从 [first, last) 移动到 dest
template <typename InputIterator, typename ForwardIterator>
ForwardIterator uninitialized_move(InputIterator first, InputIterator last,
                                   ForwardIterator dest) {
    ForwardIterator cur = dest;
    try {
        for (; first != last; ++first, ++cur) {
            ::new (static_cast<void*>(&*cur)) typename iterator_traits<ForwardIterator>::value_type(
                mystl::move(*first));
        }
        return cur;
    } catch (...) {
        for (ForwardIterator rollback = dest; rollback != cur; ++rollback) {
            destroy(&*rollback);
        }
        throw;
    }
}


// destroy_range — 销毁 [first, last) 范围内的对象
template <typename ForwardIterator>
void destroy_range(ForwardIterator first, ForwardIterator last) {
    for (; first != last; ++first) {
        destroy(&*first);
    }
}

// destroy_n — 销毁 n 个对象
template <typename ForwardIterator, typename Size>
ForwardIterator destroy_n(ForwardIterator first, Size n) {
    for (; n > 0; --n, ++first) {
        destroy(&*first);
    }
    return first;
}

MYSTL_NAMESPACE_END

#endif // MYSTL_ALLOCATOR_H