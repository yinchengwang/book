/* error_handling scaffold — 错误处理与 errno
 *
 * 复现命令：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *
 * 演示 5 段：
 *   [errno]    — errno 全局错误码
 *   [perror]   — perror 自动拼 errno 描述
 *   [strerror] — strerror 把 errno 值转为字符串
 *   [assert]   — assert 调试断言
 *   [defensive] — 防御式编程（参数检查 + 错误码返回）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

/* === 自定义错误码体系 === */
typedef enum {
    ERR_OK = 0,
    ERR_INVALID_ARG = -1,
    ERR_OUT_OF_RANGE = -2,
    ERR_NULL_POINTER = -3,
    ERR_OVERFLOW = -4,
} ErrorCode;

/* === 防御式编程：返回错误码而非崩溃 === */
static ErrorCode safe_divide(int a, int b, int *out) {
    if (!out) return ERR_NULL_POINTER;          /* 检查输出指针 */
    if (b == 0) return ERR_INVALID_ARG;          /* 除零保护 */
    if (a == INT_MIN && b == -1) return ERR_OVERFLOW;  /* INT_MIN / -1 = UB */
    *out = a / b;
    return ERR_OK;
}

int main(void) {
    /* === [errno] errno 全局错误码 === */
    printf("[errno] === errno 全局错误码 ===\n");

    errno = 0;
    FILE *fp = fopen("nonexistent_file_xyz.txt", "r");
    if (!fp) {
        printf("  fopen 失败, errno = %d (ENONENT=%d)\n", errno, ENOENT);
    }

    errno = 0;
    long big = strtol("999999999999999999999", NULL, 10);
    if (errno == ERANGE) {
        printf("  strtol 溢出, errno = %d (ERANGE=%d), 返回 %ld\n",
               errno, ERANGE, big);
    }

    /* === [perror] perror 自动拼 errno 描述 === */
    printf("\n[perror] === perror 自动打印 errno 描述 ===\n");
    errno = 0;
    fp = fopen("nonexistent_xyz.txt", "r");
    if (!fp) {
        perror("  fopen failed");                /* 格式: "fopen failed: No such file or directory\n" */
    }

    /* === [strerror] strerror 把 errno 值转字符串 === */
    printf("\n[strerror] === strerror(errno) 转字符串 ===\n");
    int errs[] = {EACCES, EBADF, ENOENT, ENOMEM, ERANGE};
    const char *names[] = {"EACCES", "EBADF", "ENOENT", "ENOMEM", "ERANGE"};
    for (int i = 0; i < 5; i++) {
        printf("  %s = %d -> \"%s\"\n",
               names[i], errs[i], strerror(errs[i]));
    }

    /* === [assert] assert 调试断言 === */
    printf("\n[assert] === assert 调试断言 ===\n");
    /* assert 在 NDEBUG 定义时被禁用 */
#ifdef NDEBUG
    printf("  NDEBUG 已定义，assert 被禁用\n");
#else
    printf("  NDEBUG 未定义，assert 生效\n");
    int x = 42;
    assert(x > 0);                              /* 不触发 */
    printf("  assert(x > 0) 通过 (x=%d)\n", x);
    /* assert(x < 0) 会触发 abort() + 打印文件行号 */
#endif

    /* === [defensive] 防御式编程 === */
    printf("\n[defensive] === 防御式编程（错误码返回） ===\n");

    int result;
    ErrorCode rc;

    rc = safe_divide(10, 3, &result);
    printf("  safe_divide(10, 3) = %d, rc=%d (OK)\n", result, rc);

    rc = safe_divide(10, 0, &result);
    printf("  safe_divide(10, 0) = rc=%d (ERR_INVALID_ARG)\n", rc);

    rc = safe_divide(10, 3, NULL);
    printf("  safe_divide(10, 3, NULL) = rc=%d (ERR_NULL_POINTER)\n", rc);

    rc = safe_divide(INT_MIN, -1, &result);
    printf("  safe_divide(INT_MIN, -1) = rc=%d (ERR_OVERFLOW, 防 UB)\n", rc);

    printf("\n=== PASS ===\n");
    return 0;
}