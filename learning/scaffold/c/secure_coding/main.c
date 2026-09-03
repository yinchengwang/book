/*
 * secure_coding scaffold — 安全编程实践
 *
 * 五类经典 C 语言安全漏洞，每类配对应的安全实现。
 * 默认编译运行 safe 版本。用 -DVULN_DEMO 编译可观察漏洞行为。
 *
 * 复现：
 *   # Safe 版本（默认）
 *   gcc -Wall -Wextra -O2 -std=c11 -o secure_demo main.c
 *   ./secure_demo
 *
 *   # 安全编译选项
 *   gcc -Wall -Wextra -Wformat=2 -Wformat-security \
 *       -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
 *       -O2 -std=c11 -o secure_demo main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== 1. Buffer Overflow ========== */

/* 漏洞版：strcpy 无边界检查 */
void vuln_buffer_overflow(const char *input) {
    char buf[16];
    strcpy(buf, input);  /* 输入 >15 字符则栈溢出 */
    printf("[vuln] buffer overflow: \"%s\" → buf[16] (strcpy, 无边界检查)\n", buf);
}

/* 安全版：strncpy + 强制 null 终止 */
void safe_buffer_overflow(const char *input) {
    char buf[16];
    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';  /* 确保 null 终止 */
    printf("[safe] strncpy+null: \"%s\" (最大 %d 字符)\n",
           buf, (int)sizeof(buf) - 1);
}

/* ========== 2. Format String ========== */

/* 漏洞版：printf 的参数是用户输入 */
void vuln_format_string(const char *input) {
    /* printf(input) — 如果 input="%x %x %n" 则可读/写栈 */
    printf("[vuln] format string: ");
    printf(input);  /* 危险！ */
    printf("\n");
}

/* 安全版：固定格式字符串 */
void safe_format_string(const char *input) {
    printf("[safe] format string: %s\n", input);  /* "%s" 而非 input */
}

/* ========== 3. Integer Overflow ========== */

/* 漏洞版：malloc 参数溢出 */
void *vuln_int_overflow(int count, size_t item_size) {
    printf("[vuln] int overflow: count=%d × item_size=%zu = %zu (可能截断)\n",
           count, item_size, (size_t)count * item_size);
    return malloc((size_t)count * item_size);
    /* count=2^30, item_size=2^30 → 乘积=2^60 但 size_t 只有 32/64 位 */
}

/* 安全版：用 __builtin_mul_overflow 或手动检查 */
void *safe_int_overflow(int count, size_t item_size) {
    size_t total;
    if (__builtin_mul_overflow((size_t)count, item_size, &total)) {
        printf("[safe] int overflow detected: %d × %zu 溢出，拒绝分配\n",
               count, item_size);
        return NULL;
    }
    printf("[safe] checked mul: %d × %zu = %zu (OK)\n", count, item_size, total);
    return malloc(total);
}

/* ========== 4. Use-After-Free ========== */

/* 漏洞版：free 后继续使用指针 */
void vuln_uaf(void) {
    char *p = (char *)malloc(32);
    strcpy(p, "original data");
    printf("[vuln] before free: p=\"%s\"\n", p);
    free(p);
    /* p 现在是悬空指针——访问它是未定义行为 */
    strcpy(p, "use after free");  /* BUG: UAF */
    printf("[vuln] after free (UAF!): p=\"%s\"\n", p);
}

/* 安全版：free 后立即置 NULL */
void safe_uaf(void) {
    char *p = (char *)malloc(32);
    strcpy(p, "original data");
    printf("[safe] before free: p=\"%s\"\n", p);
    free(p);
    p = NULL;  /* 防御：悬空指针变为可控 NULL */
    if (p == NULL)
        printf("[safe] p=NULL after free, 拒绝访问 (null guard)\n");
    else
        strcpy(p, "use after free");
}

/* ========== 5. TOCTOU (Time-of-Check-Time-of-Use) ========== */

#include <errno.h>

/* 漏洞版：先检查再打开——两次操作之间文件可能被替换 */
void vuln_toctou(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        printf("[vuln] TOCTOU: fopen(\"%s\") failed: %s\n", path, strerror(errno));
        return;
    }
    /* 漏洞：fopen 和 fread 之间，另一个进程/线程可能替换了文件 */
    printf("[vuln] TOCTOU: fopen succeeded, but file could have been swapped\n");
    fclose(f);
}

/* 安全版：用文件描述符（open/fstat/fdopen）保证同一文件 */
void safe_toctou(const char *path) {
    printf("[safe] TOCTOU mitigation: use fd-based operations\n");
    printf("[safe] (Linux: open()→fstat()→fdopen() ensures same inode)\n");
    /* 简化 demo——Windows MinGW 无完整 POSIX，此处仅演示概念 */
    FILE *f = fopen(path, "r");
    if (f) {
        printf("[safe] file opened successfully (conceptual demo)\n");
        fclose(f);
    } else {
        printf("[safe] file not found (conceptual demo): %s\n", strerror(errno));
    }
}

/* ========== main ========== */

int main(void) {
    printf("=== 安全编程实践演示 ===\n");
    printf("默认运行 safe 版本。用 -DVULN_DEMO 编译查看漏洞行为。\n");
    printf("安全编译选项: -Wformat=2 -fstack-protector-strong -D_FORTIFY_SOURCE=2\n\n");

#ifdef VULN_DEMO
    printf("⚠  VULN_DEMO 模式——运行漏洞版本 ⚠\n\n");
    vuln_buffer_overflow("short");
    printf("  （长输入未演示——会触达栈溢出导致段错误）\n\n");

    vuln_format_string("normal text");
    vuln_format_string("%x %x %x %x");  /* 读栈内容 */
    printf("\n");

    vuln_int_overflow(1 << 20, 1 << 20);
    printf("\n");

    vuln_uaf();
    printf("\n");

    vuln_toctou("NOTES.md");
    printf("\n");
#else
    /* --- Safe 版本 --- */
    safe_buffer_overflow("hello (short input, OK)");
    safe_buffer_overflow("this input is way too long for the buffer but gets truncated safely");
    printf("\n");

    safe_format_string("normal text");
    safe_format_string("%x %x %x %x");  /* 被 "%s" 安全化——打印字面量 */
    printf("\n");

    safe_int_overflow(10, 100);         /* 正常 */
    safe_int_overflow(1 << 20, 1 << 20);/* 溢出检测 */
    printf("\n");

    safe_uaf();
    printf("\n");

    safe_toctou("NOTES.md");
    printf("\n");
#endif

    printf("=== PASS ===\n");
    return 0;
}
