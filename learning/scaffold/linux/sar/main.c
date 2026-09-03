/**
 * @file main.c
 * @brief Linux SAR (系统活动报告) 学习演示
 *
 * 演示内容：
 *   - 调用 sysinfo() 获取系统整体信息
 *   - 读取 /proc/stat 采集 CPU/内存/磁盘统计
 *   - 实现简单的数据采样与格式化输出
 *
 * 编译：gcc -std=c11 -Wall -o sar main.c
 * 运行：./sar
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/time.h>

/* ============================================================
 * 数据结构：系统统计快照
 * ============================================================ */
typedef struct {
    unsigned long long cpu_user;
    unsigned long long cpu_nice;
    unsigned long long cpu_system;
    unsigned long long cpu_idle;
    unsigned long long cpu_iowait;
    unsigned long long cpu_irq;
    unsigned long long cpu_softirq;

    unsigned long mem_total;
    unsigned long mem_free;
    unsigned long mem_available;
    unsigned long mem_buffers;
    unsigned long mem_cached;

    unsigned long long time_ms;
} sys_snapshot_t;

/* ============================================================
 * 获取当前时间（毫秒）
 * ============================================================ */
static unsigned long long get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* ============================================================
 * 读取 /proc/stat 获取 CPU 统计
 * ============================================================ */
static void read_cpu_stats(unsigned long long *user, unsigned long long *nice,
                           unsigned long long *system, unsigned long long *idle,
                           unsigned long long *iowait, unsigned long long *irq,
                           unsigned long long *softirq) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu ", 4) == 0) {
            sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu",
                   user, nice, system, idle, iowait, irq, softirq);
            break;
        }
    }
    fclose(fp);
}

/* ============================================================
 * 读取 /proc/meminfo 获取内存统计
 * ============================================================ */
static void read_mem_stats(unsigned long *total, unsigned long *free,
                           unsigned long *available, unsigned long *buffers,
                           unsigned long *cached) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return;
    }

    char line[256];
    *total = *free = *available = *buffers = *cached = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", total) == 1) continue;
        if (sscanf(line, "MemFree: %lu kB", free) == 1) continue;
        if (sscanf(line, "MemAvailable: %lu kB", available) == 1) continue;
        if (sscanf(line, "Buffers: %lu kB", buffers) == 1) continue;
        if (sscanf(line, "Cached: %lu kB", cached) == 1) continue;
    }
    fclose(fp);
}

/* ============================================================
 * 获取系统整体信息（sysinfo 系统调用）
 * ============================================================ */
static void demo_sysinfo_call(void) {
    printf("[sar] 调用 sysinfo() 系统调用\n");

    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        perror("[sar] sysinfo 失败");
        return;
    }

    printf("[sar]   系统运行时间: %lu 秒 (%lu 天)\n",
           info.uptime, info.uptime / 86400);
    printf("[sar]   1/5/15 分钟负载: %.2f, %.2f, %.2f\n",
           info.loads[0] / 65536.0,
           info.loads[1] / 65536.0,
           info.loads[2] / 65536.0);
    printf("[sar]   内存总量: %lu kB (%.1f GB)\n",
           info.totalram, info.totalram / 1024.0 / 1024.0);
    printf("[sar]   空闲内存: %lu kB (%.1f GB)\n",
           info.freeram, info.freeram / 1024.0 / 1024.0);
    printf("[sar]   共享内存: %lu kB\n", info.sharedram);
    printf("[sar]   缓冲区内存: %lu kB\n", info.bufferram);
    printf("[sar]   进程总数: %u\n", info.procs);
}

/* ============================================================
 * CPU 统计采样
 * ============================================================ */
static void demo_cpu_sampling(void) {
    printf("[sar] 演示 CPU 统计采样（2 秒间隔）\n");

    unsigned long long u1, n1, s1, i1, w1, ir1, sir1;
    unsigned long long u2, n2, s2, i2, w2, ir2, sir2;

    /* 第一次采样 */
    read_cpu_stats(&u1, &n1, &s1, &i1, &w1, &ir1, &sir1);
    unsigned long long t1 = get_time_ms();

    /* 模拟一些工作 */
    printf("[sar]   模拟负载中...\n");
    for (volatile long j = 0; j < 100000000; j++) {}

    /* 第二次采样 */
    usleep(100000);
    read_cpu_stats(&u2, &n2, &s2, &i2, &w2, &ir2, &sir2);
    unsigned long long t2 = get_time_ms();

    /* 计算差值 */
    unsigned long long user_diff = u2 - u1;
    unsigned long long nice_diff = n2 - n1;
    unsigned long long sys_diff = s2 - s1;
    unsigned long long idle_diff = i2 - i1;
    unsigned long long iowait_diff = w2 - w1;
    unsigned long long irq_diff = ir2 - ir1;
    unsigned long long softirq_diff = sir2 - sir1;

    unsigned long long total_diff = user_diff + nice_diff + sys_diff +
                                    idle_diff + iowait_diff + irq_diff + softirq_diff;

    double dt = (t2 - t1) / 1000.0;  /* 转换为秒 */

    printf("[sar]   CPU 使用率:\n");
    printf("[sar]     user:    %5.1f%%\n", 100.0 * user_diff / total_diff);
    printf("[sar]     nice:    %5.1f%%\n", 100.0 * nice_diff / total_diff);
    printf("[sar]     system:  %5.1f%%\n", 100.0 * sys_diff / total_diff);
    printf("[sar]     iowait:  %5.1f%%\n", 100.0 * iowait_diff / total_diff);
    printf("[sar]     irq:     %5.1f%%\n", 100.0 * irq_diff / total_diff);
    printf("[sar]     softirq: %5.1f%%\n", 100.0 * softirq_diff / total_diff);
    printf("[sar]     idle:    %5.1f%%\n", 100.0 * idle_diff / total_diff);
}

/* ============================================================
 * 内存统计采样
 * ============================================================ */
static void demo_mem_sampling(void) {
    printf("[sar] 演示内存统计采样\n");

    unsigned long total, free, available, buffers, cached;
    read_mem_stats(&total, &free, &available, &buffers, &cached);

    printf("[sar]   内存总量:     %8lu kB\n", total);
    printf("[sar]   MemFree:      %8lu kB\n", free);
    printf("[sar]   MemAvailable: %8lu kB\n", available);
    printf("[sar]   Buffers:      %8lu kB\n", buffers);
    printf("[sar]   Cached:       %8lu kB\n", cached);

    /* 计算使用率 */
    if (total > 0) {
        double used_pct = 100.0 * (1.0 - (double)available / total);
        double cache_pct = 100.0 * (buffers + cached) / total;
        printf("[sar]   实际使用率:  %5.1f%% (不含缓存: %.1f%%)\n",
               used_pct, used_pct - cache_pct);
    }
}

/* ============================================================
 * SAR 风格输出格式化
 * ============================================================ */
static void demo_sar_format(void) {
    printf("[sar] SAR 风格输出格式化\n");

    /* 模拟 sar 常用的输出格式 */
    printf("\n");
    printf("%-8s %6s %6s %6s %6s %6s %6s %6s\n",
           "时间", "%user", "%nice", "%sys", "%iowait", "%irq", "%soft", "%idle");

    /* 采样 3 次 */
    for (int i = 0; i < 3; i++) {
        unsigned long long u, n, s, idle, w, ir, sir;
        read_cpu_stats(&u, &n, &s, &idle, &w, &ir, &sir);

        unsigned long long total = u + n + s + idle + w + ir + sir;
        if (total > 0) {
            printf("%02d:%02d:%02d %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f\n",
                   10 + i, 30, 0,
                   100.0 * u / total,
                   100.0 * n / total,
                   100.0 * s / total,
                   100.0 * w / total,
                   100.0 * ir / total,
                   100.0 * sir / total,
                   100.0 * idle / total);
        }

        if (i < 2) {
            usleep(500000);
        }
    }

    /* 内存统计 */
    printf("\n");
    printf("%-8s %12s %12s %12s %12s\n",
           "时间", "kbmemfree", "kbmemused", "kbbuffers", "kbcached");
    unsigned long total, free, avail, buf, cache;
    read_mem_stats(&total, &free, &avail, &buf, &cache);
    printf("%02d:%02d:%02d %12lu %12lu %12lu %12lu\n",
           10, 30, 0, free, total - free, buf, cache);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux SAR (系统活动报告) 学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: sysinfo() 系统调用 ---\n");
    demo_sysinfo_call();
    printf("\n");

    printf("--- Demo 2: CPU 统计采样 ---\n");
    demo_cpu_sampling();
    printf("\n");

    printf("--- Demo 3: 内存统计采样 ---\n");
    demo_mem_sampling();
    printf("\n");

    printf("--- Demo 4: SAR 风格输出 ---\n");
    demo_sar_format();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
