/*
 * test_utility.cpp - utility 全面测试
 *
 * 覆盖：move / forward / swap / exchange / pair / make_pair / integer_sequence
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstring>

using namespace mystl;

namespace {

struct Movable {
    int val;
    bool moved = false;
    Movable(int v) : val(v) {}
    Movable(const Movable&) = default;
    Movable& operator=(const Movable&) = default;
    Movable(Movable&& o) noexcept : val(o.val), moved(true) { o.val = -1; }
    Movable& operator=(Movable&& o) noexcept {
        val = o.val; moved = true; o.val = -1; return *this;
    }
};

} // namespace

void test_utility() {
    // ============ move ============
    MYSTL_TEST_BEGIN("move");
    {
        int x = 42;
        int&& r = move(x);
        EXPECT_EQ(r, 42);

        Movable m1(10);
        Movable m2 = move(m1);
        EXPECT_TRUE(m2.moved);
        EXPECT_EQ(m1.val, -1);
    }
    MYSTL_TEST_END();

    // ============ forward ============
    MYSTL_TEST_BEGIN("forward");
    {
        int x = 42;
        int& r = x;
        EXPECT_EQ(forward<int&>(r), 42);
        EXPECT_EQ(forward<int>(42), 42);
    }
    MYSTL_TEST_END();

    // ============ swap ============
    MYSTL_TEST_BEGIN("swap");
    {
        int a = 1, b = 2;
        swap(a, b);
        EXPECT_EQ(a, 2);
        EXPECT_EQ(b, 1);
    }
    MYSTL_TEST_END();

    // ============ exchange ============
    MYSTL_TEST_BEGIN("exchange");
    {
        int x = 10;
        int old = exchange(x, 20);
        EXPECT_EQ(old, 10);
        EXPECT_EQ(x, 20);
    }
    MYSTL_TEST_END();

    // ============ pair ============
    MYSTL_TEST_BEGIN("pair_default_construct");
    {
        pair<int, double> p;
        EXPECT_EQ(p.first, 0);
        EXPECT_EQ(p.second, 0.0);
    }
    // pair<const int, double> 默认构造（const 成员可以被默认初始化）
    {
        pair<const int, double> p{1, 3.14};
        EXPECT_EQ(p.first, 1);
        EXPECT_EQ(p.second, 3.14);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pair_value_construct");
    {
        pair<int, double> p(1, 3.14);
        EXPECT_EQ(p.first, 1);
        EXPECT_NE(p.second, 2.0);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pair_copy");
    {
        pair<int, double> p1(1, 3.14);
        pair<int, double> p2(p1);
        EXPECT_EQ(p2.first, 1);
        EXPECT_EQ(p2.second, 3.14);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pair_move");
    {
        Movable m(42);
        pair<int, Movable> p1(1, move(m));
        pair<int, Movable> p2(move(p1));
        EXPECT_EQ(p2.first, 1);
        EXPECT_EQ(p2.second.val, 42);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pair_convert");
    {
        pair<int, double> p1(1, 2.5);
        pair<long, float> p2(p1);
        EXPECT_EQ(p2.first, 1L);
        EXPECT_EQ(p2.second, 2.5f);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pair_comparison");
    {
        pair<int, int> a(1, 2), b(1, 2), c(1, 3);
        EXPECT_TRUE(a == b);
        EXPECT_FALSE(a != b);
        EXPECT_TRUE(a < c);
        EXPECT_FALSE(c < a);
        EXPECT_TRUE(c > a);
        EXPECT_TRUE(a <= b);
        EXPECT_TRUE(c >= b);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pair_swap");
    {
        pair<int, int> a(1, 2), b(3, 4);
        a.swap(b);
        EXPECT_EQ(a.first, 3);
        EXPECT_EQ(b.second, 2);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("pair_const_first");
    {
        // pair<const int, double> 在 map 中使用
        pair<const int, double> p1(1, 3.14);
        EXPECT_EQ(p1.first, 1);
        // 拷贝构造
        pair<const int, double> p2(p1);
        EXPECT_EQ(p2.first, 1);
        // 拷贝赋值被 delete（const 不可赋值）
    }
    MYSTL_TEST_END();

    // ============ make_pair ============
    MYSTL_TEST_BEGIN("make_pair");
    {
        auto p = make_pair(1, 3.14);
        static_assert(is_same<decltype(p), pair<int, double>>::value, "");
        EXPECT_EQ(p.first, 1);
    }
    MYSTL_TEST_END();

    // ============ integer_sequence ============
    MYSTL_TEST_BEGIN("integer_sequence");
    {
        using seq = integer_sequence<int, 0, 1, 2>;
        static_assert(seq::size() == 3, "");
        static_assert(is_same<seq::value_type, int>::value, "");
    }
    {
        using iseq = index_sequence<0, 1, 2, 3>;
        static_assert(iseq::size() == 4, "");
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== utility 全面测试 ===\n");
    test_utility();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}