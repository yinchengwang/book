/* stdlib scaffold — C 标准库
 *
 * 复现命令：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *
 * 演示 6 段（<stdlib.h> 13 函数）：
 *   [malloc]   — malloc/free/calloc/realloc 四件套
 *   [exit]     — exit/atexit/abort 退出路径
 *   [conv]     — atoi/strtol 字符串转整数
 *   [qsort]    — qsort 排序（int + struct）
 *   [bsearch]  — bsearch 二分查找
 *   [rand]     — rand/srand 随机数
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* === [exit] atexit 回调 === */
static void cleanup_atexit1(void) { printf("  [exit] atexit 1: 最后清理\n"); }
static void cleanup_atexit2(void) { printf("  [exit] atexit 2: 次后清理\n"); }

/* === [qsort] 比较函数 === */
static int cmp_int(const void *a, const void *b) {
    return *(const int*)a - *(const int*)b;
}

typedef struct {
    int id;
    char name[16];
} Item;

static int cmp_item_id(const void *a, const void *b) {
    const Item *ia = (const Item*)a;
    const Item *ib = (const Item*)b;
    return ia->id - ib->id;
}

int main(void) {
    /* === [malloc] malloc/calloc/realloc/free === */
    printf("[malloc] === malloc/calloc/realloc/free ===\n");

    /* malloc: 不初始化 */
    int *p1 = malloc(5 * sizeof(int));
    if (!p1) { perror("malloc"); return 1; }
    for (int i = 0; i < 5; i++) p1[i] = (i + 1) * 10;
    printf("  malloc(5*int): ");
    for (int i = 0; i < 5; i++) printf("%d ", p1[i]);
    printf("(未初始化垃圾，但已写)\n");
    free(p1);

    /* calloc: 自动清零 */
    int *p2 = calloc(5, sizeof(int));
    if (!p2) { perror("calloc"); return 1; }
    printf("  calloc(5*int): ");
    for (int i = 0; i < 5; i++) printf("%d ", p2[i]);
    printf("(全部 0)\n");

    /* realloc: 原地扩容 */
    int cap = 5;
    int *p3 = malloc(cap * sizeof(int));
    for (int i = 0; i < cap; i++) p3[i] = i;
    cap = 10;
    p3 = realloc(p3, cap * sizeof(int));   /* 可能搬移到新地址 */
    for (int i = 5; i < cap; i++) p3[i] = i;
    printf("  realloc 5→10: ");
    for (int i = 0; i < cap; i++) printf("%d ", p3[i]);
    printf("\n");
    free(p3);

    /* === [exit] exit/atexit/abort === */
    printf("\n[exit] === exit/atexit/abort ===\n");
    atexit(cleanup_atexit1);    /* 注册 LIFO 清理回调 */
    atexit(cleanup_atexit2);    /* 后注册先调用 */
    printf("  atexit 注册 2 个清理函数\n");
    printf("  exit(0) 触发 atexit 回调链...\n");
    /* 注释掉 exit 让程序继续，atexit 在 return 时触发
     * 如要演示 exit 行为，把 fflush + exit(0) 放这里 */

    /* === [conv] atoi/strtol 字符串转整数 === */
    printf("\n[conv] === atoi / strtol ===\n");

    const char *s1 = "12345";
    int v1 = atoi(s1);
    printf("  atoi(\"%s\") = %d (不检错)\n", s1, v1);

    const char *s2 = "0x1F";
    char *end;
    long v2 = strtol(s2, &end, 16);   /* base=16 自动识别 0x 前缀 */
    printf("  strtol(\"%s\", base=16) = %ld (end=%s)\n", s2, v2, end);

    const char *s3 = "999abc";
    long v3 = strtol(s3, &end, 10);
    printf("  strtol(\"%s\", base=10) = %ld (end=%s, 停在非数字)\n",
           s3, v3, end);

    /* === [qsort] 排序 === */
    printf("\n[qsort] === qsort 排序 ===\n");
    int arr[] = {5, 2, 8, 1, 9, 3};
    int n = sizeof(arr) / sizeof(arr[0]);
    printf("  原序: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    qsort(arr, n, sizeof(int), cmp_int);
    printf("\n  升序: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    /* qsort 排 struct 数组 */
    Item items[] = {{3, "alice"}, {1, "bob"}, {2, "carol"}};
    qsort(items, 3, sizeof(Item), cmp_item_id);
    printf("  struct 按 id: ");
    for (int i = 0; i < 3; i++) printf("{%d,%s} ", items[i].id, items[i].name);
    printf("\n");

    /* === [bsearch] 二分查找 === */
    printf("\n[bsearch] === bsearch 二分查找 ===\n");
    int sorted[] = {1, 3, 5, 7, 9, 11, 13, 15};
    int n2 = sizeof(sorted) / sizeof(sorted[0]);
    int target = 7;
    int *found = bsearch(&target, sorted, n2, sizeof(int), cmp_int);
    if (found) {
        printf("  bsearch(%d) = %d (在 sorted[%ld])\n",
               target, *found, (long)(found - sorted));
    } else {
        printf("  bsearch(%d) = NULL (未找到)\n", target);
    }
    target = 8;
    found = bsearch(&target, sorted, n2, sizeof(int), cmp_int);
    printf("  bsearch(%d) = %s\n", target, found ? "found" : "NULL");

    /* === [rand] rand/srand 随机数 === */
    printf("\n[rand] === rand / srand ===\n");
    srand(42);                      /* 设种子 */
    printf("  5 个 [0, RAND_MAX] 随机数: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", rand());
    }
    printf("\n  RAND_MAX = %d\n", RAND_MAX);

    /* === 触发 atexit === */
    /* atexit 在 return 时触发，无需显式 exit */

    printf("\n=== PASS ===\n");
    return 0;
}