/**
 * @file main.c
 * @brief io_uring 异步 I/O 演示
 *
 * 演示 io_uring_setup/submit、异步 I/O 模式
 * 展示 io_uring 相比传统 I/O 的优势
 *
 * 编译：gcc -std=c11 -o iouring main.c
 * 运行：./iouring
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/uio.h>

/* 获取微秒时间 */
static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
}

/* 演示 1：传统同步 I/O */
static void demo_sync_io(const char *path, size_t total_bytes) {
    printf("\n=== 方案 1: 传统同步 I/O ===\n");

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return; }

    char *buf = malloc(total_bytes);
    memset(buf, 'A', total_bytes);

    double start = get_time_us();

    /* 同步写入 */
    ssize_t written = write(fd, buf, total_bytes);
    if (written > 0) fsync(fd);

    /* 同步读取 */
    lseek(fd, 0, SEEK_SET);
    ssize_t nread = read(fd, buf, total_bytes);

    double elapsed = get_time_us() - start;

    printf("写入 %zd 字节, 读取 %zd 字节\n", written, nread);
    printf("总耗时: %.2f ms\n", elapsed / 1000.0);
    printf("吞吐: %.2f MB/s\n",
           (total_bytes * 2.0 / 1024.0 / 1024.0) / (elapsed / 1e6));

    free(buf);
    close(fd);
}

/* 模拟 io_uring 结构（仅演示概念，非真实实现）*/
typedef struct {
    int fd;
    void *buffer;
    size_t size;
    off_t offset;
    int opcode;          /* IORING_OP_READ / IORING_OP_WRITE */
    int complete;
} IoUringSQE;

typedef struct {
    int result;
    void *user_data;
} IoUringCQE;

typedef struct {
    IoUringSQE *sq;      /* 提交队列 */
    int sq_head;
    int sq_tail;
    int sq_entries;

    IoUringCQE *cq;      /* 完成队列 */
    int cq_head;
    int cq_tail;
    int cq_entries;
} IoUring;

/* 模拟 io_uring_setup() */
static int sim_io_uring_setup(int entries, IoUring *ring) {
    ring->sq = calloc(entries, sizeof(IoUringSQE));
    ring->cq = calloc(entries, sizeof(IoUringCQE));
    ring->sq_entries = entries;
    ring->cq_entries = entries;
    ring->sq_head = 0;
    ring->sq_tail = 0;
    ring->cq_head = 0;
    ring->cq_tail = 0;
    return 0;
}

/* 获取一个 SQE（提交队列项）*/
static IoUringSQE *io_uring_get_sqe(IoUring *ring) {
    if (ring->sq_tail - ring->sq_head >= ring->sq_entries)
        return NULL;  /* 队列满 */
    return &ring->sq[ring->sq_tail++ % ring->sq_entries];
}

/* 提交 SQE */
static int io_uring_submit(IoUring *ring) {
    int submitted = 0;

    while (ring->sq_head < ring->sq_tail) {
        IoUringSQE *sqe = &ring->sq[ring->sq_head % ring->sq_entries];

        /* 模拟执行 I/O 操作 */
        if (sqe->opcode == 1) {  /* WRITE */
            pwrite(sqe->fd, sqe->buffer, sqe->size, sqe->offset);
        } else if (sqe->opcode == 2) {  /* READ */
            pread(sqe->fd, sqe->buffer, sqe->size, sqe->offset);
        }

        /* 入完成队列 */
        IoUringCQE *cqe = &ring->cq[ring->cq_tail % ring->cq_entries];
        cqe->result = (int)sqe->size;
        cqe->user_data = sqe;
        ring->cq_tail++;

        ring->sq_head++;
        submitted++;
    }

    return submitted;
}

/* 从完成队列获取 */
static IoUringCQE *io_uring_wait_cqe(IoUring *ring) {
    if (ring->cq_head >= ring->cq_tail)
        return NULL;  /* 无完成事件 */

    IoUringCQE *cqe = &ring->cq[ring->cq_head % ring->cq_entries];
    ring->cq_head++;
    return cqe;
}

/* 演示 2：io_uring 批量异步 I/O */
static void demo_iouring_batch(const char *path, size_t total_bytes) {
    printf("\n=== 方案 2: io_uring 批量异步 I/O ===\n");

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return; }

    char *buf = malloc(total_bytes);
    memset(buf, 'B', total_bytes);

    IoUring ring;
    if (sim_io_uring_setup(256, &ring) < 0) {
        free(buf);
        close(fd);
        return;
    }

    double start = get_time_us();

    /* 批量提交多个 I/O 请求 */
    const size_t chunk_size = 4096;
    const int num_chunks = (int)(total_bytes / chunk_size);

    for (int i = 0; i < num_chunks; i++) {
        IoUringSQE *sqe = io_uring_get_sqe(&ring);
        if (!sqe) break;

        sqe->fd = fd;
        sqe->buffer = buf + i * chunk_size;
        sqe->size = chunk_size;
        sqe->offset = i * chunk_size;
        sqe->opcode = 1;  /* WRITE */
    }

    /* 一次性提交所有请求 */
    printf("提交 %d 个 I/O 请求...\n",
           (int)(ring.sq_tail - ring.sq_head));
    int submitted = io_uring_submit(&ring);
    printf("提交: %d\n", submitted);

    /* 等待完成 */
    int completed = 0;
    while (completed < submitted) {
        IoUringCQE *cqe = io_uring_wait_cqe(&ring);
        if (cqe) completed++;
    }

    double elapsed = get_time_us() - start;

    printf("完成 %d 个请求\n", completed);
    printf("总耗时: %.2f ms\n", elapsed / 1000.0);
    printf("吞吐: %.2f MB/s\n",
           (total_bytes / 1024.0 / 1024.0) / (elapsed / 1e6));
    printf("每请求延迟: %.2f us\n",
           elapsed / completed);

    free(buf);
    close(fd);
    free(ring.sq);
    free(ring.cq);
}

int main(void) {
    printf("===========================================\n");
    printf("  io_uring 异步 I/O 演示\n");
    printf("===========================================\n");

    const char *test_file = "/tmp/iouring_test.dat";
    const size_t test_size = 10 * 1024 * 1024;  /* 10MB */

    /* 1. io_uring 概述 */
    printf("\n=== 1. io_uring 概述 ===\n");
    printf(
        "io_uring 是 Linux 5.1+ 引入的高性能异步 I/O 接口，\n"
        "相比传统的 AIO/libaio 具有:\n"
        "  - 共享内存队列（SQ/CQ），减少系统调用\n"
        "  - 批量提交和完成，减少上下文切换\n"
        "  - 支持多种操作：read/write/fsync/socket/accept\n"
        "  - 轮询模式（IORING_SETUP_IOPOLL）低延迟\n\n"
        "核心组件:\n"
        "  SQE (Submission Queue Entry): 待提交的 I/O 请求\n"
        "  CQE (Completion Queue Entry): 已完成的 I/O 结果\n"
        "  SQ (Submission Queue): 环形提交队列\n"
        "  CQ (Completion Queue): 环形完成队列\n"
    );

    /* 2. io_uring 工作流程 */
    printf("\n=== 2. io_uring 工作流程 ===\n");
    printf(
        "\n  用户空间                内核空间\n"
        "  ┌────────┐          ┌──────────┐\n"
        "  │ SQ 队列 │ ──提交──→ │ I/O 引擎 │\n"
        "  └────────┘          │ (异步执行)│\n"
        "                      └──────────┘\n"
        "  ┌────────┐  ←完成──            \n"
        "  │ CQ 队列 │                      \n"
        "  └────────┘                      \n\n"
        "步骤:\n"
        "  1. io_uring_queue_init() — 初始化\n"
        "  2. io_uring_get_sqe()    — 获取 SQE\n"
        "  3. io_uring_prep_xxx()   — 准备操作\n"
        "  4. io_uring_submit()     — 提交请求\n"
        "  5. io_uring_wait_cqe()   — 等待完成\n"
    );

    /* 3. 对比演示 */
    printf("\n\n=== 3. I/O 方案对比 ===\n");

    demo_sync_io(test_file, test_size);
    demo_iouring_batch(test_file, test_size);

    /* 4. io_uring 操作类型 */
    printf("\n\n=== 4. io_uring 支持的操作 ===\n");
    printf(
        "  IORING_OP_READ           : 文件读\n"
        "  IORING_OP_WRITE          : 文件写\n"
        "  IORING_OP_FSYNC          : 文件同步\n"
        "  IORING_OP_READV/WRITEV   : 矢量 I/O\n"
        "  IORING_OP_SEND/RECV      : 网络收发\n"
        "  IORING_OP_ACCEPT         : 接受连接\n"
        "  IORING_OP_CONNECT        : 发起连接\n"
        "  IORING_OP_TIMEOUT        : 超时控制\n"
        "  IORING_OP_LINK_TIMEOUT   : 链接超时\n"
        "  IORING_OP_FILES_UPDATE   : 文件描述符传递\n"
    );

    /* 5. 数据库场景 */
    printf("\n\n=== 5. 数据库 io_uring 应用场景 ===\n");
    printf(
        "  1. WAL 批量刷盘: 多个小写入批量提交\n"
        "  2. 检查点并行刷盘: 多个脏页同时写入\n"
        "  3. 预读优化: 异步提交未来页读取请求\n"
        "  4. 索引构建: 大文件读写用 io_uring 加速\n"
        "  5. 备份/恢复: 大数据量流式 I/O\n"
    );

    /* 6. 内核版本要求 */
    printf("\n\n=== 6. 内核支持 ===\n");
    printf("  Linux 5.1+ : 基本 io_uring\n");
    printf("  Linux 5.6+ : 网络操作支持\n");
    printf("  Linux 5.12+: 多 shot 模式\n");
    printf("  Linux 5.15+: ring mapped buffer\n");
    printf("  Linux 6.0+ : multi-user 支持\n");

    /* 清理 */
    unlink(test_file);

    printf("\n===========================================\n");
    printf("  io_uring 演示完成\n");
    printf("===========================================\n");

    return 0;
}
