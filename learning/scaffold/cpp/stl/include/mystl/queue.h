/*
 * queue.h - 队列适配器
 *
 * 对标 std::queue<T, Container>，纯手写实现
 * 默认底层容器：deque<T>
 * 适配器模式包装，FIFO 语义
 */

#ifndef MYSTL_QUEUE_H
#define MYSTL_QUEUE_H

#include "deque.h"
#include "list.h"
#include "utility.h"

MYSTL_NAMESPACE_BEGIN

// ============================================================
// queue — FIFO 队列适配器
// ============================================================

template <typename T, typename Container = deque<T>>
class queue {
public:
    using value_type      = T;
    using container_type  = Container;
    using size_type       = typename Container::size_type;
    using reference       = typename Container::reference;
    using const_reference = typename Container::const_reference;

protected:
    Container c;

public:
    queue() = default;
    explicit queue(const Container& c_) : c(c_) {}
    explicit queue(Container&& c_) : c(mystl::move(c_)) {}

    bool empty() const { return c.empty(); }
    size_type size() const { return c.size(); }

    reference front() { return c.front(); }
    const_reference front() const { return c.front(); }

    reference back() { return c.back(); }
    const_reference back() const { return c.back(); }

    void push(const value_type& x) { c.push_back(x); }
    void push(value_type&& x) { c.push_back(mystl::move(x)); }

    template <typename... Args>
    void emplace(Args&&... args) {
        c.emplace_back(mystl::forward<Args>(args)...);
    }

    void pop() { c.pop_front(); }

    void swap(queue& other) noexcept { c.swap(other.c); }
};

// 非成员比较运算符
template <typename T, typename Container>
bool operator==(const queue<T, Container>& a, const queue<T, Container>& b) {
    if (a.size() != b.size()) return false;
    queue<T, Container> tmp_a = a;
    queue<T, Container> tmp_b = b;
    while (!tmp_a.empty()) {
        if (tmp_a.front() != tmp_b.front()) return false;
        tmp_a.pop();
        tmp_b.pop();
    }
    return true;
}

template <typename T, typename Container>
bool operator!=(const queue<T, Container>& a, const queue<T, Container>& b) {
    return !(a == b);
}

template <typename T, typename Container>
bool operator<(const queue<T, Container>& a, const queue<T, Container>& b) {
    queue<T, Container> tmp_a = a;
    queue<T, Container> tmp_b = b;
    while (!tmp_a.empty() && !tmp_b.empty()) {
        if (tmp_a.front() < tmp_b.front()) return true;
        if (tmp_b.front() < tmp_a.front()) return false;
        tmp_a.pop();
        tmp_b.pop();
    }
    return tmp_a.empty() && !tmp_b.empty();
}

template <typename T, typename Container>
bool operator>(const queue<T, Container>& a, const queue<T, Container>& b) {
    return b < a;
}

template <typename T, typename Container>
bool operator<=(const queue<T, Container>& a, const queue<T, Container>& b) {
    return !(b < a);
}

template <typename T, typename Container>
bool operator>=(const queue<T, Container>& a, const queue<T, Container>& b) {
    return !(a < b);
}

template <typename T, typename Container>
void swap(queue<T, Container>& a, queue<T, Container>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_QUEUE_H