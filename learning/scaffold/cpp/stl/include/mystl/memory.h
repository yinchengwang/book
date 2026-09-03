/*
 * memory.h - 智能指针和内存管理
 *
 * 对标 <memory> 中的智能指针部分，纯手写实现
 * 包含：default_delete / unique_ptr / shared_ptr / weak_ptr / make_* / addressof
 */

#ifndef MYSTL_MEMORY_H
#define MYSTL_MEMORY_H

#include "type_traits.h"
#include "utility.h"
#include <cstddef>   // size_t
#include <cstdint>   // uintptr_t (not strictly needed)
// nullptr_t alias (避免在外部依赖 std::)
using nullptr_t = decltype(nullptr);

MYSTL_NAMESPACE_BEGIN

// ============================================================
// 1. addressof — 取真实地址（绕过 operator& 重载）
// ============================================================

template <typename T>
T* addressof(T& arg) noexcept {
    return reinterpret_cast<T*>(&const_cast<char&>(
        reinterpret_cast<const volatile char&>(arg)));
}


// ============================================================
// 2. default_delete — 默认删除器
// ============================================================

template <typename T>
struct default_delete {
    default_delete() noexcept = default;
    template <typename U>
    default_delete(const default_delete<U>&) noexcept {}
    void operator()(T* p) const { delete p; }
};

template <typename T>
struct default_delete<T[]> {
    void operator()(T* p) const { delete[] p; }
};


// ============================================================
// 3. unique_ptr — 独占所有权智能指针
// ============================================================

template <typename T, typename Deleter = default_delete<T>>
class unique_ptr {
public:
    using element_type = T;
    using pointer = T*;
    using reference = T&;     // 添加 reference 成员类型
    using deleter_type = Deleter;

private:
    pointer ptr_;
    Deleter del_;

public:
    // 构造 / 析构
    unique_ptr() noexcept : ptr_(nullptr), del_() {}
    explicit unique_ptr(pointer p) noexcept : ptr_(p), del_() {}
    unique_ptr(pointer p, Deleter&& d) noexcept : ptr_(p), del_(mystl::move(d)) {}
    ~unique_ptr() { if (ptr_) del_(ptr_); }

    // 移动构造 / 赋值
    unique_ptr(unique_ptr&& u) noexcept : ptr_(u.ptr_), del_(mystl::move(u.del_)) {
        u.ptr_ = nullptr;
    }
    unique_ptr& operator=(unique_ptr&& u) noexcept {
        if (this != &u) {
            reset(u.release());
            del_ = mystl::move(u.del_);
        }
        return *this;
    }

    // 禁止拷贝
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    // 访问
    pointer get() const noexcept { return ptr_; }
    Deleter& get_deleter() noexcept { return del_; }
    const Deleter& get_deleter() const noexcept { return del_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    reference operator*() const { return *ptr_; }
    pointer operator->() const noexcept { return ptr_; }

    // 修改
    pointer release() noexcept {
        pointer tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
    void reset(pointer p = pointer()) noexcept {
        pointer old = ptr_;
        ptr_ = p;
        if (old) del_(old);
    }
    void swap(unique_ptr& u) noexcept {
        mystl::swap(ptr_, u.ptr_);
        mystl::swap(del_, u.del_);
    }
};

// unique_ptr<T[]> 数组特化
template <typename T, typename Deleter>
class unique_ptr<T[], Deleter> {
public:
    using element_type = T;
    using pointer = T*;
    using reference = T&;     // 添加 reference 成员类型
    using deleter_type = Deleter;

private:
    pointer ptr_;
    Deleter del_;

public:
    unique_ptr() noexcept : ptr_(nullptr), del_() {}
    explicit unique_ptr(pointer p) noexcept : ptr_(p), del_() {}
    ~unique_ptr() { if (ptr_) del_(ptr_); }

    unique_ptr(unique_ptr&& u) noexcept : ptr_(u.ptr_), del_(mystl::move(u.del_)) {
        u.ptr_ = nullptr;
    }
    unique_ptr& operator=(unique_ptr&& u) noexcept {
        if (this != &u) {
            reset(u.release());
            del_ = mystl::move(u.del_);
        }
        return *this;
    }
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    pointer get() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    reference operator[](size_t i) const { return ptr_[i]; }

    pointer release() noexcept {
        pointer tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
    void reset(pointer p = pointer()) noexcept {
        pointer old = ptr_;
        ptr_ = p;
        if (old) del_(old);
    }
    void swap(unique_ptr& u) noexcept {
        mystl::swap(ptr_, u.ptr_);
        mystl::swap(del_, u.del_);
    }
};


// ============================================================
// 4. 共享所有权智能指针（引用计数基础）
// ============================================================

// 引用计数控制块
struct _ref_count_base {
    long _shared_count = 1;
    long _weak_count = 0;

    virtual void _destroy() noexcept = 0;
    virtual ~_ref_count_base() = default;
};

template <typename T>
struct _ref_count : _ref_count_base {
    T* _ptr;
    _ref_count(T* p) : _ptr(p) {}
    void _destroy() noexcept override { delete _ptr; }
};

// ============================================================
// 5. weak_ptr — 弱引用（前置声明，因为 shared_ptr 需要友元）
// ============================================================
template <typename T> class weak_ptr;

// 6. shared_ptr — 共享所有权智能指针
template <typename T>
class shared_ptr {
public:
    using element_type = T;

private:
    T* ptr_;
    _ref_count_base* rep_;

    void _release() noexcept {
        if (rep_ && --rep_->_shared_count == 0) {
            rep_->_destroy();
            if (rep_->_weak_count == 0) delete rep_;
        }
    }

public:
    shared_ptr() noexcept : ptr_(nullptr), rep_(nullptr) {}
    explicit shared_ptr(T* p) : ptr_(p), rep_(p ? new _ref_count<T>(p) : nullptr) {}
    ~shared_ptr() { _release(); }

    shared_ptr(const shared_ptr& sp) noexcept : ptr_(sp.ptr_), rep_(sp.rep_) {
        if (rep_) ++rep_->_shared_count;
    }
    shared_ptr& operator=(const shared_ptr& sp) noexcept {
        if (this != &sp) {
            _release();
            ptr_ = sp.ptr_;
            rep_ = sp.rep_;
            if (rep_) ++rep_->_shared_count;
        }
        return *this;
    }
    shared_ptr(shared_ptr&& sp) noexcept : ptr_(sp.ptr_), rep_(sp.rep_) {
        sp.ptr_ = nullptr;
        sp.rep_ = nullptr;
    }
    shared_ptr& operator=(shared_ptr&& sp) noexcept {
        if (this != &sp) {
            _release();
            ptr_ = sp.ptr_;
            rep_ = sp.rep_;
            sp.ptr_ = nullptr;
            sp.rep_ = nullptr;
        }
        return *this;
    }

    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    long use_count() const noexcept { return rep_ ? rep_->_shared_count : 0; }
    bool unique() const noexcept { return use_count() == 1; }

    void reset() noexcept { shared_ptr().swap(*this); }
    void reset(T* p) { shared_ptr(p).swap(*this); }
    void swap(shared_ptr& sp) noexcept {
        mystl::swap(ptr_, sp.ptr_);
        mystl::swap(rep_, sp.rep_);
    }

    // 私有构造函数（weak_ptr::lock 使用）
private:
    shared_ptr(const shared_ptr& sp, T* p) noexcept : ptr_(p), rep_(sp.rep_) {
        if (rep_) ++rep_->_shared_count;
    }
    template <typename U> friend class weak_ptr;
};

template <typename T>
class weak_ptr {
public:
    using element_type = T;

private:
    T* ptr_;
    _ref_count_base* rep_;

public:
    weak_ptr() noexcept : ptr_(nullptr), rep_(nullptr) {}
    weak_ptr(const shared_ptr<T>& sp) noexcept : ptr_(sp.ptr_), rep_(sp.rep_) {
        if (rep_) ++rep_->_weak_count;
    }
    weak_ptr(const weak_ptr& wp) noexcept : ptr_(wp.ptr_), rep_(wp.rep_) {
        if (rep_) ++rep_->_weak_count;
    }
    weak_ptr(weak_ptr&& wp) noexcept : ptr_(wp.ptr_), rep_(wp.rep_) {
        wp.ptr_ = nullptr;
        wp.rep_ = nullptr;
    }
    ~weak_ptr() {
        if (rep_ && --rep_->_weak_count == 0 && rep_->_shared_count == 0) {
            delete rep_;
        }
    }

    weak_ptr& operator=(const weak_ptr& wp) noexcept {
        if (this != &wp) {
            this->~weak_ptr();
            ptr_ = wp.ptr_;
            rep_ = wp.rep_;
            if (rep_) ++rep_->_weak_count;
        }
        return *this;
    }
    weak_ptr& operator=(weak_ptr&& wp) noexcept {
        if (this != &wp) {
            this->~weak_ptr();
            ptr_ = wp.ptr_;
            rep_ = wp.rep_;
            wp.ptr_ = nullptr;
            wp.rep_ = nullptr;
        }
        return *this;
    }

    long use_count() const noexcept { return rep_ ? rep_->_shared_count : 0; }
    bool expired() const noexcept { return use_count() == 0; }

    shared_ptr<T> lock() const noexcept {
        if (expired()) return shared_ptr<T>();
        // 构造一个临时 shared_ptr 来增加引用计数
        shared_ptr<T> result;
        result.ptr_ = ptr_;
        result.rep_ = rep_;
        if (rep_) ++rep_->_shared_count;
        return result;
    }

    void swap(weak_ptr& wp) noexcept {
        mystl::swap(ptr_, wp.ptr_);
        mystl::swap(rep_, wp.rep_);
    }
};


// ============================================================
// 6. make_unique / make_shared 工厂函数
// ============================================================

template <typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(mystl::forward<Args>(args)...));
}

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    return shared_ptr<T>(new T(mystl::forward<Args>(args)...));
}

// 数组特化：make_unique<T[]>
template <typename T>
unique_ptr<T> make_unique(size_t n) {
    return unique_ptr<T>(new typename remove_extent<T>::type[n]());
}


// ============================================================
// 7. 比较运算符
// ============================================================

template <typename T, typename D>
bool operator==(const unique_ptr<T, D>& a, const unique_ptr<T, D>& b) {
    return a.get() == b.get();
}
template <typename T, typename D>
bool operator!=(const unique_ptr<T, D>& a, const unique_ptr<T, D>& b) {
    return a.get() != b.get();
}
template <typename T, typename D>
bool operator==(const unique_ptr<T, D>& a, nullptr_t) {
    return !a;
}
template <typename T, typename D>
bool operator==(nullptr_t, const unique_ptr<T, D>& a) {
    return !a;
}
template <typename T, typename D>
bool operator!=(const unique_ptr<T, D>& a, nullptr_t) {
    return (bool)a;
}
template <typename T, typename D>
bool operator!=(nullptr_t, const unique_ptr<T, D>& a) {
    return (bool)a;
}

template <typename T>
bool operator==(const shared_ptr<T>& a, const shared_ptr<T>& b) {
    return a.get() == b.get();
}
template <typename T>
bool operator!=(const shared_ptr<T>& a, const shared_ptr<T>& b) {
    return a.get() != b.get();
}
template <typename T>
bool operator==(const shared_ptr<T>& a, nullptr_t) {
    return !a;
}
template <typename T>
bool operator!=(const shared_ptr<T>& a, nullptr_t) {
    return (bool)a;
}
template <typename T>
bool operator==(nullptr_t, const shared_ptr<T>& a) {
    return !a;
}
template <typename T>
bool operator!=(nullptr_t, const shared_ptr<T>& a) {
    return (bool)a;
}

MYSTL_NAMESPACE_END

#endif // MYSTL_MEMORY_H