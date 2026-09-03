/*
 * test_forward_list.cpp - forward_list 容器测试
 */

#include "mystl_test.h"
#include "mystl/mystl.h"

// 辅助函数：advance forward_list iterator n 步
template <typename FL>
typename FL::iterator _fpos(FL& fl, int n) {
    auto it = fl.begin();
    for (int i = 0; i < n && it != fl.end(); ++i) ++it;
    return it;
}

void test_default_construct() {
    MYSTL_TEST_BEGIN("forward_list default construct");

    mystl::forward_list<int> fl;
    EXPECT_TRUE(fl.empty());

    MYSTL_TEST_END();
}

void test_push_pop_front() {
    MYSTL_TEST_BEGIN("forward_list push/pop front");

    mystl::forward_list<int> fl;

    fl.push_front(1);
    fl.push_front(2);
    fl.push_front(3);

    EXPECT_FALSE(fl.empty());
    EXPECT_EQ(fl.front(), 3);

    fl.pop_front();
    EXPECT_EQ(fl.front(), 2);

    fl.pop_front();
    EXPECT_EQ(fl.front(), 1);

    fl.pop_front();
    EXPECT_TRUE(fl.empty());

    // 移动版本
    int x = 100;
    fl.push_front(mystl::move(x));
    EXPECT_EQ(fl.front(), 100);

    MYSTL_TEST_END();
}

void test_front() {
    MYSTL_TEST_BEGIN("forward_list front");

    mystl::forward_list<int> fl = {10, 20, 30};

    EXPECT_EQ(fl.front(), 10);

    fl.front() = 15;
    EXPECT_EQ(fl.front(), 15);

    // const 版本
    const mystl::forward_list<int>& cfl = fl;
    EXPECT_EQ(cfl.front(), 15);

    MYSTL_TEST_END();
}

void test_empty_max_size() {
    MYSTL_TEST_BEGIN("forward_list empty/max_size");

    mystl::forward_list<int> fl;
    EXPECT_TRUE(fl.empty());
    EXPECT_TRUE(fl.max_size() > 0);

    fl.push_front(1);
    EXPECT_FALSE(fl.empty());

    MYSTL_TEST_END();
}

void test_insert_after_single() {
    MYSTL_TEST_BEGIN("forward_list insert_after single");

    mystl::forward_list<int> fl = {1, 3, 5};

    auto it = fl.insert_after(fl.before_begin(), 0);
    EXPECT_EQ(*it, 0);
    EXPECT_EQ(fl.front(), 0);

    it = fl.insert_after(fl.begin(), 2);  // 在 0 之后插入 2
    EXPECT_EQ(*it, 2);

    // 验证序列: 0, 2, 1, 3, 5
    auto it2 = fl.begin();
    EXPECT_EQ(*it2++, 0);
    EXPECT_EQ(*it2++, 2);
    EXPECT_EQ(*it2++, 1);
    EXPECT_EQ(*it2++, 3);
    EXPECT_EQ(*it2++, 5);

    MYSTL_TEST_END();
}

void test_insert_after_multiple() {
    MYSTL_TEST_BEGIN("forward_list insert_after multiple");

    mystl::forward_list<int> fl = {1, 5};

    fl.insert_after(fl.before_begin(), 3, 42);

    // 序列: 42, 42, 42, 1, 5
    auto it = fl.begin();
    EXPECT_EQ(*it++, 42);
    EXPECT_EQ(*it++, 42);
    EXPECT_EQ(*it++, 42);
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 5);

    MYSTL_TEST_END();
}

void test_insert_after_range() {
    MYSTL_TEST_BEGIN("forward_list insert_after range");

    mystl::forward_list<int> fl = {1, 5};
    int arr[] = {2, 3, 4};

    fl.insert_after(fl.begin(), arr, arr + 3);

    // 序列: 1, 2, 3, 4, 5
    auto it = fl.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(*it++, 3);
    EXPECT_EQ(*it++, 4);
    EXPECT_EQ(*it++, 5);

    MYSTL_TEST_END();
}

void test_erase_after_single() {
    MYSTL_TEST_BEGIN("forward_list erase_after single");

    mystl::forward_list<int> fl = {1, 2, 3, 4, 5};

    auto it = fl.erase_after(fl.begin());  // 删除 2
    EXPECT_EQ(*it, 3);

    // 序列: 1, 3, 4, 5
    auto it2 = fl.begin();
    EXPECT_EQ(*it2++, 1);
    EXPECT_EQ(*it2++, 3);
    EXPECT_EQ(*it2++, 4);
    EXPECT_EQ(*it2++, 5);

    MYSTL_TEST_END();
}

void test_erase_after_range() {
    MYSTL_TEST_BEGIN("forward_list erase_after range");

    mystl::forward_list<int> fl = {1, 2, 3, 4, 5, 6, 7};

    auto it = fl.erase_after(fl.begin(), _fpos(fl, 4));  // 删除 2, 3, 4
    EXPECT_EQ(*it, 5);

    // 序列: 1, 5, 6, 7
    auto it2 = fl.begin();
    EXPECT_EQ(*it2++, 1);
    EXPECT_EQ(*it2++, 5);
    EXPECT_EQ(*it2++, 6);
    EXPECT_EQ(*it2++, 7);

    MYSTL_TEST_END();
}

void test_clear() {
    MYSTL_TEST_BEGIN("forward_list clear");

    mystl::forward_list<int> fl = {1, 2, 3, 4, 5};
    fl.clear();

    EXPECT_TRUE(fl.empty());

    MYSTL_TEST_END();
}

void test_resize() {
    MYSTL_TEST_BEGIN("forward_list resize");

    mystl::forward_list<int> fl = {1, 2, 3};

    fl.resize(5, 42);
    // 序列: 1, 2, 3, 42, 42

    auto it = fl.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(*it++, 3);
    EXPECT_EQ(*it++, 42);
    EXPECT_EQ(*it++, 42);

    fl.resize(2);
    it = fl.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(it, fl.end());

    MYSTL_TEST_END();
}

void test_iterators() {
    MYSTL_TEST_BEGIN("forward_list iterators");

    mystl::forward_list<int> fl = {1, 2, 3, 4, 5};

    // before_begin
    auto bb = fl.before_begin();
    auto b = fl.begin();
    EXPECT_TRUE(bb != b);

    // 迭代
    int sum = 0;
    for (auto it = fl.begin(); it != fl.end(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    // cbegin/cend
    sum = 0;
    for (auto it = fl.cbegin(); it != fl.cend(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    // cbefore_begin
    auto cbb = fl.cbefore_begin();
    EXPECT_TRUE(cbb != fl.cbegin());

    MYSTL_TEST_END();
}

void test_splice_after_all() {
    MYSTL_TEST_BEGIN("forward_list splice_after all");

    mystl::forward_list<int> fl1 = {1, 2, 3};
    mystl::forward_list<int> fl2 = {4, 5, 6};

    fl1.splice_after(fl1.begin(), fl2);  // 在 1 之后插入 fl2 全部

    // fl1: 1, 4, 5, 6, 2, 3
    auto it = fl1.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 4);
    EXPECT_EQ(*it++, 5);
    EXPECT_EQ(*it++, 6);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(*it++, 3);

    EXPECT_TRUE(fl2.empty());

    MYSTL_TEST_END();
}

void test_splice_after_single() {
    MYSTL_TEST_BEGIN("forward_list splice_after single");

    mystl::forward_list<int> fl1 = {1, 3, 5};
    mystl::forward_list<int> fl2 = {10, 20, 30};

    fl1.splice_after(fl1.before_begin(), fl2, fl2.begin());  // 移动 20

    // fl1: 20, 1, 3, 5
    auto it = fl1.begin();
    EXPECT_EQ(*it++, 20);
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 3);
    EXPECT_EQ(*it++, 5);

    MYSTL_TEST_END();
}

void test_splice_after_range() {
    MYSTL_TEST_BEGIN("forward_list splice_after range");

    mystl::forward_list<int> fl1 = {1, 5};
    mystl::forward_list<int> fl2 = {10, 20, 30, 40, 50};

    fl1.splice_after(fl1.begin(), fl2, fl2.begin(), _fpos(fl2, 3));  // 移动 20, 30

    // fl1: 1, 20, 30, 5
    auto it = fl1.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 20);
    EXPECT_EQ(*it++, 30);
    EXPECT_EQ(*it++, 5);

    MYSTL_TEST_END();
}

void test_remove() {
    MYSTL_TEST_BEGIN("forward_list remove");

    mystl::forward_list<int> fl = {1, 2, 3, 2, 4, 2, 5};

    fl.remove(2);

    auto it = fl.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 3);
    EXPECT_EQ(*it++, 4);
    EXPECT_EQ(*it++, 5);
    EXPECT_EQ(it, fl.end());

    MYSTL_TEST_END();
}

void test_remove_if() {
    MYSTL_TEST_BEGIN("forward_list remove_if");

    mystl::forward_list<int> fl = {1, 2, 3, 4, 5, 6, 7, 8};

    fl.remove_if([](int x) { return x % 2 == 0; });

    auto it = fl.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 3);
    EXPECT_EQ(*it++, 5);
    EXPECT_EQ(*it++, 7);
    EXPECT_EQ(it, fl.end());

    MYSTL_TEST_END();
}

void test_unique() {
    MYSTL_TEST_BEGIN("forward_list unique");

    mystl::forward_list<int> fl = {1, 1, 2, 2, 2, 3, 3, 4, 5, 5};

    fl.unique();

    auto it = fl.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(*it++, 3);
    EXPECT_EQ(*it++, 4);
    EXPECT_EQ(*it++, 5);
    EXPECT_EQ(it, fl.end());

    MYSTL_TEST_END();
}

void test_merge() {
    MYSTL_TEST_BEGIN("forward_list merge");

    mystl::forward_list<int> fl1 = {1, 3, 5, 7};
    mystl::forward_list<int> fl2 = {2, 4, 6, 8};

    fl1.merge(fl2);

    auto it = fl1.begin();
    for (int i = 1; i <= 8; ++i) {
        EXPECT_EQ(*it++, i);
    }
    EXPECT_EQ(it, fl1.end());
    EXPECT_TRUE(fl2.empty());

    MYSTL_TEST_END();
}

void test_sort() {
    MYSTL_TEST_BEGIN("forward_list sort");

    mystl::forward_list<int> fl = {5, 3, 1, 4, 2};

    fl.sort();

    auto it = fl.begin();
    for (int i = 1; i <= 5; ++i) {
        EXPECT_EQ(*it++, i);
    }

    MYSTL_TEST_END();
}

void test_reverse() {
    MYSTL_TEST_BEGIN("forward_list reverse");

    mystl::forward_list<int> fl = {1, 2, 3, 4, 5};

    fl.reverse();

    auto it = fl.begin();
    EXPECT_EQ(*it++, 5);
    EXPECT_EQ(*it++, 4);
    EXPECT_EQ(*it++, 3);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(it, fl.end());

    MYSTL_TEST_END();
}

void test_swap() {
    MYSTL_TEST_BEGIN("forward_list swap");

    mystl::forward_list<int> a = {1, 2, 3};
    mystl::forward_list<int> b = {10, 20, 30, 40};

    a.swap(b);

    EXPECT_EQ(a.front(), 10);
    EXPECT_EQ(b.front(), 1);

    // 非成员 swap
    mystl::swap(a, b);
    EXPECT_EQ(a.front(), 1);
    EXPECT_EQ(b.front(), 10);

    MYSTL_TEST_END();
}

void test_comparison() {
    MYSTL_TEST_BEGIN("forward_list comparison");

    mystl::forward_list<int> a = {1, 2, 3};
    mystl::forward_list<int> b = {1, 2, 3};
    mystl::forward_list<int> c = {1, 2, 3, 4};
    mystl::forward_list<int> d = {1, 2, 4};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a == c);

    EXPECT_TRUE(a < c);
    EXPECT_TRUE(a < d);

    MYSTL_TEST_END();
}

void test_initializer_list() {
    MYSTL_TEST_BEGIN("forward_list initializer list");

    mystl::forward_list<int> fl = {1, 2, 3, 4, 5};

    EXPECT_EQ(fl.front(), 1);

    auto it = fl.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it++, 2);
    EXPECT_EQ(*it++, 3);

    MYSTL_TEST_END();
}

void test_copy_move() {
    MYSTL_TEST_BEGIN("forward_list copy/move");

    mystl::forward_list<int> fl1 = {1, 2, 3};

    // 拷贝构造
    mystl::forward_list<int> fl2(fl1);
    EXPECT_EQ(fl2.front(), 1);

    // 移动构造
    mystl::forward_list<int> fl3(mystl::move(fl1));
    EXPECT_EQ(fl3.front(), 1);
    EXPECT_TRUE(fl1.empty());

    // 拷贝赋值
    mystl::forward_list<int> fl4;
    fl4 = fl2;
    EXPECT_EQ(fl4.front(), 1);

    // 移动赋值
    mystl::forward_list<int> fl5;
    fl5 = mystl::move(fl3);
    EXPECT_EQ(fl5.front(), 1);
    EXPECT_TRUE(fl3.empty());

    MYSTL_TEST_END();
}

void test_assign() {
    MYSTL_TEST_BEGIN("forward_list assign");

    mystl::forward_list<int> fl;

    fl.assign(5, 42);

    auto it = fl.begin();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(*it++, 42);
    }

    int arr[] = {1, 2, 3, 4};
    fl.assign(arr, arr + 4);
    EXPECT_EQ(fl.front(), 1);

    MYSTL_TEST_END();
}

void test_no_size_method() {
    MYSTL_TEST_BEGIN("forward_list no size method");

    mystl::forward_list<int> fl = {1, 2, 3, 4, 5};

    // forward_list 没有 size() 方法
    // 只能手动计数
    int count = 0;
    for (auto it = fl.begin(); it != fl.end(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 5);

    MYSTL_TEST_END();
}

void test_empty_single() {
    MYSTL_TEST_BEGIN("forward_list empty/single");

    // 空链表
    mystl::forward_list<int> empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.begin(), empty.end());

    // 单元素
    mystl::forward_list<int> single = {42};
    EXPECT_EQ(single.front(), 42);

    MYSTL_TEST_END();
}

int main() {
    test_default_construct();
    test_push_pop_front();
    test_front();
    test_empty_max_size();
    test_insert_after_single();
    test_insert_after_multiple();
    test_insert_after_range();
    test_erase_after_single();
    test_erase_after_range();
    test_clear();
    test_resize();
    test_iterators();
    test_splice_after_all();
    test_splice_after_single();
    test_splice_after_range();
    test_remove();
    test_remove_if();
    test_unique();
    test_merge();
    test_sort();
    test_reverse();
    test_swap();
    test_comparison();
    test_initializer_list();
    test_copy_move();
    test_assign();
    test_no_size_method();
    test_empty_single();

    MYSTL_TEST_SUMMARY();

    return mystl_test::stats().failed > 0 ? 1 : 0;
}
