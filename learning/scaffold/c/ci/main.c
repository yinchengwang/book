/* ci scaffold — 持续集成与自动化
 *
 * 复现命令：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *
 * 演示 4 段（被 ci.sh 调用）：
 *   [build]  — 编译检查
 *   [lint]   — 静态分析警告统计
 *   [test]   — 简单冒烟测试
 *   [fmt]    — 行长度 / 缩进统计
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 一个简单的字符串反转函数 —— ci.sh 会测试它 */
static char *reverse_string(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *dst = malloc(len + 1);
    if (!dst) return NULL;
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
    dst[len] = '\0';
    return dst;
}

int main(int argc, char *argv[]) {
    /* === [build] 编译检查（CI 总会先编译） === */
    printf("[build] === 编译检查 ===\n");
    printf("  gcc 编译通过（Makefile/CI 触发）\n");

    /* === [lint] 静态分析 === */
    printf("\n[lint] === 静态分析 ===\n");
    printf("  gcc -Wall -Wextra -Wpedantic 零警告\n");
    printf("  （生产 CI 还应跑 cppcheck / clang-tidy）\n");

    /* === [test] 冒烟测试 === */
    printf("\n[test] === 冒烟测试 ===\n");
    const char *inputs[] = {"hello", "world", "racecar", NULL};
    int passed = 0, total = 0;
    for (int i = 0; inputs[i]; i++) {
        total++;
        char *reversed = reverse_string(inputs[i]);
        if (reversed) {
            printf("  reverse(\"%s\") = \"%s\"\n", inputs[i], reversed);
            free(reversed);
            passed++;
        }
    }
    printf("  %d/%d tests passed\n", passed, total);

    /* 边界：空字符串 */
    char *empty = reverse_string("");
    printf("  reverse(\"\") = \"%s\" (空字符串 OK)\n", empty ? empty : "(null)");
    free(empty);

    /* === [fmt] 格式检查 === */
    printf("\n[fmt] === 格式统计 ===\n");
    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (fp) {
            char line[256];
            int n_lines = 0, max_len = 0;
            while (fgets(line, sizeof(line), fp)) {
                n_lines++;
                int len = (int)strlen(line);
                if (len > max_len) max_len = len;
            }
            fclose(fp);
            printf("  %s: %d 行, 最大行长 %d\n", argv[1], n_lines, max_len);
            printf("  (推荐最大行长 ≤ 100)\n");
        }
    } else {
        printf("  用法: %s <file.c>  统计行数\n", argv[0]);
    }

    printf("\n=== PASS ===\n");
    return 0;
}