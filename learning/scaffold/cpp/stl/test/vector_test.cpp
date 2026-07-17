/*
 * vector_test.cpp - vector 测试
 */

#include "mystl/vector.h"
#include <iostream>
#include <vector>
#include <cassert>

int main() {
    std::cout << "=== Vector Test ===" << std::endl;

    // 基本构造测试
    mystl::vector<int> mv;
    std::vector<int> sv;

    std::cout << "Empty vector: size=" << mv.size() << ", cap=" << mv.capacity() << std::endl;
    assert(mv.size() == 0);
    assert(mv.empty());

    // push_back 测试
    for (int i = 0; i < 10; ++i) {
        mv.push_back(i);
        sv.push_back(i);
    }
    std::cout << "After push_back 10 elements: size=" << mv.size() << std::endl;
    assert(mv.size() == sv.size());

    // 迭代器测试
    std::cout << "Elements: ";
    for (auto x : mv) {
        std::cout << x << " ";
    }
    std::cout << std::endl;

    // operator[] 测试
    for (size_t i = 0; i < mv.size(); ++i) {
        assert(mv[i] == sv[i]);
    }
    std::cout << "operator[] test passed" << std::endl;

    // front/back 测试
    assert(mv.front() == 0);
    assert(mv.back() == 9);
    std::cout << "front=" << mv.front() << ", back=" << mv.back() << std::endl;

    // pop_back 测试
    mv.pop_back();
    sv.pop_back();
    assert(mv.size() == sv.size());
    std::cout << "After pop_back: size=" << mv.size() << std::endl;

    // insert 测试
    auto it = mv.cbegin() + 5;
    mv.insert(it, 100);
    std::cout << "After insert(100) at position 5: ";
    for (auto x : mv) std::cout << x << " ";
    std::cout << std::endl;

    // erase 测试
    mv.erase(mv.cbegin() + 5);
    std::cout << "After erase at position 5: ";
    for (auto x : mv) std::cout << x << " ";
    std::cout << std::endl;

    // 初始化列表构造
    mystl::vector<int> mv2 = {1, 2, 3, 4, 5};
    std::cout << "Init list: ";
    for (auto x : mv2) std::cout << x << " ";
    std::cout << std::endl;

    // 拷贝构造
    mystl::vector<int> mv3 = mv;
    assert(mv3.size() == mv.size());
    std::cout << "Copy constructor test passed" << std::endl;

    // reserve 测试
    mv.reserve(100);
    std::cout << "After reserve(100): cap=" << mv.capacity() << std::endl;
    assert(mv.capacity() >= 100);

    // clear 测试
    mv.clear();
    assert(mv.empty());
    std::cout << "After clear: size=" << mv.size() << std::endl;

    std::cout << "\n=== All Vector Tests Passed! ===" << std::endl;
    return 0;
}
