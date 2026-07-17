/*
 * priority_queue.h - 优先队列适配器
 *
 * 对标 std::priority_queue<T, Container, Compare>，纯手写实现
 * 默认底层容器：vector<T>，默认比较器：less<T>（大顶堆）
 * 堆操作：heapify_up / heapify_down
 */

#ifndef MYSTL_PRIORITY_QUEUE_H
#define MYSTL_PRIORITY_QUEUE_H

#include "vector.h"
#include "functional.h"
#include "utility.h"

MYSTL_NAMESPACE_BEGIN

// ============================================================
// priority_queue — 优先队列（二叉堆）
// ============================================================

template <typename T, typename Container = vector<T>, typename Compare = less<T>>
class priority_queue {
public:
    using value_type      = T;
    using container_type  = Container;
    using size_type       = typename Container::size_type;
    using reference       = typename Container::reference;
    using const_reference = typename Container::const_reference;

protected:
    Container c;
    Compare   comp;

    // 上浮
    void heapify_up(size_type i) {
        while (i > 0) {
            size_type parent = (i - 1) / 2;
            if (comp(c[parent], c[i])) {
                mystl::swap(c[i], c[parent]);
                i = parent;
            } else {
                break;
            }
        }
    }

    // 下沉
    void heapify_down(size_type i) {
        size_type n = c.size();
        while (true) {
            size_type largest = i;
            size_type left = 2 * i + 1;
            size_type right = 2 * i + 2;

            if (left < n && comp(c[largest], c[left])) largest = left;
            if (right < n && comp(c[largest], c[right])) largest = right;
            if (largest != i) {
                mystl::swap(c[i], c[largest]);
                i = largest;
            } else {
                break;
            }
        }
    }

public:
    priority_queue() : c(), comp() {}

    explicit priority_queue(const Compare& comp_) : c(), comp(comp_) {}

    priority_queue(const Compare& comp_, const Container& c_)
        : c(c_), comp(comp_) {
        // 建堆
        for (size_type i = c.size() / 2; i > 0; --i)
            heapify_down(i - 1);
    }

    priority_queue(const Compare& comp_, Container&& c_)
        : c(mystl::move(c_)), comp(comp_) {
        for (size_type i = c.size() / 2; i > 0; --i)
            heapify_down(i - 1);
    }

    template <typename InputIterator>
    priority_queue(InputIterator first, InputIterator last,
                   const Compare& comp_ = Compare(),
                   const Container& c_ = Container())
        : c(c_), comp(comp_) {
        // 逐个 push（避免 vector 没有 range insert 的问题）
        while (first != last) {
            c.push_back(*first);
            ++first;
        }
        for (size_type i = c.size() / 2; i > 0; --i)
            heapify_down(i - 1);
    }

    bool empty() const { return c.empty(); }
    size_type size() const { return c.size(); }

    const_reference top() const { return c[0]; }

    void push(const value_type& x) {
        c.push_back(x);
        heapify_up(c.size() - 1);
    }

    void push(value_type&& x) {
        c.push_back(mystl::move(x));
        heapify_up(c.size() - 1);
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        c.emplace_back(mystl::forward<Args>(args)...);
        heapify_up(c.size() - 1);
    }

    void pop() {
        mystl::swap(c[0], c[c.size() - 1]);
        c.pop_back();
        if (!c.empty()) heapify_down(0);
    }

    void swap(priority_queue& other) noexcept {
        mystl::swap(c, other.c);
        mystl::swap(comp, other.comp);
    }
};

template <typename T, typename Container, typename Compare>
void swap(priority_queue<T, Container, Compare>& a,
          priority_queue<T, Container, Compare>& b) noexcept {
    a.swap(b);
}

MYSTL_NAMESPACE_END

#endif // MYSTL_PRIORITY_QUEUE_H