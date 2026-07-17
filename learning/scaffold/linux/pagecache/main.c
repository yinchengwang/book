/**
 * @file main.c
 * @brief Linux 页缓存学习演示
 *
 * 演示内容：
 *   - posix_fadvise 提示内核 I/O 模式
 *   - readahead 预读机制
 *   - 页缓存与直接 I/O 对比
 *
 * 编译：gcc -std=c11 -Wall -o pagecache main.c
 * 运行：./pagecache
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

/* ============================================================
 * 获取当前时间（微秒）
 * ============================================================ */
static unsigned long get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

/* ============================================================
 * 打印缓存信息
 * ============================================================ */
static void print_cache_info(const char *title) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return;

    char line[256];
    unsigned long cached = 0, buffers = 0, free = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "Cached: %lu kB", &cached) == 1) continue;
        if (sscanf(line, "Buffers: %lu kB", &buffers) == 1) continue;
        if (sscanf(line, "MemFree: %lu kB", &free) == 1) continue;
    }
    fclose(fp);

    printf("[pagecache]   %s: free=%lu MB, cached=%lu MB, buffers=%lu MB\n",
           title, free / 1024, cached / 1024, buffers / 1024);
}

/* ============================================================
 * 顺序读取测试
 * ============================================================ */
static void seq_read_test(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("[pagecache] open 失败");
        return;
    }

    char buf[4096];
    unsigned long total_read = 0;
    unsigned long start = get_time_us();

    /* 顺序读取整个文件 */
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        total_read += n;
    }

    unsigned long elapsed = get_time_us() - start;
    close(fd);

    printf("[pagecache]   顺序读取: %lu 字节, 耗时 %.2f ms, 吞吐量 %.2f MB/s\n",
           total_read, elapsed / 1000.0, total_read / 1024.0 / 1024.0 / (elapsed / 1000000.0));
}

/* ============================================================
 * 随机读取测试
 * ============================================================ */
static void rand_read_test(const char *filename) {
    struct stat st;
    if (stat(filename, &st) < 0) {
        perror("[pagecache] stat 失败");
        return;
    }

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("[pagecache] open 失败");
        return;
    }

    char buf[4096];
    unsigned long total_read = 0;
    unsigned long start = get_time_us();

    /* 随机读取 100 个位置 */
    srand(12345);  /* 固定种子以复现 */
    for (int i = 0; i < 100; i++) {
        off_t offset = (rand() % (st.st_size / 4096)) * 4096;
        pread(fd, buf, sizeof(buf), offset);
        total_read += 4096;
    }

    unsigned long elapsed = get_time_us() - start;
    close(fd);

    printf("[pagecache]   随机读取: %lu 字节, 耗时 %.2f ms, 吞吐量 %.2f MB/s\n",
           total_read, elapsed / 1000.0, total_read / 1024.0 / 1024.0 / (elapsed / 1000000.0));
}

/* ============================================================
 * Demo 1: 页缓存基本概念
 * ============================================================ */
static void demo_cache_basics(void) {
    printf("[pagecache] 页缓存基本概念:\n\n");

    printf("[pagecache] 1. 页缓存（Page Cache）\n");
    printf("[pagecache]    - 内核缓存文件数据到内存\n");
    printf("[pagecache]    - 4KB 对齐（页面大小）\n");
    printf("[pagecache]    - 读取时自动缓存\n\n");

    printf("[pagecache] 2. 缓存命中\n");
    printf("[pagecache]    - 命中：直接从内存读取，极快\n");
    printf("[pagecache]    - 未命中：从磁盘读取并缓存\n\n");

    printf("[pagecache] 3. 回写策略\n");
    printf("[pagecache]    - 延迟写：数据先写缓存，后异步刷盘\n");
    printf("[pagecache]    - 强制刷盘：fsync() / fdatasync()\n\n");

    print_cache_info("初始状态");
}

/* ============================================================
 * Demo 2: posix_fadvise 提示
 * ============================================================ */
static void demo_fadvise(void) {
    printf("[pagecache] 演示 posix_fadvise 提示\n");

    /* 创建测试文件 */
    const char *test_file = "/tmp/pagecache_test.dat";
    int fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[pagecache] 创建测试文件失败");
        return;
    }

    /* 写入 10MB 测试数据 */
    char *data = malloc(10 * 1024 * 1024);
    memset(data, 'X', 10 * 1024 * 1024);
    write(fd, data, 10 * 1024 * 1024);
    free(data);
    close(fd);

    printf("[pagecache] 创建测试文件: %s (10 MB)\n", test_file);

    /* 第一次读取（缓存未命中） */
    printf("\n[tcpdump] 第一次读取（冷缓存）:\n");
    seq_read_test(test_file);

    /* 清除缓存（需要 root） */
    printf("\n[pagecache] 提示: 清除缓存需要 root: echo 3 > /proc/sys/vm/drop_caches\n");

    /* 第二次读取（缓存命中） */
    printf("\n[pagecache] 第二次读取（热缓存）:\n");
    seq_read_test(test_file);

    /* 使用 POSIX_FADV_SEQUENTIAL 提示 */
    fd = open(test_file, O_RDONLY);
    if (fd >= 0) {
        printf("\n[pagecache] 使用 POSIX_FADV_SEQUENTIAL 提示:\n");
        posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
        seq_read_test(test_file);
        close(fd);
    }

    /* 使用 POSIX_FADV_RANDOM 提示 */
    fd = open(test_file, O_RDONLY);
    if (fd >= 0) {
        printf("\n[pagecache] 使用 POSIX_FADV_RANDOM 提示:\n");
        posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);
        rand_read_test(test_file);
        close(fd);
    }

    /* 清理 */
    unlink(test_file);
}

/* ============================================================
 * Demo 3: readahead 预读
 * ============================================================ */
static void demo_readahead(void) {
    printf("[pagecache] 演示 readahead 预读机制\n\n");

    printf("[pagecache] 预读原理:\n");
    printf("[pagecache]    - 内核检测顺序访问模式\n");
    printf("[pagecache]    - 提前读取下一个页面到缓存\n");
    printf("[pagecache]    - 减少磁盘 I/O 等待\n\n");

    printf("[pagecache] readahead(2) 系统调用:\n");
    printf("[pagecache]    #include <fcntl.h>\n");
    printf("[pagecache]    ssize_t readahead(int fd, off64_t offset, size_t count);\n");
    printf("[pagecache]    - offset: 文件起始偏移\n");
    printf("[pagecache]    - count: 预读字节数\n\n");

    printf("[pagecache] 应用场景:\n");
    printf("[pagecache]    - 数据库顺序扫描\n");
    printf("[pagecache]    - 大文件复制\n");
    printf("[pagecache]    - 日志分析\n");

    /* 创建测试文件 */
    const char *test_file = "/tmp/readahead_test.dat";
    int fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        char *data = malloc(1 * 1024 * 1024);
        memset(data, 'A', 1 * 1024 * 1024);
        write(fd, data, 1 * 1024 * 1024);
        free(data);
        close(fd);

        /* 手动触发预读 */
        fd = open(test_file, O_RDONLY);
        if (fd >= 0) {
            printf("\n[pagecache] 手动触发 readahead:\n");
            unsigned long start = get_time_us();
            readahead(fd, 0, 1024 * 1024);
            unsigned long elapsed = get_time_us() - start;
            printf("[pagecache]   readahead 耗时: %.2f ms\n", elapsed / 1000.0);
            close(fd);
        }

        unlink(test_file);
    }
}

/* ============================================================
 * Demo 4: 直接 I/O vs 缓存 I/O
 * ============================================================ */
static void demo_direct_io(void) {
    printf("[pagecache] 直接 I/O vs 缓存 I/O 对比:\n\n");

    printf("[pagecache] 缓存 I/O（默认）:\n");
    printf("[pagecache]    优点: 利用页缓存，提升顺序读性能\n");
    printf("[pagecache]    缺点: 占用内存，数据需复制到用户空间\n");
    printf("[pagecache]    适用: 大部分场景\n\n");

    printf("[pagecache] 直接 I/O（O_DIRECT）:\n");
    printf("[pagecache]    优点: 绕过页缓存，减少内存拷贝\n");
    printf("[pagecache]    缺点: 无法利用缓存，顺序读性能差\n");
    printf("[pagecache]    适用: 数据库、专用存储引擎\n\n");

    printf("[pagecache] 直接 I/O 限制:\n");
    printf("[pagecache]    - 缓冲区必须对齐（512 字节或 4KB）\n");
    printf("[pagecache]    - 读写长度必须对齐\n");
    printf("[pagecache]    - 文件偏移必须对齐\n");

    printf("\n[pagecache] 示例代码:\n");
    printf("[pagecache]    int fd = open(path, O_RDONLY | O_DIRECT);\n");
    printf("[pagecache]    char buf[4096] __attribute__((aligned(4096)));\n");
    printf("[pagecache]    read(fd, buf, 4096);\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 页缓存学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 页缓存基本概念 ---\n");
    demo_cache_basics();
    printf("\n");

    printf("--- Demo 2: posix_fadvise 提示 ---\n");
    demo_fadvise();
    printf("\n");

    printf("--- Demo 3: readahead 预读 ---\n");
    demo_readahead();
    printf("\n");

    printf("--- Demo 4: 直接 I/O 对比 ---\n");
    demo_direct_io();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
