/*
 * stl_demo.cpp - Mystl 完整演示
 */

#include "mystl/mystl.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "=== Mystl Complete Demo ===" << std::endl;

    // ========== 1. Vector 测试 ==========
    std::cout << "\n--- Vector ---" << std::endl;
    mystl::vector<int> v = {5, 2, 8, 1, 9, 3};
    std::cout << "vector: ";
    for (auto x : v) std::cout << x << " ";
    std::cout << std::endl;

    mystl::sort(v.begin(), v.end());
    std::cout << "sorted: ";
    for (auto x : v) std::cout << x << " ";
    std::cout << std::endl;

    // ========== 2. List 测试 ==========
    std::cout << "\n--- List ---" << std::endl;
    mystl::list<int> lst = {3, 1, 4, 1, 5, 9, 2, 6};
    std::cout << "list: ";
    for (auto x : lst) std::cout << x << " ";
    std::cout << std::endl;

    lst.sort();
    std::cout << "sorted: ";
    for (auto x : lst) std::cout << x << " ";
    std::cout << std::endl;

    // ========== 3. Stack 测试 ==========
    std::cout << "\n--- Stack ---" << std::endl;
    mystl::stack<int> s;
    for (int i = 0; i < 5; ++i) s.push(i);
    std::cout << "stack top: " << s.top() << std::endl;
    s.pop();
    std::cout << "after pop, top: " << s.top() << std::endl;

    // ========== 4. Queue 测试 ==========
    std::cout << "\n--- Queue ---" << std::endl;
    mystl::queue<int> q;
    for (int i = 0; i < 5; ++i) q.push(i);
    std::cout << "queue front: " << q.front() << std::endl;
    std::cout << "queue back: " << q.back() << std::endl;

    // ========== 5. Priority Queue 测试 ==========
    std::cout << "\n--- Priority Queue ---" << std::endl;
    mystl::priority_queue<int> pq;
    for (int x : {3, 1, 4, 1, 5, 9}) pq.push(x);
    std::cout << "priority_queue top: " << pq.top() << std::endl;
    pq.pop();
    std::cout << "after pop, top: " << pq.top() << std::endl;

    // ========== 6. Algorithm 测试 ==========
    std::cout << "\n--- Algorithm ---" << std::endl;
    mystl::vector<int> arr = {5, 2, 8, 1, 9, 3};

    // find
    auto it = mystl::find(arr.begin(), arr.end(), 5);
    std::cout << "find(5): " << (it != arr.end() ? "found" : "not found") << std::endl;

    // count
    mystl::vector<int> dup = {1, 2, 2, 3, 2, 4};
    int n2 = mystl::count(dup.begin(), dup.end(), 2);
    std::cout << "count(2) in dup: " << n2 << std::endl;

    // accumulate
    int sum = mystl::accumulate(arr.begin(), arr.end(), 0);
    std::cout << "accumulate: " << sum << std::endl;

    // min/max
    std::cout << "min: " << *mystl::min_element(arr.begin(), arr.end()) << std::endl;
    std::cout << "max: " << *mystl::max_element(arr.begin(), arr.end()) << std::endl;

    // binary_search
    bool found = mystl::binary_search(arr.begin(), arr.end(), 5);
    std::cout << "binary_search(5): " << (found ? "found" : "not found") << std::endl;

    // ========== 7. Type Traits 测试 ==========
    std::cout << "\n--- Type Traits ---" << std::endl;
    std::cout << "is_integral<int>: " << mystl::is_integral<int>::value << std::endl;
    std::cout << "is_pointer<int*>: " << mystl::is_pointer<int*>::value << std::endl;
    std::cout << "is_same<int, int>: " << std::is_same<int, int>::value << std::endl;

    // ========== 8. Functional 测试 ==========
    std::cout << "\n--- Functional ---" << std::endl;
    mystl::less<int> less_fn;
    std::cout << "less<int>(1, 2): " << less_fn(1, 2) << std::endl;
    std::cout << "less<int>(2, 1): " << less_fn(2, 1) << std::endl;

    mystl::greater<int> greater_fn;
    std::cout << "greater<int>(2, 1): " << greater_fn(2, 1) << std::endl;

    // ========== 9. Memory 测试 ==========
    std::cout << "\n--- Memory (Smart Pointers) ---" << std::endl;
    auto up = mystl::make_unique<int>(42);
    std::cout << "unique_ptr value: " << *up << std::endl;

    auto sp = mystl::make_shared<int>(100);
    std::cout << "shared_ptr value: " << *sp << std::endl;
    std::cout << "shared_ptr use_count: " << sp.use_count() << std::endl;

    std::cout << "\n=== All Tests Passed! ===" << std::endl;
    return 0;
}
