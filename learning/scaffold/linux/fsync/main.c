/**
 * @file main.c
 * @brief Linux fsync/fdatasync 磁盘同步机制演示
 *
 * 演示内容：
 *   - fsync vs fdatasync 的区别
 *   - 脏页强制回写流程
 *   - WAL / redo log 的同步策略
 *
 * 编译：gcc -std=c11 -Wall -o fsync main.c
 * 运行：./fsync
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>

/**
 * @brief 微妙级时间差计算
 */
static long time_diff_us(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000000L +
           (end->tv_usec - start->tv_usec);
}

/* ============================================================
 * Demo 1: fsync 原理讲解
 * ============================================================ */
static void demo_fsync_principles(void) {
    printf("[fsync] fsync/fdatasync 机制原理:\n\n");

    printf("[fsync]   用户空间:                  磁盘:\n");
    printf("[fsync]   write() ──► Page Cache ──► 脏页 ──► pdflush/flusher 线程\n");
    printf("[fsync]     │                                     │\n");
    printf("[fsync]     └─ 返回（数据仍在内存）                └─ 时机：脏页过期/内存不足\n");
    printf("[fsync]                                             │\n");
    printf("[fsync]   fsync() ──────────────────────────────────┘\n");
    printf("[fsync]     │              强制：Page Cache → 磁盘\n");
    printf("[fsync]     └─ 返回（数据已落盘）\n\n");

    printf("[fsync] fsync vs fdatasync:\n");
    printf("[fsync]   fsync(): 数据 + 元数据（inode 大小/mtime）\n");
    printf("[fsync]   fdatasync(): 仅数据（除非元数据影响读取）\n");
}

/* ============================================================
 * Demo 2: 性能对比（fsync vs fdatasync vs 无同步）
 * ============================================================ */
static void demo_perf_comparison(const char *test_file) {
    printf("[fsync] 性能对比: write vs write+fsync vs write+fdatasync\n");

    const int N = 100;                  /* 操作次数 */
    const size_t BUF_SIZE = 4096;       /* 4KB */
    char *buf = calloc(1, BUF_SIZE);
    struct timeval t_start, t_end;

    /* 测试 1: 仅 write */
    int fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    gettimeofday(&t_start, NULL);
    for (int i = 0; i < N; i++) {
        write(fd, buf, BUF_SIZE);
    }
    gettimeofday(&t_end, NULL);
    long t_write = time_diff_us(&t_start, &t_end);
    printf("[fsync]   仅 write(%d x 4KB): %ld us (%.1f us/op)\n",
           N, t_write, (double)t_write / N);
    close(fd);
    unlink(test_file);

    /* 测试 2: write + fsync */
    fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    gettimeofday(&t_start, NULL);
    for (int i = 0; i < N; i++) {
        write(fd, buf, BUF_SIZE);
        fsync(fd);
    }
    gettimeofday(&t_end, NULL);
    long t_fsync = time_diff_us(&t_start, &t_end);
    printf("[fsync]   write+fsync(%d x 4KB): %ld us (%.1f us/op)\n",
           N, t_fsync, (double)t_fsync / N);
    close(fd);
    unlink(test_file);

    /* 测试 3: write + fdatasync */
    fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    gettimeofday(&t_start, NULL);
    for (int i = 0; i < N; i++) {
        write(fd, buf, BUF_SIZE);
        fdatasync(fd);
    }
    gettimeofday(&t_end, NULL);
    long t_fdatasync = time_diff_us(&t_start, &t_end);
    printf("[fsync]   write+fdatasync(%d x 4KB): %ld us (%.1f us/op)\n",
           N, t_fdatasync, (double)t_fdatasync / N);
    close(fd);
    unlink(test_file);

    printf("[fsync]   fsync 开销: %.1fx (vs 仅 write)\n",
           (double)t_fsync / (t_write > 0 ? t_write : 1));
    printf("[fsync]   fdatasync 节省: %.0f%% vs fsync\n",
           100.0 * (t_fsync - t_fdatasync) / (t_fsync > 0 ? t_fsync : 1));

    free(buf);
}

/* ============================================================
 * Demo 3: 文件同步语义
 * ============================================================ */
static void demo_sync_semantics(const char *test_file) {
    printf("[fsync] 文件同步语义演示\n");

    int fd = open(test_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open 失败");
        return;
    }

    /* 写入数据 */
    const char *data = "Hello, fsync! This data MUST be on disk.";
    write(fd, data, strlen(data));
    printf("[fsync]   写入 %zu 字节\n", strlen(data));

    /* 检查文件系统状态（在 fsync 前可能在 Page Cache） */
    struct stat st;
    fstat(fd, &st);
    printf("[fsync]   st_size after write: %lld\n", (long long)st.st_size);

    /* fsync */
    if (fsync(fd) == 0) {
        printf("[fsync]   fsync 成功 — 数据和元数据已落盘\n");
    }

    fstat(fd, &st);
    printf("[fsync]   st_size after fsync:  %lld\n", (long long)st.st_size);

    /* 测试 O_SYNC */
    printf("[fsync]   对比 O_SYNC: 每次 write 自动同步（不需调用 fsync）\n");
    printf("[fsync]   代价: 每个 write 都是同步操作，性能最低\n");

    close(fd);
    unlink(test_file);
}

/* ============================================================
 * Demo 4: WAL 同步策略
 * ============================================================ */
static void demo_wal_sync_strategy(void) {
    printf("[fsync] WAL 同步策略:\n\n");

    printf("[fsync] 1. 组提交 (Group Commit):\n");
    printf("[fsync]    - 多个事务共享一次 fsync\n");
    printf("[fsync]    - 优点: 减少 fsync 次数\n");
    printf("[fsync]    - 实现: WAL Writer 批量刷盘\n\n");

    printf("[fsync] 2. 异步提交:\n");
    printf("[fsync]    - synchronous_commit = off/remote_write/local\n");
    printf("[fsync]    - 某些事务可容忍丢失最后几条 WAL\n\n");

    printf("[fsync] 3. WAL Segment 大小:\n");
    printf("[fsync]    - PostgreSQL: 16MB / segment\n");
    printf("[fsync]    - 避免过于频繁的 fsync\n\n");

    printf("[fsync] 4. fsync 频率控制:\n");
    printf("[fsync]    - wal_writer_delay (PG: 200ms)\n");
    printf("[fsync]    - wal_writer_flush_after (PG: 1MB)\n");
    printf("[fsync]    - 含大量小写时该设频繁写入以权衡\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    const char *test_file = "/tmp/fsync_test.bin";

    printf("========================================\n");
    printf("  Linux fsync 磁盘同步机制演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: fsync 原理 ---\n");
    demo_fsync_principles();
    printf("\n");

    printf("--- Demo 2: 性能对比 ---\n");
    demo_perf_comparison(test_file);
    printf("\n");

    printf("--- Demo 3: 同步语义 ---\n");
    demo_sync_semantics(test_file);
    printf("\n");

    printf("--- Demo 4: WAL 同步策略 ---\n");
    demo_wal_sync_strategy();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
