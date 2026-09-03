/**
 * @file main.c
 * @brief blktrace 块设备追踪演示
 *
 * 演示 blktrace + blkparse 分析 I/O 延迟
 * 展示磁盘 I/O 请求追踪和延迟分解
 *
 * 编译：gcc -std=c11 -o blktrace main.c -lpthread
 * 运行：./blktrace
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>

/* 模拟不同类型的 I/O 操作 */
typedef enum {
    IO_READ,
    IO_WRITE,
    IO_SYNC,
    IO_RANDOM,
    IO_SEQUENTIAL
} IoType;

/* I/O 操作统计 */
typedef struct {
    size_t total_bytes;
    size_t io_count;
    double total_time_us;
    double min_latency_us;
    double max_latency_us;
} IoStats;

static IoStats g_stats = {0};
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 获取微秒级时间戳 */
static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

/* 模拟同步 I/O 操作 */
static int do_sync_io(const char *path, int write, size_t size, off_t offset) {
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    char *buf = malloc(size);
    if (!buf) {
        close(fd);
        return -1;
    }

    if (write) {
        memset(buf, 'A', size);
    }

    double start = get_time_us();

    if (write) {
        pwrite(fd, buf, size, offset);
        fsync(fd);  /* 强制刷盘 */
    } else {
        pread(fd, buf, size, offset);
    }

    double elapsed = get_time_us() - start;

    pthread_mutex_lock(&g_stats_mutex);
    g_stats.total_bytes += size;
    g_stats.io_count++;
    g_stats.total_time_us += elapsed;
    if (g_stats.min_latency_us == 0 || elapsed < g_stats.min_latency_us) {
        g_stats.min_latency_us = elapsed;
    }
    if (elapsed > g_stats.max_latency_us) {
        g_stats.max_latency_us = elapsed;
    }
    pthread_mutex_unlock(&g_stats_mutex);

    free(buf);
    close(fd);
    return 0;
}

/* 顺序 I/O 线程 */
static void *sequential_io_thread(void *arg) {
    const char *path = (const char *)arg;
    const size_t block_size = 4096;
    const int num_blocks = 256;

    for (int i = 0; i < num_blocks; i++) {
        do_sync_io(path, 1, block_size, i * block_size);
    }

    /* 读回验证 */
    for (int i = 0; i < num_blocks; i++) {
        do_sync_io(path, 0, block_size, i * block_size);
    }

    return NULL;
}

/* 随机 I/O 线程 */
static void *random_io_thread(void *arg) {
    const char *path = (const char *)arg;
    const size_t block_size = 4096;
    const int num_ops = 100;
    unsigned int seed = time(NULL) ^ pthread_self();

    for (int i = 0; i < num_ops; i++) {
        off_t offset = (rand_r(&seed) % 1024) * block_size;
        do_sync_io(path, 1, block_size, offset);
    }

    return NULL;
}

/* 打印 I/O 统计 */
static void print_io_stats(void) {
    pthread_mutex_lock(&g_stats_mutex);
    printf("\n=== I/O 统计 ===\n");
    printf("总字节数:  %zu bytes (%.2f MB)\n",
           g_stats.total_bytes, g_stats.total_bytes / 1024.0 / 1024.0);
    printf("I/O 次数:  %zu\n", g_stats.io_count);
    printf("总耗时:    %.2f ms\n", g_stats.total_time_us / 1000.0);
    printf("平均延迟:  %.2f us\n",
           g_stats.io_count > 0 ? g_stats.total_time_us / g_stats.io_count : 0);
    printf("最小延迟:  %.2f us\n", g_stats.min_latency_us);
    printf("最大延迟:  %.2f us\n", g_stats.max_latency_us);
    pthread_mutex_unlock(&g_stats_mutex);
}

/* 检查 blktrace 是否可用 */
static int check_blktrace_available(void) {
    FILE *fp = popen("which blktrace 2>/dev/null", "r");
    if (!fp) return 0;
    char buf[64];
    int available = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);
    return available;
}

int main(void) {
    printf("===========================================\n");
    printf("  blktrace 块设备追踪演示\n");
    printf("===========================================\n");

    const char *test_file = "/tmp/blktrace_test.dat";

    /* 检查 blktrace 可用性 */
    if (!check_blktrace_available()) {
        printf("\n[警告] blktrace 未安装，需要:\n");
        printf("  sudo apt install blktrace\n");
        printf("\n继续运行 I/O 延迟模拟...\n");
    }

    /* 1. 顺序写入 I/O */
    printf("\n\n=== 1. 顺序写入测试 ===");
    printf("\n[说明] 顺序写入是磁盘 I/O 的理想场景\n");
    printf("[操作] 写入 256 个 4KB 块...\n");

    pthread_t seq_thread;
    double start = get_time_us();
    pthread_create(&seq_thread, NULL, sequential_io_thread, (void *)test_file);
    pthread_join(seq_thread, NULL);
    printf("顺序写入耗时: %.2f ms\n", (get_time_us() - start) / 1000.0);

    /* 2. 随机 I/O */
    printf("\n\n=== 2. 随机 I/O 测试 ===");
    printf("\n[说明] 随机 I/O 会触发磁盘寻道，高延迟\n");
    printf("[操作] 执行 100 次随机 4KB 写入...\n");

    pthread_t rand_thread;
    start = get_time_us();
    pthread_create(&rand_thread, NULL, random_io_thread, (void *)test_file);
    pthread_join(rand_thread, NULL);
    printf("随机 I/O 耗时: %.2f ms\n", (get_time_us() - start) / 1000.0);

    /* 3. I/O 延迟分解说明 */
    printf("\n\n=== 3. I/O 延迟分解 ===");
    printf("\n[说明] 一次磁盘 I/O 的延迟组成:\n");
    printf("  1. 队列等待 (queue wait): 请求在调度队列中等待\n");
    printf("  2. 寻道时间 (seek): 磁头移动到目标磁道\n");
    printf("  3. 旋转延迟 (rotation): 等待目标扇区旋转到磁头下方\n");
    printf("  4. 传输时间 (transfer): 数据实际读写\n");
    printf("\n总延迟 = 队列等待 + 寻道 + 旋转 + 传输\n");

    /* 4. 打印统计 */
    print_io_stats();

    /* 5. blktrace 使用说明 */
    printf("\n\n=== 5. blktrace 使用说明 ===\n");
    printf("\n# 启动追踪（需要 root）\n");
    printf("sudo blktrace -d /dev/sda -o /tmp/trace\n");
    printf("\n# 运行 I/O 工作负载\n");
    printf("./blktrace\n");
    printf("\n# 停止追踪（Ctrl+C）\n");
    printf("\n# 分析追踪结果\n");
    printf("blkparse -i /tmp/trace -d trace.bin | less\n");
    printf("\n# 生成 I/O 延迟直方图\n");
    printf("iostat -x 1\n");
    printf("\n# 查看特定扇区访问\n");
    printf("blkparse -i /tmp/trace -s | grep 'Q  R'\n");

    /* 清理 */
    unlink(test_file);

    printf("\n===========================================\n");
    printf("  blktrace 演示完成\n");
    printf("===========================================\n");

    return 0;
}
