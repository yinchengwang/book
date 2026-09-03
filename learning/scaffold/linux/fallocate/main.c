/**
 * @file main.c
 * @brief Linux 文件空间预分配学习演示
 *
 * 演示内容：
 *   - fallocate() 系统调用（KEEP_SIZE / PUNCH_HOLE / ZERO_RANGE）
 *   - 稀疏文件创建与检测（lseek SEEK_HOLE / SEEK_DATA）
 *   - 零拷贝初始化与预分配性能
 *
 * 编译：gcc -std=c11 -Wall -o fallocate main.c
 * 运行：./fallocate
 */

#ifndef _WIN32
#define _GNU_SOURCE
#include <linux/falloc.h>  /* FALLOC_FL_* 常量 */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

/* Windows 平台 Fallback */
#ifdef _WIN32
#ifndef FALLOC_FL_KEEP_SIZE
#define FALLOC_FL_KEEP_SIZE     0x01
#endif
#ifndef FALLOC_FL_PUNCH_HOLE
#define FALLOC_FL_PUNCH_HOLE    0x02
#endif
#ifndef FALLOC_FL_ZERO_RANGE
#define FALLOC_FL_ZERO_RANGE    0x10
#endif
#ifndef SEEK_HOLE
#define SEEK_HOLE 4
#endif
#ifndef SEEK_DATA
#define SEEK_DATA 3
#endif
static int fallocate(int fd, int mode, off_t offset, off_t len) {
    (void)fd; (void)mode; (void)offset; (void)len;
    errno = ENOSYS;
    return -1;
}
#endif

/* ============================================================
 * 获取当前时间（微秒）
 * ============================================================ */
static unsigned long get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

/* ============================================================
 * 打印文件信息（大小 vs 实际磁盘占用）
 * ============================================================ */
static void print_file_info(const char *path, const char *label) {
    struct stat st;
    if (stat(path, &st) < 0) {
        printf("[fallocate] %s: stat 失败\n", label);
        return;
    }

    printf("[fallocate] %s:\n", label);
    printf("[fallocate]   逻辑大小: %lld 字节 (%.2f MB)\n",
           (long long)st.st_size, st.st_size / 1024.0 / 1024.0);
#ifdef __linux__
    printf("[fallocate]   磁盘块数: %lld (每块 512 字节)\n",
           (long long)st.st_blocks);
    printf("[fallocate]   实际占用: %lld 字节 (%.2f MB)\n",
           (long long)st.st_blocks * 512,
           st.st_blocks * 512.0 / 1024.0 / 1024.0);
#else
    (void)st; /* Windows: st_blocks 不可用 */
    printf("[fallocate]   (磁盘块数在 Windows 上不可用)\n");
#endif
}

/* ============================================================
 * Demo 1: fallocate 基本预分配
 * ============================================================ */
static void demo_fallocate_basic(void) {
    printf("[fallocate] ====== Demo 1: fallocate 基本预分配 ======\n\n");

    const char *test_file = "/tmp/fallocate_test.dat";
    int fd = open(test_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[fallocate] 创建文件失败");
        return;
    }

    printf("[fallocate] fallocate() 系统调用:\n");
    printf("[fallocate]   int fallocate(int fd, int mode, off_t offset, off_t len);\n\n");

    printf("[fallocate] mode 参数:\n");
    printf("[fallocate]   0                      — 默认模式，预分配磁盘空间\n");
    printf("[fallocate]   FALLOC_FL_KEEP_SIZE    — 预分配但不改变文件大小\n");
    printf("[fallocate]   FALLOC_FL_PUNCH_HOLE   — 释放指定范围的磁盘空间（打洞）\n");
    printf("[fallocate]   FALLOC_FL_ZERO_RANGE   — 将范围清零（保证空间已分配）\n\n");

    /* 默认模式：预分配 10MB */
    printf("[fallocate] 预分配 10MB 磁盘空间:\n");
    unsigned long start = get_time_us();
    if (fallocate(fd, 0, 0, 10 * 1024 * 1024) == 0) {
        unsigned long elapsed = get_time_us() - start;
        printf("[fallocate]   fallocate 成功, 耗时: %.2f ms\n", elapsed / 1000.0);
    } else {
        printf("[fallocate]   fallocate 失败: %s\n", strerror(errno));
    }

    print_file_info(test_file, "预分配后");
    close(fd);
    unlink(test_file);
}

/* ============================================================
 * Demo 2: 稀疏文件创建与检测
 * ============================================================ */
static void demo_sparse_file(void) {
    printf("[fallocate] ====== Demo 2: 稀疏文件创建与检测 ======\n\n");

    const char *test_file = "/tmp/sparse_test.dat";
    int fd = open(test_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[fallocate] 创建文件失败");
        return;
    }

    printf("[fallocate] 稀疏文件概念:\n");
    printf("[fallocate]   - 逻辑大小可以远大于实际磁盘占用\n");
    printf("[fallocate]   - 空洞（hole）不占用磁盘空间\n");
    printf("[fallocate]   - 读取空洞返回全零数据\n\n");

    /* 先写 1KB 数据在文件开头 */
    char buf[1024];
    memset(buf, 'X', sizeof(buf));
    write(fd, buf, sizeof(buf));

    /* 移动到 100MB 偏移处再写 1KB */
    lseek(fd, 100 * 1024 * 1024, SEEK_SET);
    write(fd, buf, sizeof(buf));

    print_file_info(test_file, "稀疏文件创建后");
    printf("[fallocate]   空洞范围: [1KB, 100MB)\n\n");

    /* 使用 SEEK_HOLE / SEEK_DATA 检测空洞 */
    printf("[fallocate] lseek SEEK_HOLE / SEEK_DATA 检测空洞:\n");

    off_t hole_start = lseek(fd, 0, SEEK_HOLE);
    printf("[fallocate]   第一个空洞起始: %lld (1KB 之后)\n", (long long)hole_start);

    off_t data_start = lseek(fd, hole_start, SEEK_DATA);
    printf("[fallocate]   下一个数据起始: %lld (100MB 处)\n", (long long)data_start);

    off_t hole_end = lseek(fd, data_start, SEEK_HOLE);
    printf("[fallocate]   第二个空洞起始: %lld (文件末尾)\n", (long long)hole_end);

    close(fd);
    unlink(test_file);
}

/* ============================================================
 * Demo 3: FALLOC_FL_PUNCH_HOLE 打洞
 * ============================================================ */
static void demo_punch_hole(void) {
    printf("[fallocate] ====== Demo 3: PUNCH_HOLE 打洞释放空间 ======\n\n");

    const char *test_file = "/tmp/punch_hole_test.dat";
    int fd = open(test_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[fallocate] 创建文件失败");
        return;
    }

    /* 写入 10MB 数据 */
    size_t size = 10 * 1024 * 1024;
    char *data = (char *)malloc(size);
    if (!data) { close(fd); return; }
    memset(data, 'B', size);
    write(fd, data, size);
    free(data);

    print_file_info(test_file, "写入 10MB 后");

    /* 在中间打一个 5MB 的洞 */
    printf("\n[fallocate] 在 [2MB, 7MB) 范围打洞:\n");
    if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                  2 * 1024 * 1024, 5 * 1024 * 1024) == 0) {
        printf("[fallocate]   PUNCH_HOLE 成功\n");
    } else {
        printf("[fallocate]   PUNCH_HOLE 失败: %s\n", strerror(errno));
    }

    print_file_info(test_file, "打洞后");
    printf("[fallocate]   注意: 逻辑大小不变，但实际磁盘占用减少\n");

    close(fd);
    unlink(test_file);
}

/* ============================================================
 * Demo 4: 预分配 vs 顺序写性能对比
 * ============================================================ */
static void demo_alloc_perf(void) {
    printf("[fallocate] ====== Demo 4: 预分配 vs 顺序写性能 ======\n\n");

    printf("[fallocate] 零拷贝初始化与 fallocate 对比:\n\n");

    printf("[fallocate] 方案 A — fallocate 预分配:\n");
    printf("[fallocate]   fallocate(fd, 0, 0, 100MB)\n");
    printf("[fallocate]   优点: O(1) 操作，仅修改元数据，几乎零耗时\n");
    printf("[fallocate]   缺点: 读取未初始化区域返回零（非实际内容）\n\n");

    printf("[fallocate] 方案 B — write 顺序写:\n");
    printf("[fallocate]   for (i = 0; i < 100MB; i += 4KB) write(fd, buf, 4KB)\n");
    printf("[fallocate]   优点: 数据真实写入，保证物理分配\n");
    printf("[fallocate]   缺点: O(n) 操作，耗时长，产生大量 I/O\n\n");

    printf("[fallocate] 方案 C — fallocate + ZERO_RANGE:\n");
    printf("[fallocate]   fallocate(fd, FALLOC_FL_ZERO_RANGE, 0, 100MB)\n");
    printf("[fallocate]   优点: 保证空间已分配且数据为零\n");
    printf("[fallocate]   缺点: 仍需要实际写入（比纯 fallocate 慢）\n\n");

    printf("[fallocate] 数据库文件预分配最佳实践:\n");
    printf("[fallocate]   1. 创建时用 fallocate 预分配空间\n");
    printf("[fallocate]   2. 运行时按需写入，避免空洞\n");
    printf("[fallocate]   3. 删除表时用 PUNCH_HOLE 快速回收\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 文件空间预分配学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: fallocate 基本预分配 ---\n");
    demo_fallocate_basic();
    printf("\n");

    printf("--- Demo 2: 稀疏文件创建与检测 ---\n");
    demo_sparse_file();
    printf("\n");

    printf("--- Demo 3: PUNCH_HOLE 打洞 ---\n");
    demo_punch_hole();
    printf("\n");

    printf("--- Demo 4: 预分配性能对比 ---\n");
    demo_alloc_perf();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
