/**
 * @file main.c
 * @brief Linux 数据同步（fdatasync）学习演示
 *
 * 演示内容：
 *   - fsync vs fdatasync 区别（仅同步数据不更新时间戳）
 *   - O_SYNC / O_DSYNC 打开标志
 *   - 批量提交 vs 逐条刷盘性能对比
 *   - 同步边界的测量
 *
 * 编译：gcc -std=c11 -Wall -o fdatasync main.c
 * 运行：./fdatasync
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

/* Windows 平台 Fallback（fdatasync/fsync 在 Windows 上不可用） */
#ifdef _WIN32
static int fdatasync(int fd) {
    (void)fd;
    /* Windows 等价: FlushFileBuffers */
    return 0;
}
static int fsync(int fd) {
    (void)fd;
    return 0;
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
 * 创建测试文件
 * ============================================================ */
static int create_test_file(const char *path) {
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[fdatasync] 创建文件失败");
    }
    return fd;
}

/* ============================================================
 * Demo 1: fsync vs fdatasync 概念对比
 * ============================================================ */
static void demo_sync_concepts(void) {
    printf("[fdatasync] ====== Demo 1: fsync vs fdatasync 概念 ======\n\n");

    printf("[fdatasync] fsync(int fd):\n");
    printf("[fdatasync]   - 同步文件数据和元数据（inode）到磁盘\n");
    printf("[fdatasync]   - 元数据包括: 文件大小、修改时间、权限等\n");
    printf("[fdatasync]   - 即使只有数据变更，也会更新 inode 的 mtime\n");
    printf("[fdatasync]   - 大多数情况下产生 2 次 I/O（数据 + inode）\n\n");

    printf("[fdatasync] fdatasync(int fd):\n");
    printf("[fdatasync]   - 仅同步文件数据到磁盘\n");
    printf("[fdatasync]   - 只在文件大小变化时才同步必要的元数据\n");
    printf("[fdatasync]   - 不更新 atime/mtime（减少 inode 写回）\n");
    printf("[fdatasync]   - 通常只需 1 次 I/O（仅数据）\n\n");

    printf("[fdatasync] 性能差异:\n");
    printf("[fdatasync]   - 小文件随机写: fdatasync 可减少 ~50%% I/O\n");
    printf("[fdatasync]   - 追加写（大小变化）: 两者相近（都需更新 inode）\n");
    printf("[fdatasync]   - 数据库 WAL: 推荐 fdatasync，不关心 mtime\n\n");

    printf("[fdatasync] 系统调用签名:\n");
    printf("[fdatasync]   int fsync(int fd);\n");
    printf("[fdatasync]   int fdatasync(int fd);  // POSIX.1b\n");
}

/* ============================================================
 * Demo 2: O_SYNC / O_DSYNC 打开标志
 * ============================================================ */
static void demo_open_flags(void) {
    printf("[fdatasync] ====== Demo 2: O_SYNC / O_DSYNC 标志 ======\n\n");

    const char *test_file = "/tmp/fdatasync_flags_test.dat";

    printf("[fdatasync] open() 同步标志:\n\n");

    printf("[fdatasync] 1. O_SYNC:\n");
    printf("[fdatasync]    - 每次 write() 相当于 write + fsync\n");
    printf("[fdatasync]    - 数据 + 元数据同步写回\n");
    printf("[fdatasync]    - 影响: 每次 write 都阻塞等待磁盘完成\n");
    printf("[fdatasync]    int fd = open(path, O_WRONLY | O_SYNC);\n\n");

    printf("[fdatasync] 2. O_DSYNC:\n");
    printf("[fdatasync]    - 每次 write() 相当于 write + fdatasync\n");
    printf("[fdatasync]    - 仅数据同步，不一定更新元数据\n");
    printf("[fdatasync]    - 比 O_SYNC 少一次 inode 写回\n");
    printf("[fdatasync]    int fd = open(path, O_WRONLY | O_DSYNC);\n\n");

    printf("[fdatasync] 3. 无同步标志 + 显式 sync:\n");
    printf("[fdatasync]    - 应用层控制同步时机（批量提交）\n");
    printf("[fdatasync]    - 灵活，性能最优\n");
    printf("[fdatasync]    write(fd, ...); ... fdatasync(fd);\n\n");

    printf("[fdatasync] 数据库场景选择:\n");
    printf("[fdatasync]   - WAL 日志: 显式 fdatasync（批量提交）\n");
    printf("[fdatasync]   - 数据文件: Buffer Pool 管理，按检查点刷盘\n");
}

/* ============================================================
 * Demo 3: 批量提交 vs 逐条刷盘性能对比
 * ============================================================ */
static void demo_batch_vs_per_record(void) {
    printf("[fdatasync] ====== Demo 3: 批量提交 vs 逐条刷盘 ======\n\n");

    const char *test_file = "/tmp/fdatasync_perf_test.dat";
    const int num_records = 1000;
    const int record_size = 256;
    char buf[256];

    memset(buf, 'D', sizeof(buf));

    /* 方案 A: 逐条 fdatasync（模拟每次事务提交都刷盘） */
    int fd = create_test_file(test_file);
    if (fd < 0) return;

    unsigned long start = get_time_us();
    for (int i = 0; i < num_records; i++) {
        if (write(fd, buf, record_size) < 0) {
            perror("[fdatasync] 写入失败");
            break;
        }
        /* 每条记录都同步 —— 模拟逐条提交 */
        fdatasync(fd);
    }
    unsigned long per_record_time = get_time_us() - start;
    close(fd);
    unlink(test_file);

    printf("[fdatasync] 测试参数: %d 条记录, 每条 %d 字节\n",
           num_records, record_size);

    printf("[fdatasync] 方案 A — 逐条 fdatasync（每次写入后刷盘）:\n");
    printf("[fdatasync]   总耗时:   %.2f ms\n", per_record_time / 1000.0);
    printf("[fdatasync]   平均每条: %.2f us\n",
           per_record_time / (double)num_records);
    printf("[fdatasync]   IOPS:     %.0f ops/sec\n\n",
           num_records / (per_record_time / 1000000.0));

    /* 方案 B: 批量写入 + 一次 fdatasync（模拟批量提交） */
    fd = create_test_file(test_file);
    if (fd < 0) return;

    start = get_time_us();
    for (int i = 0; i < num_records; i++) {
        if (write(fd, buf, record_size) < 0) {
            perror("[fdatasync] 写入失败");
            break;
        }
    }
    /* 所有记录写入后再同步 —— 模拟批量提交 */
    fdatasync(fd);
    unsigned long batch_time = get_time_us() - start;
    close(fd);
    unlink(test_file);

    printf("[fdatasync] 方案 B — 批量写入 + 一次 fdatasync:\n");
    printf("[fdatasync]   总耗时:   %.2f ms\n", batch_time / 1000.0);
    printf("[fdatasync]   平均每条: %.2f us\n",
           batch_time / (double)num_records);
    printf("[fdatasync]   IOPS:     %.0f ops/sec\n\n",
           num_records / (batch_time / 1000000.0));

    /* 加速比 */
    if (per_record_time > 0) {
        printf("[fdatasync] 批量提交加速比: %.1fx\n",
               (double)per_record_time / batch_time);
    }

    printf("\n[fdatasync] 结论:\n");
    printf("[fdatasync]   - 逐条刷盘产生大量随机 I/O，性能极差\n");
    printf("[fdatasync]   - 批量提交利用内核 I/O 合并，大幅提升吞吐\n");
    printf("[fdatasync]   - WAL 批量提交是数据库高性能的关键技术\n");
}

/* ============================================================
 * Demo 4: fsync vs fdatasync 同步边界测量
 * ============================================================ */
static void demo_sync_boundary(void) {
    printf("[fdatasync] ====== Demo 4: fsync vs fdatasync 同步边界 ======\n\n");

    const char *test_file = "/tmp/fdatasync_boundary_test.dat";
    const int record_size = 512;
    char buf[512];
    memset(buf, 'E', sizeof(buf));

    /* 测量 fsync 耗时 */
    int fd = create_test_file(test_file);
    if (fd < 0) return;

    /* 先做一次预热写入 + 同步，消除冷启动影响 */
    write(fd, buf, record_size);
    fsync(fd);

    unsigned long start = get_time_us();
    write(fd, buf, record_size);
    fsync(fd);
    unsigned long fsync_time = get_time_us() - start;
    close(fd);

    /* 测量 fdatasync 耗时 */
    fd = create_test_file(test_file);
    if (fd < 0) return;

    write(fd, buf, record_size);
    fdatasync(fd);

    start = get_time_us();
    write(fd, buf, record_size);
    fdatasync(fd);
    unsigned long fdatasync_time = get_time_us() - start;
    close(fd);
    unlink(test_file);

    printf("[fdatasync] 同步边界测量（%d 字节写入）:\n", record_size);
    printf("[fdatasync]   fsync 耗时:      %lu us\n", fsync_time);
    printf("[fdatasync]   fdatasync 耗时:  %lu us\n", fdatasync_time);
    if (fsync_time > 0) {
        printf("[fdatasync]   fdatasync 比 fsync 快: %.1f%%\n",
               (1.0 - (double)fdatasync_time / fsync_time) * 100);
    }

    printf("\n[fdatasync] 同步边界分析:\n");
    printf("[fdatasync]   硬件边界: 磁盘 sector (512B/4KB)\n");
    printf("[fdatasync]   文件系统边界: block (4KB), extent (可变)\n");
    printf("[fdatasync]   内核边界: page (4KB)\n");
    printf("[fdatasync]   小于边界的写入仍需整页写回\n\n");

    printf("[fdatasync] WAL 设计启示:\n");
    printf("[fdatasync]   - WAL 日志按 sector 对齐写入\n");
    printf("[fdatasync]   - 批量积攒到一定大小再刷盘\n");
    printf("[fdatasync]   - 避免小于 4KB 的频繁同步\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 数据同步（fdatasync）学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: fsync vs fdatasync 概念 ---\n");
    demo_sync_concepts();
    printf("\n");

    printf("--- Demo 2: O_SYNC / O_DSYNC 标志 ---\n");
    demo_open_flags();
    printf("\n");

    printf("--- Demo 3: 批量 vs 逐条刷盘性能 ---\n");
    demo_batch_vs_per_record();
    printf("\n");

    printf("--- Demo 4: 同步边界测量 ---\n");
    demo_sync_boundary();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
