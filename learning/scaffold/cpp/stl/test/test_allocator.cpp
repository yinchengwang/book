/*
 * test_allocator.cpp - allocator 全面测试
 *
 * 覆盖：allocator / allocator_traits / uninitialized_copy / fill / fill_n / move
 */

#include "mystl/mystl.h"
#include "mystl_test.h"
#include <cstdio>
#include <cstring>

using namespace mystl;

struct Counted {
    static int alive;
    int id;
    Counted(int i = 0) : id(i) { ++alive; }
    Counted(const Counted& o) : id(o.id) { ++alive; }
    Counted& operator=(const Counted& o) { id = o.id; return *this; }
    ~Counted() { --alive; }
};
int Counted::alive = 0;

void test_allocator() {
    // ============ allocator ============
    MYSTL_TEST_BEGIN("allocator_basic");
    {
        allocator<int> a;
        int* p = a.allocate(5);
        EXPECT_TRUE(p != nullptr);
        a.deallocate(p, 5);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("allocator_construct_destroy");
    {
        allocator<int> a;
        int* p = a.allocate(1);
        a.construct(p, 42);
        EXPECT_EQ(*p, 42);
        a.destroy(p);
        a.deallocate(p, 1);
    }
    MYSTL_TEST_END();

    MYSTL_TEST_BEGIN("allocator_rebind");
    {
        allocator<int> a;
        allocator<int>::rebind<double>::other da;
        double* p = da.allocate(3);
        EXPECT_TRUE(p != nullptr);
        da.deallocate(p, 3);
    }
    MYSTL_TEST_END();

    // ============ allocator_traits ============
    MYSTL_TEST_BEGIN("allocator_traits");
    {
        using traits = allocator_traits<allocator<int>>;
        static_assert(is_same<traits::value_type, int>::value, "");
        // rebind
        using rebind_t = traits::rebind_alloc<double>;
        rebind_t ra;
        double* p = ra.allocate(2);
        EXPECT_TRUE(p != nullptr);
        ra.deallocate(p, 2);
    }
    MYSTL_TEST_END();

    // ============ uninitialized_copy ============
    MYSTL_TEST_BEGIN("uninitialized_copy");
    {
        int src[4] = {1, 2, 3, 4};
        allocator<int> a;
        int* dst = a.allocate(4);
        uninitialized_copy(src, src + 4, dst);
        EXPECT_EQ(dst[0], 1);
        EXPECT_EQ(dst[3], 4);
        // 手动 destruct + deallocate
        for (int i = 0; i < 4; ++i) a.destroy(dst + i);
        a.deallocate(dst, 4);
    }
    MYSTL_TEST_END();

    // ============ uninitialized_fill ============
    MYSTL_TEST_BEGIN("uninitialized_fill");
    {
        allocator<int> a;
        int* p = a.allocate(5);
        uninitialized_fill(p, p + 5, 99);
        for (int i = 0; i < 5; ++i) EXPECT_EQ(p[i], 99);
        for (int i = 0; i < 5; ++i) a.destroy(p + i);
        a.deallocate(p, 5);
    }
    MYSTL_TEST_END();

    // ============ uninitialized_fill_n ============
    MYSTL_TEST_BEGIN("uninitialized_fill_n");
    {
        allocator<int> a;
        int* p = a.allocate(3);
        uninitialized_fill_n(p, 3, 77);
        EXPECT_EQ(p[0], 77);
        EXPECT_EQ(p[2], 77);
        for (int i = 0; i < 3; ++i) a.destroy(p + i);
        a.deallocate(p, 3);
    }
    MYSTL_TEST_END();

    // ============ uninitialized_move ============
    MYSTL_TEST_BEGIN("uninitialized_move");
    {
        allocator<int> a;
        int* src = a.allocate(4);
        uninitialized_fill_n(src, 4, 10);
        int* dst = a.allocate(4);
        uninitialized_move(src, src + 4, dst);
        EXPECT_EQ(dst[0], 10);
        EXPECT_EQ(dst[3], 10);
        for (int i = 0; i < 4; ++i) { a.destroy(src + i); a.destroy(dst + i); }
        a.deallocate(src, 4);
        a.deallocate(dst, 4);
    }
    MYSTL_TEST_END();
}

int main() {
    printf("=== allocator 全面测试 ===\n");
    test_allocator();
    MYSTL_TEST_SUMMARY();
    return mystl_test::stats().failed > 0 ? 1 : 0;
}