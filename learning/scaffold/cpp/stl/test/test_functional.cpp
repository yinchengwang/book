/*
 * test_functional.cpp - functional 全面测试
 *
 * 覆盖：less / greater / equal_to / hash / reference_wrapper / ref / cref
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>

using namespace mystl;

void test_functional() {
    // ============ 关系比较仿函数 ============
    MYSTL_TEST_BEGIN("less");
    {
        less<int> cmp;
        EXPECT_TRUE(cmp(1, 2));
        EXPECT_FALSE(cmp(2, 1));
        EXPECT_FALSE(cmp(1, 1));
        // void 透明特化
        less<void> cmpv;
        EXPECT_TRUE(cmpv(1, 2.0));
        EXPECT_FALSE(cmpv(3, 2));
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("greater");
    {
        greater<int> cmp;
        EXPECT_TRUE(cmp(2, 1));
        EXPECT_FALSE(cmp(1, 2));
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("equal_to");
    {
        equal_to<int> cmp;
        EXPECT_TRUE(cmp(1, 1));
        EXPECT_FALSE(cmp(1, 2));
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("not_equal_to");
    {
        not_equal_to<int> cmp;
        EXPECT_TRUE(cmp(1, 2));
        EXPECT_FALSE(cmp(1, 1));
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("less_equal");
    {
        less_equal<int> cmp;
        EXPECT_TRUE(cmp(1, 2));
        EXPECT_TRUE(cmp(1, 1));
        EXPECT_FALSE(cmp(2, 1));
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("greater_equal");
    {
        greater_equal<int> cmp;
        EXPECT_TRUE(cmp(2, 1));
        EXPECT_TRUE(cmp(1, 1));
        EXPECT_FALSE(cmp(1, 2));
    }
    MYSTL_TEST_END();

    // ============ hash ============
    MYSTL_TEST_BEGIN("hash_int");
    {
        hash<int> h;
        EXPECT_EQ(h(42), h(42));
        // 不同值大概率不同哈希
        EXPECT_NE(h(1), h(2));
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("hash_float");
    {
        hash<double> h;
        EXPECT_EQ(h(3.14), h(3.14));
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("hash_pointer");
    {
        int x = 0;
        hash<int*> hp;
        hash<const int*> hcp;
        EXPECT_EQ(hp(&x), hp(&x));
        EXPECT_EQ(hcp(&x), hcp(&x));
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("hash_string");
    {
        hash<const char*> h;
        // 相同字符串产生相同哈希
        EXPECT_EQ(h("hello"), h("hello"));
    }
    MYSTL_TEST_END();

    // ============ reference_wrapper ============
    MYSTL_TEST_BEGIN("reference_wrapper");
    {
        int x = 10;
        reference_wrapper<int> r(x);
        EXPECT_EQ(r.get(), 10);
        EXPECT_EQ((int&)r, 10);
        r.get() = 20;
        EXPECT_EQ(x, 20);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("reference_wrapper_function");
    {
        auto add = [](int a, int b) { return a + b; };
        reference_wrapper<decltype(add)> r(add);
        EXPECT_EQ(r(1, 2), 3);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("ref_cref");
    {
        int x = 42;
        auto r = ref(x);
        EXPECT_TRUE((is_same<reference_wrapper<int>, decltype(r)>::value));
        EXPECT_EQ(r.get(), 42);

        const int cx = 99;
        auto cr = cref(cx);
        EXPECT_TRUE((is_same<reference_wrapper<const int>, decltype(cr)>::value));
        EXPECT_EQ(cr.get(), 99);
    }
    MYSTL_TEST_END();

    // ============ 其它仿函数 ============
    MYSTL_TEST_BEGIN("arithmetic_functors");
    {
        plus<int> p;
        EXPECT_EQ(p(3, 4), 7);
        minus<int> m;
        EXPECT_EQ(m(10, 3), 7);
        multiplies<int> mul;
        EXPECT_EQ(mul(3, 4), 12);
        divides<int> d;
        EXPECT_EQ(d(12, 4), 3);
        modulus<int> mod;
        EXPECT_EQ(mod(10, 3), 1);
        negate<int> n;
        EXPECT_EQ(n(5), -5);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("logical_functors");
    {
        logical_and<bool> land;
        EXPECT_TRUE(land(true, true));
        EXPECT_FALSE(land(true, false));
        logical_or<bool> lor;
        EXPECT_TRUE(lor(true, false));
        logical_not<bool> lnot;
        EXPECT_TRUE(lnot(false));
        EXPECT_FALSE(lnot(true));
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== functional 全面测试 ===\n");
    test_functional();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}