/*
 * cross_platform scaffold — 跨平台 C 编程演示
 *
 * 展示如何使用 #ifdef 实现平台适配：
 *   platform.h    — 平台检测宏定义
 *   platform.c    — 平台特定实现
 *   main.c        — 跨平台入口（本文件）
 *
 * 复现（单文件联编）：
 *   gcc -Wall -Wextra -O2 -std=c11 -o cross_demo main.c platform.c
 *   ./cross_demo
 */

#include <stdio.h>
#include "platform.h"

int main(void) {
    printf("=== 跨平台 C 编程演示 ===\n\n");

    /* --- OS 信息 --- */
    printf("--- 1. 系统信息 ---\n");
    printf("[platform] OS:       %s\n", PLATFORM_OS_NAME);
    printf("[platform] Compiler: %s\n",
#ifdef __GNUC__
           "GCC " PLATFORM_STRINGIFY(__GNUC__) "." PLATFORM_STRINGIFY(__GNUC_MINOR__)
#elif defined(_MSC_VER)
           "MSVC"
#else
           "unknown"
#endif
    );

    /* --- 硬件信息 --- */
    printf("\n--- 2. 硬件信息 ---\n");
    printf("[platform] CPU cores: %d\n", platform_cpu_count());
    printf("[platform] Page size: %lu bytes\n", platform_page_size());

    /* --- 字节序 --- */
    printf("\n--- 3. 字节序 ---\n");
    printf("[platform] Endianness: %s\n", is_little_endian() ? "Little-Endian" : "Big-Endian");

    unsigned int test_val = 0x12345678;
    unsigned char *bytes = (unsigned char *)&test_val;
    printf("[platform] 0x12345678 in memory: %02x %02x %02x %02x\n",
           bytes[0], bytes[1], bytes[2], bytes[3]);

    /* --- 路径分隔符 --- */
    printf("\n--- 4. 路径 ---\n");
    printf("[platform] Path separator: '%c'\n", PLATFORM_PATH_SEP);
    printf("[platform] Example path: usr%clocal%cbin\n",
           PLATFORM_PATH_SEP, PLATFORM_PATH_SEP);

    /* --- Sleep 跨平台 --- */
    printf("\n--- 5. Sleep ---\n");
    printf("[platform] Sleeping 100ms... ");
    fflush(stdout);
    platform_sleep_ms(100);
    printf("done\n");

    /* --- 动态库加载 --- */
    printf("\n--- 6. 动态库 ---\n");
    printf("[platform] Shared lib extension: %s\n", PLATFORM_SHARED_LIB_EXT);
    printf("[platform] Dlopen test: %s\n",
           platform_dlopen_test() ? "PASS" : "SKIP (no test lib available)");

    printf("\n=== PASS ===\n");
    return 0;
}
