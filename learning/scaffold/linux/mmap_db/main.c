/**
 * @file main.c
 * @brief mmap 内存映射数据库演示
 *
 * 演示 mmap 文件映射、随机访问、预读策略
 * 展示 MMAP 存储引擎的核心设计
 *
 * 编译：gcc -std=c11 -o mmap_db main.c
 * 运行：./mmap_db
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

/* 页面大小（通常是 4KB）*/
#define PAGE_SIZE 4096

/* 获取微秒时间 */
static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
}

/* 模拟简单的 MMAP 存储引擎 */
typedef struct {
    int fd;                  /* 文件描述符 */
    size_t file_size;        /* 文件大小 */
    void *mapped_addr;       /* 映射地址 */
    size_t mapped_size;      /* 映射大小 */
    int is_owner;            /* 是否拥有映射 */
} MmapStorage;

/* 初始化 mmap 存储 */
static int mmap_storage_init(MmapStorage *storage, const char *path, size_t size) {
    /* 打开或创建文件 */
    storage->fd = open(path, O_RDWR | O_CREAT, 0644);
    if (storage->fd < 0) {
        perror("open");
        return -1;
    }

    /* 扩展文件大小 */
    if (ftruncate(storage->fd, size) < 0) {
        perror("ftruncate");
        close(storage->fd);
        return -1;
    }

    /* 执行 mmap */
    storage->mapped_addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, storage->fd, 0);
    if (storage->mapped_addr == MAP_FAILED) {
        perror("mmap");
        close(storage->fd);
        return -1;
    }

    storage->mapped_size = size;
    storage->file_size = size;
    storage->is_owner = 1;

    printf("[MMAP] 映射 %zu 字节到 %p\n", size, storage->mapped_addr);
    return 0;
}

/* 关闭 mmap 存储 */
static void mmap_storage_close(MmapStorage *storage) {
    if (storage->mapped_addr && storage->mapped_addr != MAP_FAILED) {
        if (munmap(storage->mapped_addr, storage->mapped_size) < 0) {
            perror("munmap");
        }
        printf("[MMAP] 解除映射\n");
    }
    if (storage->fd >= 0) {
        close(storage->fd);
    }
}

/* mmap 存储：写入记录 */
static int mmap_storage_write(MmapStorage *storage, off_t offset,
                               const void *data, size_t size) {
    if (offset + size > storage->mapped_size) {
        fprintf(stderr, "[MMAP] 写入越界\n");
        return -1;
    }

    /* 直接写入映射内存，无需系统调用 */
    memcpy((char *)storage->mapped_addr + offset, data, size);
    return 0;
}

/* mmap 存储：读取记录 */
static int mmap_storage_read(MmapStorage *storage, off_t offset,
                              void *data, size_t size) {
    if (offset + size > storage->mapped_size) {
        fprintf(stderr, "[MMAP] 读取越界\n");
        return -1;
    }

    /* 直接从映射内存读取，无需系统调用 */
    memcpy(data, (char *)storage->mapped_addr + offset, size);
    return 0;
}

/* 顺序写入基准测试 */
static double bench_sequential_write(MmapStorage *storage, size_t total_size) {
    double start = get_time_us();

    const int block_size = 256;
    char buf[block_size];
    memset(buf, 'X', block_size);

    for (size_t offset = 0; offset < total_size; offset += block_size) {
        mmap_storage_write(storage, offset, buf, block_size);
    }

    /* 强制同步到磁盘 */
    if (msync(storage->mapped_addr, total_size, MS_SYNC) < 0) {
        perror("msync");
    }

    return get_time_us() - start;
}

/* 顺序读取基准测试 */
static double bench_sequential_read(MmapStorage *storage, size_t total_size) {
    double start = get_time_us();

    const int block_size = 256;
    char buf[block_size];

    for (size_t offset = 0; offset < total_size; offset += block_size) {
        mmap_storage_read(storage, offset, buf, block_size);
    }

    return get_time_us() - start;
}

/* 随机读取基准测试 */
static double bench_random_read(MmapStorage *storage, size_t total_size,
                                 int num_ops) {
    double start = get_time_us();

    const int block_size = 256;
    char buf[block_size];
    unsigned int seed = 42;

    for (int i = 0; i < num_ops; i++) {
        off_t offset = (rand_r(&seed) % (total_size / block_size)) * block_size;
        mmap_storage_read(storage, offset, buf, block_size);
    }

    return get_time_us() - start;
}

int main(void) {
    printf("===========================================\n");
    printf("  mmap 内存映射数据库演示\n");
    printf("===========================================\n");

    const char *db_file = "/tmp/mmap_db_test.dat";
    const size_t db_size = 10 * 1024 * 1024;  /* 10MB */

    /* 1. mmap 概述 */
    printf("\n=== 1. mmap 概述 ===\n");
    printf(
        "mmap 将文件映射到进程的虚拟地址空间，\n"
        "读写映射区域就像操作内存一样，无需 read()/write()。\n\n"
        "优势:\n"
        "  - 零拷贝：直接操作映射内存，避免内核/用户态拷贝\n"
        "  - 编程简单：像操作内存一样读写文件\n"
        "  - 按需加载：操作系统自动管理页面换入换出\n"
        "  - 共享映射：多个进程可共享同一映射\n\n"
        "劣势:\n"
        "  - 页面错误开销：首次访问触发缺页中断\n"
        "  - 存储引擎复杂度：需要处理 WAL 和崩溃一致性\n"
        "  - 文件大小限制：受虚拟地址空间限制\n"
    );

    /* 2. 初始化 mmap 存储 */
    printf("\n\n=== 2. 初始化 mmap 存储 ===\n");
    MmapStorage storage;
    if (mmap_storage_init(&storage, db_file, db_size) < 0) {
        return 1;
    }

    /* 3. 顺序写入测试 */
    printf("\n=== 3. 顺序写入基准测试 ===\n");
    double write_time = bench_sequential_write(&storage, db_size);
    printf("写入 %zu 字节耗时: %.2f ms\n", db_size, write_time / 1000.0);
    printf("写入吞吐: %.2f MB/s\n",
           db_size / 1024.0 / 1024.0 / (write_time / 1e6));

    /* 4. 顺序读取测试 */
    printf("\n=== 4. 顺序读取基准测试 ===\n");
    double read_time = bench_sequential_read(&storage, db_size);
    printf("读取 %zu 字节耗时: %.2f ms\n", db_size, read_time / 1000.0);
    printf("读取吞吐: %.2f MB/s\n",
           db_size / 1024.0 / 1024.0 / (read_time / 1e6));

    /* 5. 随机读取测试 */
    printf("\n=== 5. 随机读取基准测试 ===\n");
    const int random_ops = 10000;
    double random_time = bench_random_read(&storage, db_size, random_ops);
    printf("随机读取 %d 次耗时: %.2f ms\n", random_ops, random_time / 1000.0);
    printf("平均每次: %.2f us\n", random_time / random_ops);

    /* 6. mmap 存储引擎设计 */
    printf("\n\n=== 6. MMAP 存储引擎设计要点 ===\n");
    printf(
        "  1. 页面管理: 将数据划分为固定大小的页面（4KB）\n"
        "  2. 页面表: 维护 PageID → 虚拟地址的映射\n"
        "  3. 预读策略: 顺序扫描时提前映射未来页\n"
        "  4. 脏页追踪: 记录修改过的页面用于刷盘\n"
        "  5. WAL 日志: mmap 数据变更先写 WAL，保证持久性\n"
        "  6. 崩溃恢复: 重启时扫描 WAL 重放操作\n"
    );

    /* 7. MMAP vs 传统 I/O */
    printf("\n\n=== 7. MMAP vs 传统 I/O ===\n");
    printf(
        "  场景          | MMAP 优势          | 传统 I/O 优势\n"
        "  --------------|--------------------|--------------------\n"
        "  顺序读写      | 吞吐量高           | 稳定可控\n"
        "  随机访问      | 延迟低             | 无缺页开销\n"
        "  大文件        | 内存不足时受限     | 稳定\n"
        "  多进程共享    | 原生支持           | 需要额外机制\n"
        "  崩溃一致性    | 需要 WAL           | fsync 即可\n"
    );

    /* 8. 已知使用 mmap 的数据库 */
    printf("\n\n=== 8. 使用 mmap 的数据库 ===\n");
    printf(
        "  - SQLite: 默认使用 mmap 优化小查询\n"
        "  - WiredTiger (MongoDB): 部分场景使用 mmap\n"
        "  - RocksDB: 可选 mmap 选项\n"
        "  - LeanStorage: 纯 mmap 存储引擎\n"
    );

    /* 清理 */
    mmap_storage_close(&storage);
    unlink(db_file);

    printf("\n===========================================\n");
    printf("  mmap 数据库演示完成\n");
    printf("===========================================\n");

    return 0;
}
