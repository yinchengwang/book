/*
 * list_test.cpp - list 测试
 */

#include "mystl/list.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "=== List Test ===" << std::endl;

    // 基本构造测试
    mystl::list<int> lst;
    std::cout << "Empty list: size=" << lst.size() << std::endl;
    assert(lst.size() == 0);
    assert(lst.empty());

    // push_back 测试
    for (int i = 0; i < 5; ++i) {
        lst.push_back(i);
    }
    std::cout << "After push_back 5 elements: ";
    for (auto x : lst) std::cout << x << " ";
    std::cout << std::endl;

    // push_front 测试
    lst.push_front(100);
    std::cout << "After push_front(100): ";
    for (auto x : lst) std::cout << x << " ";
    std::cout << std::endl;

    // front/back 测试
    std::cout << "front=" << lst.front() << ", back=" << lst.back() << std::endl;
    assert(lst.front() == 100);
    assert(lst.back() == 4);

    // pop 测试
    lst.pop_front();
    lst.pop_back();
    std::cout << "After pop: ";
    for (auto x : lst) std::cout << x << " ";
    std::cout << std::endl;

    // 初始化列表
    mystl::list<int> lst2 = {1, 2, 3};
    std::cout << "Init list: ";
    for (auto x : lst2) std::cout << x << " ";
    std::cout << std::endl;

    // insert 测试
    lst.insert(lst.begin(), 99);
    std::cout << "After insert at begin: ";
    for (auto x : lst) std::cout << x << " ";
    std::cout << std::endl;

    // erase 测试
    lst.erase(lst.begin());
    std::cout << "After erase at begin: ";
    for (auto x : lst) std::cout << x << " ";
    std::cout << std::endl;

    // sort 测试
    mystl::list<int> lst3 = {5, 3, 1, 4, 2};
    lst3.sort();
    std::cout << "After sort: ";
    for (auto x : lst3) std::cout << x << " ";
    std::cout << std::endl;

    // reverse 测试
    lst3.reverse();
    std::cout << "After reverse: ";
    for (auto x : lst3) std::cout << x << " ";
    std::cout << std::endl;

    // unique 测试
    mystl::list<int> lst4 = {1, 1, 2, 2, 3, 3, 3};
    lst4.unique();
    std::cout << "After unique: ";
    for (auto x : lst4) std::cout << x << " ";
    std::cout << std::endl;

    std::cout << "\n=== All List Tests Passed! ===" << std::endl;
    return 0;
}
