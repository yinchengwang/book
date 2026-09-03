/*
 * test_iterator.cpp - iterator 全面测试
 *
 * 覆盖：iterator_tags / iterator_traits / reverse_iterator / advance / distance
 *      / next / prev / back_insert_iterator / front_insert_iterator / insert_iterator
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>

using namespace mystl;

void test_iterator() {
    // ============ iterator_tags ============
    MYSTL_TEST_BEGIN("iterator_tags");
    {
        static_assert(is_same<input_iterator_tag, input_iterator_tag>::value, "");
        static_assert(is_same<forward_iterator_tag, forward_iterator_tag>::value, "");
        static_assert(is_same<bidirectional_iterator_tag, bidirectional_iterator_tag>::value, "");
        static_assert(is_same<random_access_iterator_tag, random_access_iterator_tag>::value, "");
    }
    MYSTL_TEST_END();

    // ============ iterator_traits ============
    MYSTL_TEST_BEGIN("iterator_traits");
    {
        // 原生指针特化
        static_assert(is_same<iterator_traits<int*>::value_type, int>::value, "");
        static_assert(is_same<iterator_traits<int*>::difference_type, ptrdiff_t>::value, "");
        static_assert(is_same<iterator_traits<const int*>::value_type, int>::value, "");
        static_assert(is_same<iterator_traits<const int*>::difference_type, ptrdiff_t>::value, "");
    }
    MYSTL_TEST_END();

    // ============ advance ============
    MYSTL_TEST_BEGIN("advance");
    {
        int arr[] = {10, 20, 30, 40, 50};
        int* p = arr;
        advance(p, 2);
        EXPECT_EQ(*p, 30);
        advance(p, -1);
        EXPECT_EQ(*p, 20);
    }
    MYSTL_TEST_END();

    // ============ distance ============
    MYSTL_TEST_BEGIN("distance");
    {
        int arr[] = {10, 20, 30, 40, 50};
        ptrdiff_t d = distance(arr, arr + 5);
        EXPECT_EQ(d, 5);
        d = distance(arr + 1, arr + 4);
        EXPECT_EQ(d, 3);
    }
    MYSTL_TEST_END();

    // ============ next / prev ============
    MYSTL_TEST_BEGIN("next_prev");
    {
        int arr[] = {10, 20, 30};
        auto it = arr;
        auto n = next(it);
        EXPECT_EQ(*n, 20);
        n = next(it, 2);
        EXPECT_EQ(*n, 30);
        auto p = prev(n);
        EXPECT_EQ(*p, 20);
        p = prev(n, 2);
        EXPECT_EQ(*p, 10);
    }
    MYSTL_TEST_END();

    // ============ reverse_iterator ============
    MYSTL_TEST_BEGIN("reverse_iterator_basic");
    {
        int arr[] = {1, 2, 3, 4, 5};
        reverse_iterator<int*> ri(arr + 5);
        EXPECT_EQ(*ri, 5);
        ++ri;
        EXPECT_EQ(*ri, 4);
        --ri;
        EXPECT_EQ(*ri, 5);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("reverse_iterator_operators");
    {
        int arr[] = {1, 2, 3, 4, 5};
        reverse_iterator<int*> ri(arr + 5);
        reverse_iterator<int*> rend(arr);
        int sum = 0;
        while (ri != rend) {
            sum += *ri;
            ++ri;
        }
        EXPECT_EQ(sum, 15);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("reverse_iterator_random_access");
    {
        int arr[] = {10, 20, 30, 40, 50};
        reverse_iterator<int*> ri(arr + 5);
        EXPECT_EQ(ri[0], 50);
        EXPECT_EQ(ri[1], 40);
        EXPECT_EQ(ri[2], 30);

        auto ri2 = ri + 2;
        EXPECT_EQ(*ri2, 30);
        auto ri3 = ri2 - 1;
        EXPECT_EQ(*ri3, 40);

        EXPECT_TRUE(ri2 > ri);
        EXPECT_TRUE(ri < ri2);
        EXPECT_TRUE(ri <= ri2);
        EXPECT_TRUE(ri2 >= ri);
        EXPECT_TRUE(ri != ri2);
    }
    MYSTL_TEST_END();

    // ============ back_insert_iterator ============
    MYSTL_TEST_BEGIN("back_insert_iterator");
    {
        vector<int> v;
        v.push_back(1);
        v.push_back(2);
        auto bit = back_inserter(v);
        *bit = 3;
        ++bit;
        *bit = 4;
        EXPECT_EQ(v.size(), 4u);
        EXPECT_EQ(v[3], 4);
    }
    MYSTL_TEST_END();

    // ============ front_insert_iterator ============
    MYSTL_TEST_BEGIN("front_insert_iterator");
    {
        list<int> l;
        l.push_back(3);
        l.push_back(4);
        auto fit = front_inserter(l);
        *fit = 2;
        ++fit;
        *fit = 1;
        EXPECT_EQ(l.size(), 4u);
        EXPECT_EQ(l.front(), 1);
    }
    MYSTL_TEST_END();

    // ============ insert_iterator ============
    MYSTL_TEST_BEGIN("insert_iterator");
    {
        list<int> l;
        l.push_back(10);
        l.push_back(30);
        auto it = l.begin();
        ++it; // 指向 30
        auto ins = inserter(l, it);
        *ins = 20;
        ++ins;
        *ins = 25;
        EXPECT_EQ(l.size(), 4u);
        // 验证 10, 20, 25, 30
        auto cit = l.begin();
        EXPECT_EQ(*cit, 10); ++cit;
        EXPECT_EQ(*cit, 20); ++cit;
        EXPECT_EQ(*cit, 25); ++cit;
        EXPECT_EQ(*cit, 30);
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== iterator 全面测试 ===\n");
    test_iterator();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}