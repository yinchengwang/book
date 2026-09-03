/* preprocessor scaffold — C 预处理器与宏
 *
 * 复现命令：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *   gcc -E -dM - < /dev/null    # 查看本机所有预定义宏
 *
 * 演示 8 段：
 *   [define]    — 简单对象宏 + 常量宏
 *   [func]      — 函数宏（带副作用坑）
 *   [stringfy]  — # 操作符（字符串化）
 *   [concat]    — ## 操作符（记号拼接）
 *   [ifdef]     — 条件编译
 *   [once]      — #pragma once vs include guard
 *   [do-while]  — do{ }while(0) 多语句宏
 *   [variadic]  — __VA_ARGS__ 变参宏
 */

#include <stdio.h>

/* === 8 段预处理演示 === */

#define PI              3.14159
#define MAX_PATH_LEN    4096
#define VERSION_MAJOR   1
#define VERSION_MINOR   2
#define VERSION_STR     "1.2"

#define SQUARE(x)       ((x) * (x))                 /* 函数宏：必须外加括号 */
#define DOUBLE(x)       ((x) + (x))                 /* 加括号防优先级坑 */
#define MAX(a, b)       ((a) > (b) ? (a) : (b))

#define STR(x)          #x                         /* 字符串化 */
#define CONCAT(a, b)    a##b                        /* 记号拼接 */

/* === 条件编译：按平台/编译器选路径 === */
#if defined(__linux__)
  #define PLATFORM_NAME "Linux"
#elif defined(_WIN32)
  #define PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
  #define PLATFORM_NAME "macOS"
#else
  #define PLATFORM_NAME "Unknown"
#endif

#define DEBUG_LEVEL     2

#if DEBUG_LEVEL > 0
  #define LOG_INFO(fmt, ...)   fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_INFO(fmt, ...)   ((void)0)
#endif

#if DEBUG_LEVEL > 1
  #define LOG_DEBUG(fmt, ...)  fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_DEBUG(fmt, ...)  ((void)0)
#endif

/* === do-while(0) 多语句宏：可在 if/else 中安全使用 === */
#define SWAP_INT(a, b)  do {                        \
                            int tmp = (a);          \
                            (a) = (b);             \
                            (b) = tmp;             \
                        } while (0)

#define CHECK(cond)     do {                        \
                            if (!(cond)) {          \
                                fprintf(stderr, "CHECK failed: %s\n", #cond); \
                                return -1;          \
                            }                       \
                        } while (0)

int main(void) {
    /* === [define] 简单对象宏 === */
    printf("[define] === 对象宏 ===\n");
    printf("  PI            = %f\n", PI);
    printf("  VERSION_STR   = \"%s\"\n", VERSION_STR);

    /* === [func] 函数宏 === */
    printf("\n[func] === 函数宏 ===\n");
    printf("  SQUARE(5)     = %d\n", SQUARE(5));             /* 25 */
    printf("  DOUBLE(10)    = %d\n", DOUBLE(10));             /* 20 */
    printf("  MAX(3,7)      = %d\n", MAX(3, 7));              /* 7 */

    /* 副作用演示：MAX(++a, ++b) 中 a/b 多次 ++ */
    int a = 3, b = 7;
    int m = MAX(a, b);                                      /* a < b，返回 b，b 不变 */
    printf("  MAX(a,b) 副作用 = %d (a=%d, b=%d)\n", m, a, b);

    /* === [stringfy] 字符串化 === */
    printf("\n[stringfy] === # 操作符 ===\n");
    printf("  STR(hello)    = \"%s\"\n", STR(hello));        /* "hello" */
    printf("  STR(PI)       = \"%s\"\n", STR(PI));           /* "PI"（不展开！） */

    /* === [concat] 记号拼接 === */
    printf("\n[concat] === ## 操作符 ===\n");
    int xy = 42;
    printf("  CONCAT(x,y)   = %d\n", CONCAT(x, y));          /* xy */
    printf("  整数变量名 xy = 42\n");

    /* === [ifdef] 条件编译 === */
    printf("\n[ifdef] === 条件编译（按平台选） ===\n");
    printf("  PLATFORM_NAME = \"%s\"\n", PLATFORM_NAME);

    /* === [once] #pragma once（演示在头文件中用法）=== */
    printf("\n[once] === #pragma once vs include guard ===\n");
    /* #pragma once 是 GCC/Clang/MSVC 通用扩展，等价于传统 include guard */
    /*   传统方式：
     *     #ifndef HEADER_GUARD_NAME
     *     #define HEADER_GUARD_NAME
     *     ... 内容 ...
     *     #endif
     *   现代方式：
     *     #pragma once
     *     ... 内容 ...
     */
    printf("  #pragma once 由 GCC/Clang/MSVC 三家支持\n");
    printf("  头文件守卫推荐 #pragma once（简洁）\n");

    /* === [do-while] 多语句宏 === */
    printf("\n[do-while] === do{ }while(0) 多语句宏 ===\n");
    int x = 10, y = 20;
    printf("  SWAP 前: x=%d, y=%d\n", x, y);
    SWAP_INT(x, y);
    printf("  SWAP 后: x=%d, y=%d\n", x, y);

    /* === [variadic] 变参宏 === */
    printf("\n[variadic] === __VA_ARGS__ ===\n");
    LOG_INFO("系统启动, version=%s, platform=%s", VERSION_STR, PLATFORM_NAME);
    LOG_DEBUG("a=%d, b=%d", a, b);
    LOG_INFO("无参版本（##__VA_ARGS__ 兼容 GCC/Clang）");

    /* CHECK 宏示例 —— 直接调用（本身 return -1），无需外层 if */
    CHECK(a > 0);
    CHECK(b > 0);
    printf("  所有 CHECK 通过\n");

    printf("\n=== PASS ===\n");
    return 0;
}