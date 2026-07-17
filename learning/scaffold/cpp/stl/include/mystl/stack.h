/*
 * stack.h - 栈适配器
 *
 * 对标 std::stack<T, Container>，纯手写实现
 * 默认底层容器：deque<T>
 * 适配器模式包装，所有操作委托给底层容器
 */

#ifndef MYSTL_STACK_H
#define MYSTL_STACK_H

#include "deque.h"
#include "vector.h"
#include "utility.h"

MYSTL_NAMESPACE_BEGIN

// ============================================================
// stack — LIFO 栈适配器
// ============================================================

template <typename T, typename Container = deque<T>>
class stack {
public:
    using value_type      = T;
    using container_type  = Container;
    using size_type       = typename Container::size_type;
    using reference       = typename Container::reference;
    using const_reference = typename Container::const_reference;

protected:
    Container c;

public:
    stack() = default;
    explicit stack(const Container& c_) : c(c_) {}
    explicit stack(Container&& c_) : c(mystl::move(c_)) {}

    bool empty() const { return c.empty(); }
    size_type size() const { return c.size(); }

    reference top() { return c.back(); }
    const_reference top() const { return c.back(); }

    void push(const value_type& x) { c.push_back(x); }
    void push(value_type&& x) { c.push_back(mystl::move(x)); }

    template <typename... Args>
    void emplace(Args&&... args) {
        c.emplace_back(mystl::forward<Args>(args)...);
    }

    void pop() { c.pop_back(); }

    void swap(stack& other) noexcept { c.swap(other.c); }
};

// 非成员比较运算符
template <typename T, typename Container>
bool operator==(const stack<T, Container>& a, const stack<T, Container>& b) {
    // 注意：标准要求比较底层容器
    // 但 stack 的 c 是 protected，这里通过友元或直接使用容器的比较
    // 简单做法：通过 size + 逐元素比较
    if (a.size() != b.size()) return false;
    stack<T, Container> tmp_a = a;
    stack<T, Container> tmp_b = b;
    while (!tmp_a.empty()) {
        if (tmp_a.top() != tmp_b.top()) return false;
        tmp_a.pop();
        tmp_b.pop();
    }
    return true;
}

template <typename T, typename Container>
bool operator!=(const stack<T, Container>& a, const stack<T, Container>& b) {
    return !(a == b);
}

template <typename T, typename Container>
bool operator<(const stack<T, Container>& a, const stack<T, Container>& b) {
    stack<T, Container> tmp_a = a;
    stack<T, Container> tmp_b = b;
    // 从底到顶比较
    // 简单实现：递归比较
    if (tmp_a.empty()) return !tmp_b.empty();
    if (tmp_b.empty()) return false;
    T top_a = tmp_a.top(); tmp_a.pop();
    T top_b = tmp_b.top(); tmp_b.pop();
    if (top_a < top_b) return true;
    if (top_b < top_a) return false;
    return tmp_a < tmp_b;
}

template <typename T, typename Container>
bool operator>(const stack<T, Container>& a, const stack<T, Container>& b) {
    return b < a;
}

template <typename T, typename Container>
bool operator<=(const stack<T, Container>& a, const stack<T, Container>& b) {
    return !(b < a);
}

template <typename T, typename Container>
bool operator>=(const stack<T, Container>& a, const stack<T, Container>& b) {
    return !(a < b);
}

template <typename T, typename Container>
void swap(stack<T, Container>& a, stack<T, Container>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_STACK_H