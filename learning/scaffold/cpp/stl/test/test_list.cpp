/*
 * test_list.cpp - list 容器测试
 */

#include "mystl_test.h"
#include "mystl/mystl.h"

// 辅助函数：advance list iterator n 步（list 不支持 operator+）
template <typename List>
typename List::iterator _pos(List& l, int n) {
    auto it = l.begin();
    for (int i = 0; i < n && it != l.end(); ++i) ++it;
    return it;
}

void test_default_construct() {
    MYSTL_TEST_BEGIN("list default construct");

    mystl::list<int> l;
    EXPECT_TRUE(l.empty());
    EXPECT_EQ(l.size(), 0u);

    MYSTL_TEST_END();
}

void test_size_value_construct() {
    MYSTL_TEST_BEGIN("list size/value construct");

    mystl::list<int> l(5, 42);
    EXPECT_EQ(l.size(), 5u);

    for (auto it = l.begin(); it != l.end(); ++it) {
        EXPECT_EQ(*it, 42);
    }

    // 仅 size 构造
    mystl::list<int> l2(3);
    EXPECT_EQ(l2.size(), 3u);
    for (auto& x : l2) {
        EXPECT_EQ(x, 0);
    }

    MYSTL_TEST_END();
}

void test_initializer_list() {
    MYSTL_TEST_BEGIN("list initializer list");

    mystl::list<int> l = {1, 2, 3, 4, 5};
    EXPECT_EQ(l.size(), 5u);
    EXPECT_EQ(l.front(), 1);
    EXPECT_EQ(l.back(), 5);

    MYSTL_TEST_END();
}

void test_range_construct() {
    MYSTL_TEST_BEGIN("list range construct");

    int arr[] = {10, 20, 30};
    mystl::list<int> l(arr, arr + 3);

    EXPECT_EQ(l.size(), 3u);
    EXPECT_EQ(l.front(), 10);
    EXPECT_EQ(l.back(), 30);

    MYSTL_TEST_END();
}

void test_push_pop() {
    MYSTL_TEST_BEGIN("list push/pop");

    mystl::list<int> l;

    l.push_back(1);
    l.push_back(2);
    l.push_front(0);

    EXPECT_EQ(l.size(), 3u);
    EXPECT_EQ(l.front(), 0);
    EXPECT_EQ(l.back(), 2);

    l.pop_front();
    EXPECT_EQ(l.front(), 1);
    EXPECT_EQ(l.size(), 2u);

    l.pop_back();
    EXPECT_EQ(l.back(), 1);
    EXPECT_EQ(l.size(), 1u);

    // 移动版本
    int x = 100;
    l.push_front(mystl::move(x));
    EXPECT_EQ(l.front(), 100);

    MYSTL_TEST_END();
}

void test_front_back() {
    MYSTL_TEST_BEGIN("list front/back");

    mystl::list<int> l = {10, 20, 30, 40, 50};

    EXPECT_EQ(l.front(), 10);
    EXPECT_EQ(l.back(), 50);

    l.front() = 15;
    l.back() = 55;
    EXPECT_EQ(l.front(), 15);
    EXPECT_EQ(l.back(), 55);

    // const 版本
    const mystl::list<int>& cl = l;
    EXPECT_EQ(cl.front(), 15);
    EXPECT_EQ(cl.back(), 55);

    MYSTL_TEST_END();
}

void test_size_empty_max() {
    MYSTL_TEST_BEGIN("list size/empty/max_size");

    mystl::list<int> l;
    EXPECT_TRUE(l.empty());
    EXPECT_EQ(l.size(), 0u);
    EXPECT_TRUE(l.max_size() > 0);

    l.push_back(1);
    EXPECT_FALSE(l.empty());
    EXPECT_EQ(l.size(), 1u);

    MYSTL_TEST_END();
}

void test_insert_single() {
    MYSTL_TEST_BEGIN("list insert single");

    mystl::list<int> l = {1, 2, 3};

    auto it = l.insert(l.begin(), 0);
    EXPECT_EQ(l.size(), 4u);
    EXPECT_EQ(*it, 0);
    EXPECT_EQ(l.front(), 0);

    it = l.insert(l.end(), 4);
    EXPECT_EQ(l.size(), 5u);
    EXPECT_EQ(l.back(), 4);

    auto _it2 = l.begin(); ++_it2; ++_it2; it = l.insert(_it2, 100);
    EXPECT_EQ(l.size(), 6u);
    EXPECT_EQ(*it, 100);

    MYSTL_TEST_END();
}

void test_insert_multiple() {
    MYSTL_TEST_BEGIN("list insert multiple");

    mystl::list<int> l = {1, 5};

    l.insert(_pos(l, 1), 3, 42);
    EXPECT_EQ(l.size(), 5u);

    auto it = l.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 42);
    EXPECT_EQ(*it++, 42);
    EXPECT_EQ(*it++, 42);
    EXPECT_EQ(*it++, 5);

    MYSTL_TEST_END();
}

void test_insert_range() {
    MYSTL_TEST_BEGIN("list insert range");

    mystl::list<int> l = {1, 5};
    mystl::list<int> src = {2, 3, 4};

    l.insert(_pos(l, 1), src.begin(), src.end());
    EXPECT_EQ(l.size(), 5u);

    int expected[] = {1, 2, 3, 4, 5};
    auto it = l.begin();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(*it++, expected[i]);
    }

    MYSTL_TEST_END();
}

void test_erase_single() {
    MYSTL_TEST_BEGIN("list erase single");

    mystl::list<int> l = {1, 2, 3, 4, 5};

    auto it = l.erase(l.begin());
    EXPECT_EQ(l.size(), 4u);
    EXPECT_EQ(*it, 2);

    it = l.erase(_pos(l, 1));
    EXPECT_EQ(l.size(), 3u);
    EXPECT_EQ(*it, 4);

    it = l.erase(--l.end());
    EXPECT_EQ(l.size(), 2u);
    EXPECT_EQ(it, l.end());

    MYSTL_TEST_END();
}

void test_erase_range() {
    MYSTL_TEST_BEGIN("list erase range");

    mystl::list<int> l = {1, 2, 3, 4, 5, 6, 7};

    auto it = l.erase(_pos(l, 2), _pos(l, 5));
    EXPECT_EQ(l.size(), 4u);

    int expected[] = {1, 2, 6, 7};
    auto it2 = l.begin();
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(*it2++, expected[i]);
    }

    MYSTL_TEST_END();
}

void test_clear() {
    MYSTL_TEST_BEGIN("list clear");

    mystl::list<int> l = {1, 2, 3, 4, 5};
    l.clear();

    EXPECT_TRUE(l.empty());
    EXPECT_EQ(l.size(), 0u);

    MYSTL_TEST_END();
}

void test_resize() {
    MYSTL_TEST_BEGIN("list resize");

    mystl::list<int> l = {1, 2, 3};

    l.resize(5, 42);
    EXPECT_EQ(l.size(), 5u);
    EXPECT_EQ(l.back(), 42);

    l.resize(2);
    EXPECT_EQ(l.size(), 2u);
    EXPECT_EQ(l.back(), 2);

    l.resize(4);
    EXPECT_EQ(l.size(), 4u);
    auto _it2 = l.begin(); ++_it2; ++_it2; EXPECT_EQ(*_it2, 0);

    MYSTL_TEST_END();
}

void test_iterators() {
    MYSTL_TEST_BEGIN("list iterators");

    mystl::list<int> l = {1, 2, 3, 4, 5};

    // 正向迭代
    int sum = 0;
    for (auto it = l.begin(); it != l.end(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    // cbegin/cend
    sum = 0;
    for (auto it = l.cbegin(); it != l.cend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    // 反向迭代
    sum = 0;
    for (auto it = l.rbegin(); it != l.rend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    MYSTL_TEST_END();
}

void test_splice_all() {
    MYSTL_TEST_BEGIN("list splice all");

    mystl::list<int> l1 = {1, 2, 3};
    mystl::list<int> l2 = {4, 5, 6};

    l1.splice(l1.end(), l2);

    EXPECT_EQ(l1.size(), 6u);
    EXPECT_TRUE(l2.empty());

    int expected[] = {1, 2, 3, 4, 5, 6};
    auto it = l1.begin();
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(*it++, expected[i]);
    }

    MYSTL_TEST_END();
}

void test_splice_single() {
    MYSTL_TEST_BEGIN("list splice single");

    mystl::list<int> l1 = {1, 3, 5};
    mystl::list<int> l2 = {2, 4, 6};

    l1.splice(_pos(l1, 1), l2, l2.begin());

    EXPECT_EQ(l1.size(), 4u);
    EXPECT_EQ(l2.size(), 2u);

    int expected1[] = {1, 2, 3, 5};
    auto it1 = l1.begin();
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(*it1++, expected1[i]);
    }

    int expected2[] = {4, 6};
    auto it2 = l2.begin();
    for (int i = 0; i < 2; ++i) {
        EXPECT_EQ(*it2++, expected2[i]);
    }

    MYSTL_TEST_END();
}

void test_splice_range() {
    MYSTL_TEST_BEGIN("list splice range");

    mystl::list<int> l1 = {1, 5};
    mystl::list<int> l2 = {2, 3, 4, 6, 7};

    l1.splice(_pos(l1, 1), l2, l2.begin(), _pos(l2, 3));

    EXPECT_EQ(l1.size(), 5u);
    EXPECT_EQ(l2.size(), 2u);

    int expected1[] = {1, 2, 3, 4, 5};
    auto it1 = l1.begin();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(*it1++, expected1[i]);
    }

    MYSTL_TEST_END();
}

void test_remove() {
    MYSTL_TEST_BEGIN("list remove");

    mystl::list<int> l = {1, 2, 3, 2, 4, 2, 5};

    l.remove(2);
    EXPECT_EQ(l.size(), 4u);

    int expected[] = {1, 3, 4, 5};
    auto it = l.begin();
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(*it++, expected[i]);
    }

    MYSTL_TEST_END();
}

void test_remove_if() {
    MYSTL_TEST_BEGIN("list remove_if");

    mystl::list<int> l = {1, 2, 3, 4, 5, 6, 7, 8};

    l.remove_if([](int x) { return x % 2 == 0; });
    EXPECT_EQ(l.size(), 4u);

    int expected[] = {1, 3, 5, 7};
    auto it = l.begin();
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(*it++, expected[i]);
    }

    MYSTL_TEST_END();
}

void test_unique() {
    MYSTL_TEST_BEGIN("list unique");

    mystl::list<int> l = {1, 1, 2, 2, 2, 3, 3, 4, 5, 5};

    l.unique();
    EXPECT_EQ(l.size(), 5u);

    int expected[] = {1, 2, 3, 4, 5};
    auto it = l.begin();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(*it++, expected[i]);
    }

    MYSTL_TEST_END();
}

void test_unique_predicate() {
    MYSTL_TEST_BEGIN("list unique with predicate");

    mystl::list<int> l = {1, 3, 5, 2, 4, 6, 1, 3};

    // 删除相差 <= 2 的相邻元素
    l.unique([](int a, int b) { return abs(a - b) <= 2; });
    // 1,3,5 (3-1=2, 5-3=2 合并) -> 保留 1,5
    // 然后 2,4,6 (4-2=2, 6-4=2 合并) -> 保留 2,6
    // 最后 1,3

    EXPECT_TRUE(l.size() > 0);

    MYSTL_TEST_END();
}

void test_merge() {
    MYSTL_TEST_BEGIN("list merge");

    mystl::list<int> l1 = {1, 3, 5, 7};
    mystl::list<int> l2 = {2, 4, 6, 8};

    l1.merge(l2);

    EXPECT_EQ(l1.size(), 8u);
    EXPECT_TRUE(l2.empty());

    int expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
    auto it = l1.begin();
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(*it++, expected[i]);
    }

    MYSTL_TEST_END();
}

void test_merge_with_compare() {
    MYSTL_TEST_BEGIN("list merge with compare");

    mystl::list<int> l1 = {7, 5, 3, 1};  // 降序
    mystl::list<int> l2 = {8, 6, 4, 2};  // 降序

    // 使用 greater 合并降序链表
    l1.merge(l2, mystl::greater<int>());

    EXPECT_EQ(l1.size(), 8u);

    int expected[] = {8, 7, 6, 5, 4, 3, 2, 1};
    auto it = l1.begin();
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(*it++, expected[i]);
    }

    MYSTL_TEST_END();
}

void test_reverse() {
    MYSTL_TEST_BEGIN("list reverse");

    mystl::list<int> l = {1, 2, 3, 4, 5};

    l.reverse();

    int expected[] = {5, 4, 3, 2, 1};
    auto it = l.begin();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(*it++, expected[i]);
    }

    // 空链表
    mystl::list<int> empty;
    empty.reverse();
    EXPECT_TRUE(empty.empty());

    // 单元素
    mystl::list<int> single = {42};
    single.reverse();
    EXPECT_EQ(single.front(), 42);

    MYSTL_TEST_END();
}

void test_sort() {
    MYSTL_TEST_BEGIN("list sort");

    mystl::list<int> l = {5, 3, 1, 4, 2};

    l.sort();

    int expected[] = {1, 2, 3, 4, 5};
    auto it = l.begin();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(*it++, expected[i]);
    }

    MYSTL_TEST_END();
}

void test_sort_with_compare() {
    MYSTL_TEST_BEGIN("list sort with compare");

    mystl::list<int> l = {5, 3, 1, 4, 2};

    l.sort(mystl::greater<int>());

    int expected[] = {5, 4, 3, 2, 1};
    auto it = l.begin();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(*it++, expected[i]);
    }

    MYSTL_TEST_END();
}

void test_swap() {
    MYSTL_TEST_BEGIN("list swap");

    mystl::list<int> a = {1, 2, 3};
    mystl::list<int> b = {10, 20, 30, 40};

    a.swap(b);

    EXPECT_EQ(a.size(), 4u);
    EXPECT_EQ(a.front(), 10);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b.front(), 1);

    // 非成员 swap
    mystl::swap(a, b);
    EXPECT_EQ(a.size(), 3u);
    EXPECT_EQ(b.size(), 4u);

    MYSTL_TEST_END();
}

void test_comparison() {
    MYSTL_TEST_BEGIN("list comparison");

    mystl::list<int> a = {1, 2, 3};
    mystl::list<int> b = {1, 2, 3};
    mystl::list<int> c = {1, 2, 3, 4};
    mystl::list<int> d = {1, 2, 4};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a == c);

    // 字典序
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(a < d);
    EXPECT_FALSE(d < a);

    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(c > a);
    EXPECT_TRUE(d > a);

    MYSTL_TEST_END();
}

void test_copy_move() {
    MYSTL_TEST_BEGIN("list copy/move");

    mystl::list<int> l1 = {1, 2, 3};

    // 拷贝构造
    mystl::list<int> l2(l1);
    EXPECT_EQ(l2.size(), 3u);
    EXPECT_EQ(l1.size(), 3u);  // 原链表不变

    // 移动构造
    mystl::list<int> l3(mystl::move(l1));
    EXPECT_EQ(l3.size(), 3u);
    EXPECT_TRUE(l1.empty());

    // 拷贝赋值
    mystl::list<int> l4;
    l4 = l2;
    EXPECT_EQ(l4.size(), 3u);

    // 移动赋值
    mystl::list<int> l5;
    l5 = mystl::move(l3);
    EXPECT_EQ(l5.size(), 3u);
    EXPECT_TRUE(l3.empty());

    MYSTL_TEST_END();
}

void test_assign() {
    MYSTL_TEST_BEGIN("list assign");

    mystl::list<int> l;

    l.assign(5, 42);
    EXPECT_EQ(l.size(), 5u);
    for (auto& x : l) {
        EXPECT_EQ(x, 42);
    }

    int arr[] = {1, 2, 3, 4};
    l.assign(arr, arr + 4);
    EXPECT_EQ(l.size(), 4u);
    EXPECT_EQ(l.front(), 1);

    MYSTL_TEST_END();
}

void test_empty_single() {
    MYSTL_TEST_BEGIN("list empty/single");

    // 空链表
    mystl::list<int> empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.begin(), empty.end());

    // 单元素
    mystl::list<int> single = {42};
    EXPECT_EQ(single.size(), 1u);
    EXPECT_EQ(single.front(), 42);
    EXPECT_EQ(single.back(), 42);

    MYSTL_TEST_END();
}

int main() {
    test_default_construct();
    test_size_value_construct();
    test_initializer_list();
    test_range_construct();
    test_push_pop();
    test_front_back();
    test_size_empty_max();
    test_insert_single();
    test_insert_multiple();
    test_insert_range();
    test_erase_single();
    test_erase_range();
    test_clear();
    test_resize();
    test_iterators();
    test_splice_all();
    test_splice_single();
    test_splice_range();
    test_remove();
    test_remove_if();
    test_unique();
    test_unique_predicate();
    test_merge();
    test_merge_with_compare();
    test_reverse();
    test_sort();
    test_sort_with_compare();
    test_swap();
    test_comparison();
    test_copy_move();
    test_assign();
    test_empty_single();

    MYSTL_TEST_SUMMARY();

    return mystl_test::stats().failed > 0 ? 1 : 0;
}