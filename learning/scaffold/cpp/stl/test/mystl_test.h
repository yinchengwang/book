/*
 * mystl_test.h - 统一的极简测试框架
 *
 * 提供以下宏：
 *   MYSTL_TEST_BEGIN(name)  - 测试用例开始
 *   MYSTL_TEST_END          - 测试用例结束
 *   EXPECT_EQ(a, b)         - 期望 a == b
 *   EXPECT_NE(a, b)         - 期望 a != b
 *   EXPECT_LT(a, b)         - 期望 a < b
 *   EXPECT_TRUE(x)          - 期望 x 为真
 *   EXPECT_FALSE(x)         - 期望 x 为假
 *   ASSERT_EQ(a, b)         - 断言 a == b（失败立即退出）
 *   ASSERT_TRUE(x)          - 断言 x 为真
 */

#ifndef MYSTL_TEST_H
#define MYSTL_TEST_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mystl_test {

struct TestStats {
    int total_cases = 0;
    int passed = 0;
    int failed = 0;
};

inline TestStats& stats() {
    static TestStats s;
    return s;
}

inline int& current_case_failed() {
    static int f = 0;
    return f;
}

inline char* current_case_name() {
    static char buf[256] = "";
    return buf;
}

} // namespace mystl_test

#define MYSTL_TEST_BEGIN(name) \
    do { \
        snprintf(mystl_test::current_case_name(), sizeof(mystl_test::current_case_name()), "%s", name); \
        mystl_test::current_case_failed() = 0; \
        mystl_test::stats().total_cases++; \
        printf("\n[%s] %s\n", #name[0] == '\0' ? "case" : "case", name); \
    } while (0)

#define MYSTL_TEST_END() \
    do { \
        if (mystl_test::current_case_failed() == 0) { \
            mystl_test::stats().passed++; \
            printf("  [PASS]\n"); \
        } else { \
            mystl_test::stats().failed++; \
            printf("  [FAIL] %d assertions failed\n", mystl_test::current_case_failed()); \
        } \
    } while (0)

#define EXPECT_EQ(a, b) \
    do { \
        auto _va = (a); \
        auto _vb = (b); \
        if (!(_va == _vb)) { \
            printf("  [FAIL] %s:%d: EXPECT_EQ(%s, %s)\n  ", \
                   __FILE__, __LINE__, #a, #b); \
            mystl_test::current_case_failed()++; \
        } \
    } while (0)

#define EXPECT_NE(a, b) \
    do { \
        auto _va = (a); \
        auto _vb = (b); \
        if (!(_va != _vb)) { \
            printf("  [FAIL] %s:%d: EXPECT_NE\n", __FILE__, __LINE__); \
            mystl_test::current_case_failed()++; \
        } \
    } while (0)

#define EXPECT_LT(a, b) \
    do { \
        auto _va = (a); \
        auto _vb = (b); \
        if (!(_va < _vb)) { \
            printf("  [FAIL] %s:%d: EXPECT_LT\n", __FILE__, __LINE__); \
            mystl_test::current_case_failed()++; \
        } \
    } while (0)

#define EXPECT_TRUE(x) \
    do { \
        if (!(x)) { \
            printf("  [FAIL] %s:%d: EXPECT_TRUE(%s)\n", __FILE__, __LINE__, #x); \
            mystl_test::current_case_failed()++; \
        } \
    } while (0)

#define EXPECT_FALSE(x) \
    do { \
        if ((x)) { \
            printf("  [FAIL] %s:%d: EXPECT_FALSE(%s)\n", __FILE__, __LINE__, #x); \
            mystl_test::current_case_failed()++; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        auto _va = (a); \
        auto _vb = (b); \
        if (!(_va == _vb)) { \
            printf("  [FATAL] %s:%d: ASSERT_EQ failed\n", __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_TRUE(x) \
    do { \
        if (!(x)) { \
            printf("  [FATAL] %s:%d: ASSERT_TRUE failed\n", __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define MYSTL_TEST_SUMMARY() \
    do { \
        printf("\n========================================\n"); \
        printf("总计: %d 个测试用例  通过: %d  失败: %d\n", \
               mystl_test::stats().total_cases, \
               mystl_test::stats().passed, \
               mystl_test::stats().failed); \
        printf("========================================\n"); \
    } while (0)

#endif // MYSTL_TEST_H