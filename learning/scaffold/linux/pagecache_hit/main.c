/**
 * @file main.c
 * @brief 页缓存命中率分析演示
 *
 * 演示 /proc/vmstat 统计、页缓存行为分析
 * 展示缓存命中率的计算和优化思路
 *
 * 编译：gcc -std=c11 -o pagecache_hit main.c
 * 运行：./pagecache_hit
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* 缓存统计结构 */
typedef struct {
    unsigned longpgfault;      /* 页错误总数 */
    unsigned longpgmajfault;   /* 主文件页错误（从磁盘加载）*/
    unsigned longpgpgin;       /* 页换入次数 */
    unsigned longpgpgout;      /* 页换出次数 */
    unsigned longpgfrees;      /* 页释放次数 */
    unsigned longpgsteal;      /* 页回收次数 */
    unsigned longnuma_hit;     /* NUMA 命中 */
    unsigned longnuma_miss;    /* NUMA 未命中 */
} VmStat;

/* 读取 /proc/vmstat */
static int read_vmstat(VmStat *stat) {
    FILE *fp = fopen("/proc/vmstat", "r");
    if (!fp) {
        perror("fopen /proc/vmstat");
        return -1;
    }

    memset(stat, 0, sizeof(*stat));
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        unsigned long val;
        if (sscanf(line, "pgfault %lu", &val) == 1) stat->pgfault = val;
        else if (sscanf(line, "pgmajfault %lu", &val) == 1) stat->pgmajfault = val;
        else if (sscanf(line, "pgpgin %lu", &val) == 1) stat->pgpgin = val;
        else if (sscanf(line, "pgpgout %lu", &val) == 1) stat->pgpgout = val;
        else if (sscanf(line, "pgfrees %lu", &val) == 1) stat->pgfrees = val;
        else if (sscanf(line, "pgsteal %lu", &val) == 1) stat->pgsteal = val;
        else if (sscanf(line, "numa_hit %lu", &val) == 1) stat->numa_hit = val;
        else if (sscanf(line, "numa_miss %lu", &val) == 1) stat->numa_miss = val;
    }
    fclose(fp);
    return 0;
}

/* 打印页缓存统计 */
static void print_vmstat(const VmStat *before, const VmStat *after) {
    printf("\n=== 页缓存统计变化 ===\n");
    printf("%-20s %12s %12s %12s\n", "指标", "开始", "结束", "增量");
    printf("%-20s %12s %12s %12s\n", "------", "------", "------", "------");

    #define PRINT_STAT(name) do { \
        unsigned long start = before->name; \
        unsigned long end = after->name; \
        unsigned long delta = end - start; \
        printf("%-20s %12lu %12lu %12lu\n", #name, start, end, delta); \
    } while(0)

    PRINT_STAT(pgfault);
    PRINT_STAT(pgmajfault);
    PRINT_STAT(pgpgin);
    PRINT_STAT(pgpgout);
    PRINT_STAT(pgfrees);
    PRINT_STAT(pgsteal);
    PRINT_STAT(numa_hit);
    PRINT_STAT(numa_miss);
}

/* 计算命中率 */
static double calc_hit_ratio(unsigned long hit, unsigned long total) {
    if (total == 0) return 0.0;
    return 100.0 * (1.0 - (double)hit / total);
}

/* 执行顺序读（缓存友好）*/
static void sequential_read(const char *path, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    char *buf = malloc(size);
    size_t total_read = 0;

    printf("\n[顺序读] 读取 %zu 字节...\n", size);
    ssize_t n;
    off_t offset = 0;
    while ((n = pread(fd, buf, size, offset)) > 0 && total_read < size * 10) {
        total_read += n;
        offset += n;
    }

    printf("[顺序读] 实际读取: %zu 字节\n", total_read);
    free(buf);
    close(fd);
}

/* 执行随机读（缓存不友好）*/
static void random_read(const char *path, size_t block_size, int num_ops) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    char *buf = malloc(block_size);
    unsigned int seed = (unsigned int)getpid();

    printf("\n[随机读] 执行 %d 次随机读取，每次 %zu 字节...\n", num_ops, block_size);

    off_t file_size = lseek(fd, 0, SEEK_END);
    for (int i = 0; i < num_ops; i++) {
        off_t offset = (rand_r(&seed) % (file_size / block_size)) * block_size;
        pread(fd, buf, block_size, offset);
    }

    printf("[随机读] 完成 %d 次读取\n", num_ops);
    free(buf);
    close(fd);
}

/* 创建测试文件 */
static int create_test_file(const char *path, size_t size) {
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    char *buf = malloc(size);
    memset(buf, 'X', size);

    ssize_t written = write(fd, buf, size);
    free(buf);
    close(fd);

    return (written == (ssize_t)size) ? 0 : -1;
}

int main(void) {
    printf("===========================================\n");
    printf("  页缓存命中率分析演示\n");
    printf("===========================================\n");

    const char *test_file = "/tmp/pagecache_test.dat";
    const size_t file_size = 10 * 1024 * 1024;  /* 10MB */

    /* 读取 /proc/meminfo 中的缓存信息 */
    printf("\n\n=== 1. /proc/meminfo 缓存信息 ===\n");
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemAvailable", 12) == 0 ||
                strncmp(line, "Cached:", 7) == 0 ||
                strncmp(line, "Buffers:", 8) == 0 ||
                strncmp(line, "SwapCached", 10) == 0) {
                printf("%s", line);
            }
        }
        fclose(fp);
    }

    /* 创建测试文件 */
    printf("\n\n=== 2. 创建测试文件 ===\n");
    printf("创建 %zu MB 测试文件...\n", file_size / 1024 / 1024);
    if (create_test_file(test_file, file_size) < 0) {
        return 1;
    }
    printf("测试文件创建完成\n");

    /* 记录初始状态 */
    VmStat before, after;
    read_vmstat(&before);

    /* 3. 顺序读（缓存友好）*/
    printf("\n\n=== 3. 顺序读测试 ===");
    printf("\n[说明] 顺序访问触发预读，缓存命中率高\n");
    sequential_read(test_file, 4096);

    /* 4. 重复顺序读（完全命中）*/
    printf("\n\n=== 4. 重复读测试 ===");
    printf("\n[说明] 第二次读取完全命中缓存\n");
    read_vmstat(&after);
    print_vmstat(&before, &after);
    memcpy(&before, &after, sizeof(before));

    sequential_read(test_file, 4096);

    /* 记录中间状态 */
    read_vmstat(&after);
    print_vmstat(&before, &after);
    memcpy(&before, &after, sizeof(before));

    /* 5. 随机读（缓存不友好）*/
    printf("\n\n=== 5. 随机读测试 ===");
    printf("\n[说明] 随机访问无法利用预读，缓存命中率低\n");
    random_read(test_file, 4096, 1000);

    read_vmstat(&after);
    print_vmstat(&before, &after);

    /* 6. 命中率计算 */
    printf("\n\n=== 6. 缓存命中率分析 ===\n");
    unsigned long pg_fault_delta = after.pgfault - before.pgfault;
    unsigned long pg_majfault_delta = after.pgmajfault - before.pgmajfault;

    printf("页错误增量:  %lu\n", pg_fault_delta);
    printf("主缺页错误:  %lu (从磁盘加载)\n", pg_majfault_delta);
    printf("次缺页错误:  %lu (仅页表映射)\n", pg_fault_delta - pg_majfault_delta);
    printf("缓存命中率:  %.2f%%\n",
           calc_hit_ratio(pg_majfault_delta, pg_fault_delta));

    /* 7. /proc/vmstat 关键指标说明 */
    printf("\n\n=== 7. /proc/vmstat 关键指标 ===\n");
    printf("  pgfault     : 页错误总数（次缺页+主缺页）\n");
    printf("  pgmajfault  : 主缺页错误（需要磁盘 I/O）\n");
    printf("  pgpgin      : 页换入次数（从磁盘读入内存）\n");
    printf("  pgpgout     : 页换出次数（从内存写回磁盘）\n");
    printf("  pgsteal     : 页回收次数（被 LRU 换出）\n");
    printf("  numa_hit    : NUMA 节点本地分配命中\n");
    printf("  numa_miss   : NUMA 节点远程分配（跨节点访问）\n");

    /* 清理 */
    unlink(test_file);

    printf("\n===========================================\n");
    printf("  页缓存命中率分析完成\n");
    printf("===========================================\n");

    return 0;
}
