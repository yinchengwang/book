/**
 * @file main.c
 * @brief io_uring 高速存储 I/O 演示
 *
 * 演示 io_uring 批量提交、异步读写、存储优化
 * 展示数据库存储引擎的高性能 I/O 模式
 *
 * 编译：gcc -std=c11 -o io_uring_storage main.c
 * 运行：./io_uring_storage
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

/* 获取微秒时间 */
static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
}

/* 模拟 I/O 请求 */
typedef struct {
    off_t offset;
    size_t size;
    int opcode;        /* 1=READ, 2=WRITE */
    char *buffer;
} IoRequest;

/* 模拟 io_uring 存储引擎 */
typedef struct {
    int fd;
    IoRequest *pending;    /* 待处理的请求 */
    int pending_count;
    int capacity;

    /* 统计 */
    size_t total_read_bytes;
    size_t total_write_bytes;
    int read_count;
    int write_count;
} IoUringStorage;

/* 初始化存储引擎 */
static int storage_init(IoUringStorage *storage, const char *path, size_t file_size) {
    storage->fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (storage->fd < 0) {
        perror("open");
        return -1;
    }

    /* 扩展文件 */
    if (ftruncate(storage->fd, file_size) < 0) {
        perror("ftruncate");
        close(storage->fd);
        return -1;
    }

    storage->capacity = 256;
    storage->pending = calloc(storage->capacity, sizeof(IoRequest));
    storage->pending_count = 0;
    storage->total_read_bytes = 0;
    storage->total_write_bytes = 0;
    storage->read_count = 0;
    storage->write_count = 0;

    return 0;
}

/* 关闭存储引擎 */
static void storage_close(IoUringStorage *storage) {
    if (storage->fd >= 0) {
        close(storage->fd);
    }
    free(storage->pending);
}

/* 提交 I/O 请求（模拟批量提交）*/
static void storage_submit_request(IoUringStorage *storage, int opcode,
                                    off_t offset, size_t size, char *buf) {
    if (storage->pending_count >= storage->capacity) {
        /* 队列满，强制同步执行 */
        storage_flush(storage);
    }

    IoRequest *req = &storage->pending[storage->pending_count++];
    req->opcode = opcode;
    req->offset = offset;
    req->size = size;
    req->buffer = buf;
}

/* 刷新请求队列（模拟 io_uring_submit）*/
static void storage_flush(IoUringStorage *storage) {
    if (storage->pending_count == 0) return;

    /* 模拟批量执行所有请求 */
    for (int i = 0; i < storage->pending_count; i++) {
        IoRequest *req = &storage->pending[i];

        if (req->opcode == 1) {  /* READ */
            pread(storage->fd, req->buffer, req->size, req->offset);
            storage->total_read_bytes += req->size;
            storage->read_count++;
        } else {  /* WRITE */
            pwrite(storage->fd, req->buffer, req->size, req->offset);
            storage->total_write_bytes += req->size;
            storage->write_count++;
        }
    }

    storage->pending_count = 0;
}

/* 批量写入测试 */
static double bench_batch_write(IoUringStorage *storage, size_t total_size,
                                 size_t batch_size) {
    double start = get_time_us();

    char *buf = malloc(batch_size);
    memset(buf, 'A', batch_size);

    off_t offset = 0;
    while (offset < total_size) {
        storage_submit_request(storage, 2, offset, batch_size, buf);
        offset += batch_size;
    }

    /* 批量提交 */
    storage_flush(storage);

    free(buf);
    return get_time_us() - start;
}

/* 批量读取测试 */
static double bench_batch_read(IoUringStorage *storage, size_t total_size,
                                size_t batch_size) {
    double start = get_time_us();

    char *buf = malloc(batch_size);

    off_t offset = 0;
    while (offset < total_size) {
        storage_submit_request(storage, 1, offset, batch_size, buf);
        offset += batch_size;
    }

    /* 批量提交 */
    storage_flush(storage);

    free(buf);
    return get_time_us() - start;
}

/* 打印统计 */
static void print_stats(const IoUringStorage *storage, double elapsed_us) {
    printf("\n=== I/O 统计 ===\n");
    printf("读取次数: %d, 总字节: %zu (%.2f MB)\n",
           storage->read_count, storage->total_read_bytes,
           storage->total_read_bytes / 1024.0 / 1024.0);
    printf("写入次数: %d, 总字节: %zu (%.2f MB)\n",
           storage->write_count, storage->total_write_bytes,
           storage->total_write_bytes / 1024.0 / 1024.0);
    printf("总耗时: %.2f ms\n", elapsed_us / 1000.0);
    printf("吞吐: %.2f MB/s\n",
           (storage->total_read_bytes + storage->total_write_bytes)
           / 1024.0 / 1024.0 / (elapsed_us / 1e6));
}

/* 创建测试文件 */
static int create_test_file(const char *path, size_t size) {
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;

    char *buf = malloc(1024 * 1024);
    memset(buf, 'X', 1024 * 1024);

    size_t written = 0;
    while (written < size) {
        size_t chunk = (size - written > 1024 * 1024) ? 1024 * 1024 : size - written;
        write(fd, buf, chunk);
        written += chunk;
    }

    free(buf);
    close(fd);
    return 0;
}

int main(void) {
    printf("===========================================\n");
    printf("  io_uring 高速存储 I/O 演示\n");
    printf("===========================================\n");

    const char *test_file = "/tmp/io_uring_storage_test.dat";
    const size_t file_size = 100 * 1024 * 1024;  /* 100MB */
    const size_t batch_size = 4096;              /* 4KB 页面 */

    /* 1. 创建测试文件 */
    printf("\n=== 1. 创建测试文件 ===\n");
    printf("创建 %zu MB 测试文件...\n", file_size / 1024 / 1024);
    if (create_test_file(test_file, file_size) < 0) {
        perror("create_test_file");
        return 1;
    }
    printf("测试文件创建完成\n");

    /* 2. 初始化存储引擎 */
    printf("\n\n=== 2. 初始化 io_uring 存储引擎 ===\n");
    IoUringStorage storage;
    if (storage_init(&storage, test_file, file_size) < 0) {
        return 1;
    }
    printf("io_uring 存储引擎就绪，批量队列深度: %d\n", storage.capacity);

    /* 3. 批量写入测试 */
    printf("\n=== 3. 批量写入测试 ===\n");
    printf("文件大小: %zu MB, 批量大小: %zu 字节\n",
           file_size / 1024 / 1024, batch_size);
    double write_time = bench_batch_write(&storage, file_size, batch_size);
    printf("批量写入完成\n");

    /* 4. 批量读取测试 */
    printf("\n=== 4. 批量读取测试 ===\n");
    double read_time = bench_batch_read(&storage, file_size, batch_size);
    printf("批量读取完成\n");

    /* 5. 打印统计 */
    print_stats(&storage, write_time + read_time);

    /* 6. io_uring 存储优化要点 */
    printf("\n\n=== 6. io_uring 存储优化要点 ===\n");
    printf(
        "  1. 批量提交: 合并多个小 I/O 为一次系统调用\n"
        "  2. 固定缓冲区: 预注册缓冲区避免每次注册开销\n"
        "  3. 轮询模式: IORING_SETUP_IOPOLL 绕过调度器延迟\n"
        "  4. 多文件: SQ 轮询分配给不同文件描述符\n"
        "  5. 链接操作: IORING_OP_LINK 保证原子性\n"
    );

    /* 7. 数据库存储场景 */
    printf("\n\n=== 7. 数据库存储场景 ===\n");
    printf(
        "  1. WAL 顺序写入: 批量提交 + IORING_OP_FSYNC\n"
        "  2. 检查点刷盘: 多页面并行写入\n"
        "  3. 页面预读: 异步提交未来页面读取请求\n"
        "  4. 索引构建: 大文件顺序写入用 io_uring 加速\n"
        "  5. 备份恢复: 流式大文件 I/O\n"
    );

    /* 8. 性能对比 */
    printf("\n\n=== 8. 性能对比（理论）===\n");
    printf(
        "  方案        | 单次 I/O 延迟 | 10万次吞吐\n"
        "  ------------|--------------|-------------\n"
        "  同步 read   | ~10-50us     | 100-200 MB/s\n"
        "  libaio      | ~5-20us      | 200-400 MB/s\n"
        "  io_uring    | ~1-5us       | 500-1000 MB/s\n"
        "  io_uring+IO | ~0.5-2us     | 1000-2000 MB/s\n"
    );

    /* 清理 */
    storage_close(&storage);
    unlink(test_file);

    printf("\n===========================================\n");
    printf("  io_uring 存储演示完成\n");
    printf("===========================================\n");

    return 0;
}
