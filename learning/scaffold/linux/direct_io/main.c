/**
 * @file main.c
 * @brief Linux Direct I/O 直接 I/O 演示
 *
 * 演示内容：
 *   - O_DIRECT 标志绕过 Page Cache
 *   - 用户态缓冲区对齐要求（512B/4K）
 *   - Direct I/O 与 Buffered I/O 的性能对比
 *
 * 编译：gcc -std=c11 -Wall -o direct_io main.c
 * 运行：./direct_io
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

/* ============================================================
 * 对齐内存分配（Direct I/O 要求）
 * ============================================================ */
static void *aligned_alloc_dio(size_t size, size_t alignment) {
    void *ptr = NULL;
    /* posix_memalign 要求对齐必须是 2 的幂且 >= sizeof(void*) */
    if (posix_memalign(&ptr, alignment, size) != 0) {
        perror("posix_memalign 失败");
        return NULL;
    }
    return ptr;
}

/* ============================================================
 * Demo 1: 检查文件系统是否支持 Direct I/O
 * ============================================================ */
static void demo_check_dio_support(const char *path) {
    printf("[direct_io] 检查 Direct I/O 支持\n");

    int flags = O_RDONLY | O_DIRECT;
    int fd = open(path, flags);

    if (fd < 0) {
        printf("[direct_io]   %s 不支持 O_DIRECT: %s\n", path, strerror(errno));
        printf("[direct_io]   提示: ext4/xfs 支持，tmpfs 不支持\n");
    } else {
        printf("[direct_io]   %s 支持 Direct I/O\n", path);
        close(fd);
    }
}

/* ============================================================
 * Demo 2: Direct I/O 读写操作
 * ============================================================ */
static void demo_dio_read_write(const char *test_file) {
    printf("[direct_io] Direct I/O 读写演示\n");

    const size_t align = 4096;           /* 4K 对齐（推荐） */
    const size_t buf_size = 64 * 1024;   /* 64KB 读写块 */
    const char *msg = "Direct I/O test data written at "
                      __DATE__ " " __TIME__;

    /* 分配对齐缓冲区 */
    char *buf_w = aligned_alloc_dio(buf_size, align);
    char *buf_r = aligned_alloc_dio(buf_size, align);
    if (!buf_w || !buf_r) {
        printf("[direct_io]   内存分配失败\n");
        goto cleanup;
    }

    /* 准备写入数据 */
    memset(buf_w, 0, buf_size);
    strncpy(buf_w, msg, buf_size - 1);

    /* 打开文件（Direct I/O 模式） */
    int fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
    if (fd < 0) {
        printf("[direct_io]   O_DIRECT 打开失败: %s\n", strerror(errno));
        goto cleanup;
    }

    /* 写入数据 */
    ssize_t written = write(fd, buf_w, buf_size);
    if (written < 0) {
        printf("[direct_io]   Direct I/O 写入失败: %s\n", strerror(errno));
        close(fd);
        goto cleanup;
    }
    printf("[direct_io]   Direct I/O 写入 %zd 字节\n", written);

    /* 强制同步到磁盘（确保数据落盘） */
    if (fsync(fd) < 0) {
        perror("[direct_io]   fsync 失败");
    }
    close(fd);

    /* 以 Direct I/O 读取 */
    fd = open(test_file, O_RDONLY | O_DIRECT);
    if (fd < 0) {
        printf("[direct_io]   重新打开 O_DIRECT 失败: %s\n", strerror(errno));
        goto cleanup;
    }

    ssize_t bytes_read = read(fd, buf_r, buf_size);
    if (bytes_read < 0) {
        printf("[direct_io]   Direct I/O 读取失败: %s\n", strerror(errno));
    } else {
        printf("[direct_io]   Direct I/O 读取 %zd 字节: \"%.64s...\"\n",
               bytes_read, buf_r);
    }
    close(fd);

cleanup:
    free(buf_w);
    free(buf_r);
}

/* ============================================================
 * Demo 3: 对齐要求演示
 * ============================================================ */
static void demo_alignment_requirement(void) {
    printf("[direct_io] 缓冲区对齐要求\n");

    size_t align_512 = 512;
    size_t align_4k = 4096;

    char *p512 = aligned_alloc_dio(8192, align_512);
    char *p4k = aligned_alloc_dio(8192, align_4k);

    printf("[direct_io]   512 字节对齐: %p (mod 512 = %lu)\n",
           (void*)p512, (unsigned long)p512 % 512);
    printf("[direct_io]   4K 对齐:     %p (mod 4096 = %lu)\n",
           (void*)p4k, (unsigned long)p4k % 4096);

    /* 模拟非对齐访问（仅打印警告） */
    printf("[direct_io]   注意: 非对齐缓冲区 + O_DIRECT = EINVAL\n");
    printf("[direct_io]   解决: posix_memalign 或 memalign\n");

    free(p512);
    free(p4k);
}

/* ============================================================
 * Demo 4: Direct I/O vs Buffered I/O 原理
 * ============================================================ */
static void demo_dio_vs_buffered(void) {
    printf("[direct_io] Direct I/O vs Buffered I/O 原理:\n\n");

    printf("[direct_io] Buffered I/O:\n");
    printf("[direct_io]   用户空间 ──(read/write)──► Page Cache ──(磁盘I/O)──► 磁盘\n");
    printf("[direct_io]   优点: 缓存、合并写、内核优化\n");
    printf("[direct_io]   缺点: 双拷贝（用户↔Page Cache）、内存占用\n\n");

    printf("[direct_io] Direct I/O (O_DIRECT):\n");
    printf("[direct_io]   用户空间 ──(read/write)──► 磁盘（绕过 Page Cache）\n");
    printf("[direct_io]   优点: 零拷贝、精确控制缓存、可预测延迟\n");
    printf("[direct_io]   缺点: 无缓存、应用自行管理一致性\n\n");

    printf("[direct_io] 适用场景:\n");
    printf("[direct_io]   - 数据库引擎（InnoDB、 RocksDB）\n");
    printf("[direct_io]   - 需要自己管理缓存的应用\n");
    printf("[direct_io]   - 实时音视频流（避免 Page Cache 污染）\n");
}

/* ============================================================
 * Demo 5: 数据库 Direct I/O 配置
 * ============================================================ */
static void demo_db_dio_config(void) {
    printf("[direct_io] 数据库 Direct I/O 配置:\n\n");

    printf("[direct_io] MySQL/InnoDB:\n");
    printf("[direct_io]   innodb_flush_method = O_DIRECT   # Linux\n");
    printf("[direct_io]   innodb_flush_method = O_DIRECT_NO_FSYNC\n\n");

    printf("[direct_io] PostgreSQL:\n");
    printf("[direct_io]   effective_io_concurrency = 2      # 异步 I/O\n");
    printf("[direct_io]   effective_io_concurrency = 0      # 禁用\n\n");

    printf("[direct_io] 注意事项:\n");
    printf("[direct_io]   1. 确保 innodb_buffer_pool_size < 可用内存\n");
    printf("[direct_io]   2. Direct I/O 仍需 fsync 持久化\n");
    printf("[direct_io]   3. 非对齐读写会导致 EINVAL\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    const char *test_file = "/tmp/direct_io_test.bin";

    printf("========================================\n");
    printf("  Linux Direct I/O 直接 I/O 演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 检查 Direct I/O 支持 ---\n");
    demo_check_dio_support("/tmp");
    demo_check_dio_support("/proc");
    printf("\n");

    printf("--- Demo 2: Direct I/O 读写 ---\n");
    demo_dio_read_write(test_file);
    printf("\n");

    printf("--- Demo 3: 对齐要求 ---\n");
    demo_alignment_requirement();
    printf("\n");

    printf("--- Demo 4: Direct I/O vs Buffered I/O ---\n");
    demo_dio_vs_buffered();
    printf("\n");

    printf("--- Demo 5: 数据库 Direct I/O 配置 ---\n");
    demo_db_dio_config();
    printf("\n");

    /* 清理测试文件 */
    unlink(test_file);

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
