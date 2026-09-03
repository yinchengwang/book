/**
 * @file main.c
 * @brief FIO 存储性能基准测试学习演示
 *
 * 演示内容：
 *   - 使用 system() 调用 fio 工具进行 I/O 性能测试
 *   - C 实现简单的 dd 风格顺序/随机读写基准
 *   - IOPS / 延迟 / 带宽三大指标展示
 *
 * 编译：gcc -std=c11 -Wall -o fio main.c
 * 运行：./fio
 * 前提：系统需安装 fio 工具（apt install fio / yum install fio）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

/* pread 在 Windows 上可能不可用，Fallback 到 lseek+read */
#ifdef _WIN32
static ssize_t safe_pread(int fd, void *buf, size_t count, off_t offset) {
    if (lseek(fd, offset, SEEK_SET) < 0) return -1;
    return read(fd, buf, count);
}
#define pread safe_pread
#endif

/* ============================================================
 * 获取当前时间（秒，双精度）
 * ============================================================ */
static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* ============================================================
 * 格式化大小显示
 * ============================================================ */
static const char *fmt_bytes(unsigned long bytes) {
    static char buf[32];
    if (bytes >= 1024UL * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024UL * 1024)
        snprintf(buf, sizeof(buf), "%.2f MB", bytes / (1024.0 * 1024));
    else if (bytes >= 1024)
        snprintf(buf, sizeof(buf), "%.2f KB", bytes / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%lu B", bytes);
    return buf;
}

/* ============================================================
 * C 实现的顺序写基准测试（dd 风格）
 * ============================================================ */
static void bench_seq_write(const char *path, size_t total_bytes, size_t block_size) {
    printf("[fio] 顺序写基准 (dd 风格): %s 字节, 块大小=%s\n",
           fmt_bytes(total_bytes), fmt_bytes(block_size));

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("[fio] open 失败"); return; }

    char *buf = (char *)malloc(block_size);
    memset(buf, 'X', block_size);

    size_t written = 0;
    double start = now_sec();

    while (written < total_bytes) {
        size_t chunk = block_size;
        if (written + chunk > total_bytes) chunk = total_bytes - written;
        ssize_t n = write(fd, buf, chunk);
        if (n < 0) { perror("[fio] write 失败"); break; }
        written += n;
    }

    double elapsed = now_sec() - start;
    close(fd);
    free(buf);

    double bw = (written / (1024.0 * 1024.0)) / (elapsed > 0 ? elapsed : 0.001);
    double iops = (written / (double)block_size) / (elapsed > 0 ? elapsed : 0.001);

    printf("[fio]   写入: %s, 耗时=%.3f s, 带宽=%.1f MB/s, IOPS=%.0f\n\n",
           fmt_bytes(written), elapsed, bw, iops);
}

/* ============================================================
 * C 实现的顺序读基准测试
 * ============================================================ */
static void bench_seq_read(const char *path, size_t block_size) {
    struct stat st;
    if (stat(path, &st) < 0) { printf("[fio] 文件不存在，跳过读测试\n\n"); return; }

    printf("[fio] 顺序读基准: 块大小=%s\n", fmt_bytes(block_size));

    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("[fio] open 失败"); return; }

    char *buf = (char *)malloc(block_size);
    size_t total_read = 0;
    double start = now_sec();

    ssize_t n;
    while ((n = read(fd, buf, block_size)) > 0) {
        total_read += n;
    }

    double elapsed = now_sec() - start;
    close(fd);
    free(buf);

    double bw = (total_read / (1024.0 * 1024.0)) / (elapsed > 0 ? elapsed : 0.001);
    double iops = (total_read / (double)block_size) / (elapsed > 0 ? elapsed : 0.001);

    printf("[fio]   读取: %s, 耗时=%.3f s, 带宽=%.1f MB/s, IOPS=%.0f\n\n",
           fmt_bytes(total_read), elapsed, bw, iops);
}

/* ============================================================
 * C 实现的随机读基准测试
 * ============================================================ */
static void bench_rand_read(const char *path, size_t block_size, int num_ops) {
    struct stat st;
    if (stat(path, &st) < 0) { printf("[fio] 文件不存在，跳过随机读测试\n\n"); return; }

    printf("[fio] 随机读基准: %d 次操作, 块大小=%s\n", num_ops, fmt_bytes(block_size));

    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("[fio] open 失败"); return; }

    char *buf = (char *)malloc(block_size);
    off_t max_blocks = st.st_size / (off_t)block_size;
    if (max_blocks < 1) max_blocks = 1;

    double start = now_sec();
    srand(42);

    for (int i = 0; i < num_ops; i++) {
        off_t offset = (rand() % max_blocks) * (off_t)block_size;
        pread(fd, buf, block_size, offset);
    }

    double elapsed = now_sec() - start;
    close(fd);
    free(buf);

    double iops = num_ops / (elapsed > 0 ? elapsed : 0.001);
    double latency_us = (elapsed / num_ops) * 1e6;
    double bw = (num_ops * block_size / (1024.0 * 1024.0)) / (elapsed > 0 ? elapsed : 0.001);

    printf("[fio]   IOPS=%.0f, 平均延迟=%.1f us, 带宽=%.1f MB/s\n\n",
           iops, latency_us, bw);
}

/* ============================================================
 * Demo 1: FIO 工具概念与参数说明
 * ============================================================ */
static void demo_fio_concepts(void) {
    printf("[fio] === FIO 工具概念 ===\n\n");

    printf("[fio] FIO (Flexible I/O Tester) — Linux 标准 I/O 压测工具\n\n");

    printf("[fio] 核心参数:\n");
    printf("[fio]   --name=NAME          作业名称\n");
    printf("[fio]   --rw=read|write|randread|randwrite|randrw  I/O 模式\n");
    printf("[fio]   --bs=4k              块大小（block size）\n");
    printf("[fio]   --size=1G            测试文件大小\n");
    printf("[fio]   --numjobs=4          并发作业数\n");
    printf("[fio]   --iodepth=32         I/O 队列深度\n");
    printf("[fio]   --direct=1           使用直接 I/O（绕过页缓存）\n");
    printf("[fio]   --runtime=30         运行时间（秒）\n");
    printf("[fio]   --group_reporting    聚合报告所有作业\n\n");

    printf("[fio] 三大核心指标:\n");
    printf("[fio]   ① IOPS:   每秒 I/O 操作数（衡量小 I/O 能力）\n");
    printf("[fio]   ② 带宽:   MB/s 吞吐量（衡量大 I/O 能力）\n");
    printf("[fio]   ③ 延迟:   微秒/毫秒级响应时间（衡量交互性能）\n");
}

/* ============================================================
 * Demo 2: 调用 FIO 工具（如果已安装）
 * ============================================================ */
static void demo_fio_run(void) {
    printf("\n[fio] === FIO 工具调用 ===\n\n");

    /* 检查 fio 是否可用 */
    int has_fio = (system("which fio > /dev/null 2>&1") == 0);

    if (!has_fio) {
        printf("[fio] fio 未安装，跳过实际测试\n");
        printf("[fio] 安装: sudo apt install fio  # Debian/Ubuntu\n");
        printf("[fio]        sudo yum install fio  # RHEL/CentOS\n\n");

        printf("[fio] 示例命令（安装后运行）:\n");
        printf("[fio]   fio --name=seq-read --rw=read --bs=1M "
               "--size=256M --direct=1 --numjobs=1\n");
        return;
    }

    printf("[fio] fio 已安装，运行快速测试 ...\n\n");

    /* 顺序写测试 */
    printf("[fio] 测试 1: 顺序写（1MB 块，64MB 文件）\n");
    int rc = system(
        "fio --name=seq-write --rw=write --bs=1M --size=64M "
        "--direct=1 --numjobs=1 --iodepth=1 --runtime=5 "
        "--group_reporting --output-format=terse 2>/dev/null"
    );
    if (rc != 0) printf("[fio]   运行失败（可能需要 root 或直接 I/O 不支持）\n");

    /* 随机读测试 */
    printf("\n[fio] 测试 2: 随机读（4KB 块，64MB 文件）\n");
    rc = system(
        "fio --name=rand-read --rw=randread --bs=4k --size=64M "
        "--direct=1 --numjobs=1 --iodepth=32 --runtime=5 "
        "--group_reporting --output-format=terse 2>/dev/null"
    );
    if (rc != 0) printf("[fio]   运行失败\n");
}

/* ============================================================
 * Demo 3: C 实现的 dd 风格基准
 * ============================================================ */
static void demo_c_benchmark(void) {
    printf("\n[fio] === C 实现基准测试 ===\n\n");

    const char *test_file = "/tmp/fio_bench_test.dat";

    /* 先写入 64MB 测试数据 */
    printf("[fio] 准备测试数据: 64MB 文件\n");
    bench_seq_write(test_file, 64UL * 1024 * 1024, 1024 * 1024);

    /* 顺序读（大块） */
    bench_seq_read(test_file, 1024 * 1024);

    /* 顺序读（小块） */
    bench_seq_read(test_file, 4096);

    /* 随机读 */
    bench_rand_read(test_file, 4096, 10000);

    /* 清理 */
    unlink(test_file);
    printf("[fio] 已清理测试文件\n\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  FIO 存储性能基准测试学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: FIO 概念与参数 ---\n");
    demo_fio_concepts();

    printf("--- Demo 2: FIO 工具调用 ---\n");
    demo_fio_run();

    printf("\n--- Demo 3: C 实现基准测试 ---\n");
    demo_c_benchmark();

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
