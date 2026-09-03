/*
 * test_memory.cpp - memory 全面测试
 *
 * 覆盖：addressof / default_delete / unique_ptr / shared_ptr / weak_ptr
 *      / make_unique / make_shared
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>

using namespace mystl;

struct Point {
    int x, y;
    Point(int a = 0, int b = 0) : x(a), y(b) {}
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

void test_memory() {
    // ============ addressof ============
    MYSTL_TEST_BEGIN("addressof");
    {
        int x = 42;
        EXPECT_EQ(addressof(x), &x);
    }
    MYSTL_TEST_END();

    // ============ default_delete ============
    MYSTL_TEST_BEGIN("default_delete");
    {
        default_delete<int> d;
        // 单纯验证可构造、可调用
        int* p = new int(5);
        d(p); // 释放
    }
    MYSTL_TEST_END();

    // ============ unique_ptr ============
    MYSTL_TEST_BEGIN("unique_ptr_basic");
    {
        unique_ptr<int> p(new int(42));
        EXPECT_TRUE(p);
        EXPECT_EQ(*p, 42);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("unique_ptr_arrow");
    {
        unique_ptr<Point> p(new Point(3, 4));
        EXPECT_EQ(p->x, 3);
        EXPECT_EQ(p->y, 4);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("unique_ptr_move");
    {
        unique_ptr<int> p1(new int(10));
        unique_ptr<int> p2 = move(p1);
        EXPECT_FALSE(p1);  // p1 被移动
        EXPECT_TRUE(p2);
        EXPECT_EQ(*p2, 10);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("unique_ptr_release");
    {
        unique_ptr<int> p(new int(99));
        int* raw = p.release();
        EXPECT_FALSE(p);
        EXPECT_EQ(*raw, 99);
        delete raw;
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("unique_ptr_reset");
    {
        unique_ptr<int> p(new int(1));
        p.reset(new int(2));
        EXPECT_EQ(*p, 2);
        p.reset();
        EXPECT_FALSE(p);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("unique_ptr_swap");
    {
        unique_ptr<int> p1(new int(10));
        unique_ptr<int> p2(new int(20));
        p1.swap(p2);
        EXPECT_EQ(*p1, 20);
        EXPECT_EQ(*p2, 10);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("unique_ptr_nullptr_compare");
    {
        unique_ptr<int> p;
        EXPECT_TRUE(p == nullptr);
        EXPECT_FALSE(p != nullptr);
        p.reset(new int(1));
        EXPECT_FALSE(p == nullptr);
        EXPECT_TRUE(p != nullptr);
    }
    MYSTL_TEST_END();

    // ============ make_unique ============
    MYSTL_TEST_BEGIN("make_unique");
    {
        auto p = make_unique<int>(42);
        EXPECT_EQ(*p, 42);
    }
    {
        auto p = make_unique<Point>(1, 2);
        EXPECT_EQ(p->x, 1);
        EXPECT_EQ(p->y, 2);
    }
    MYSTL_TEST_END();

    // ============ shared_ptr ============
    MYSTL_TEST_BEGIN("shared_ptr_basic");
    {
        auto p = make_shared<int>(42);
        EXPECT_EQ(*p, 42);
        EXPECT_EQ(p.use_count(), 1L);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("shared_ptr_copy");
    {
        shared_ptr<int> p1 = make_shared<int>(10);
        shared_ptr<int> p2(p1);
        EXPECT_EQ(*p2, 10);
        EXPECT_EQ(p1.use_count(), 2L);
        EXPECT_EQ(p2.use_count(), 2L);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("shared_ptr_assign");
    {
        shared_ptr<int> p1 = make_shared<int>(1);
        shared_ptr<int> p2 = make_shared<int>(2);
        p2 = p1;
        EXPECT_EQ(*p2, 1);
        EXPECT_EQ(p1.use_count(), 2L);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("shared_ptr_reset");
    {
        shared_ptr<int> p = make_shared<int>(5);
        p.reset(new int(10));
        EXPECT_EQ(*p, 10);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("shared_ptr_nullptr_compare");
    {
        shared_ptr<int> p;
        EXPECT_TRUE(p == nullptr);
        EXPECT_FALSE(p != nullptr);
        p = make_shared<int>(1);
        EXPECT_FALSE(p == nullptr);
        EXPECT_TRUE(p != nullptr);
    }
    MYSTL_TEST_END();

    // ============ weak_ptr ============
    MYSTL_TEST_BEGIN("weak_ptr_basic");
    {
        auto sp = make_shared<int>(42);
        weak_ptr<int> wp(sp);
        EXPECT_EQ(wp.use_count(), 1L);
        EXPECT_FALSE(wp.expired());
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("weak_ptr_lock");
    {
        auto sp = make_shared<int>(42);
        weak_ptr<int> wp(sp);
        auto locked = wp.lock();
        EXPECT_TRUE(locked);
        EXPECT_EQ(*locked, 42);
        EXPECT_EQ(sp.use_count(), 2L);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("weak_ptr_expire");
    {
        weak_ptr<int> wp;
        {
            auto sp = make_shared<int>(42);
            wp = sp;
            EXPECT_FALSE(wp.expired());
        }
        EXPECT_TRUE(wp.expired());
        auto locked = wp.lock();
        EXPECT_FALSE(locked);
    }
    MYSTL_TEST_END();

    // ============ make_shared ============
    MYSTL_TEST_BEGIN("make_shared");
    {
        auto p = make_shared<int>(100);
        EXPECT_EQ(*p, 100);
        EXPECT_EQ(p.use_count(), 1L);
    }
    {
        auto p = make_shared<Point>(5, 6);
        EXPECT_EQ(p->x, 5);
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== memory 全面测试 ===\n");
    test_memory();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}