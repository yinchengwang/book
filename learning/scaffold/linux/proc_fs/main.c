/**
 * @file main.c
 * @brief procfs 文件系统接口演示
 *
 * 演示读取 /proc/meminfo、/proc/cpuinfo、/proc/self/status
 * 展示 procfs 在 Linux 系统诊断中的应用
 *
 * 编译：gcc -std=c11 -o proc_fs main.c
 * 运行：./proc_fs
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 读取文件内容并打印 */
static int read_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    char line[256];
    printf("\n=== %s ===\n", path);
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    fclose(fp);
    return 0;
}

/* 解析 /proc/meminfo 获取关键指标 */
static void parse_meminfo(void) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("fopen /proc/meminfo");
        return;
    }

    unsigned long mem_total = 0, mem_free = 0, mem_available = 0;
    unsigned long buffers = 0, cached = 0, swap_cached = 0;
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        unsigned long val;
        if (sscanf(line, "MemTotal: %lu kB", &val) == 1) mem_total = val;
        else if (sscanf(line, "MemFree: %lu kB", &val) == 1) mem_free = val;
        else if (sscanf(line, "MemAvailable: %lu kB", &val) == 1) mem_available = val;
        else if (sscanf(line, "Buffers: %lu kB", &val) == 1) buffers = val;
        else if (sscanf(line, "Cached: %lu kB", &val) == 1) cached = val;
        else if (sscanf(line, "SwapCached: %lu kB", &val) == 1) swap_cached = val;
    }
    fclose(fp);

    printf("\n=== 内存统计 ===\n");
    printf("总内存:    %lu MB\n", mem_total / 1024);
    printf("空闲内存:  %lu MB\n", mem_free / 1024);
    printf("可用内存:  %lu MB\n", mem_available / 1024);
    printf("Buffers:   %lu MB\n", buffers / 1024);
    printf("Cached:    %lu MB\n", cached / 1024);
    if (mem_total > 0) {
        printf("使用率:    %.1f%%\n",
               100.0 * (1.0 - (double)mem_available / mem_total));
    }
}

/* 解析 /proc/cpuinfo 获取 CPU 信息 */
static void parse_cpuinfo(void) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        perror("fopen /proc/cpuinfo");
        return;
    }

    int cpu_count = 0;
    char model[256] = {0};
    char vendor[256] = {0};
    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "processor", 9) == 0) {
            cpu_count++;
        } else if (strncmp(line, "model name", 10) == 0 && !model[0]) {
            char *colon = strchr(line, ':');
            if (colon) {
                strncpy(model, colon + 2, sizeof(model) - 1);
                model[strlen(model) - 1] = '\0';  // 去掉换行
            }
        } else if (strncmp(line, "vendor_id", 9) == 0 && !vendor[0]) {
            char *colon = strchr(line, ':');
            if (colon) {
                strncpy(vendor, colon + 2, sizeof(vendor) - 1);
                vendor[strlen(vendor) - 1] = '\0';
            }
        }
    }
    fclose(fp);

    printf("\n=== CPU 信息 ===\n");
    printf("型号:      %s\n", model);
    printf("厂商:      %s\n", vendor);
    printf("核心数:    %d\n", cpu_count);
}

/* 解析 /proc/self/status 获取进程状态 */
static void parse_process_status(void) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/status");

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("fopen /proc/self/status");
        return;
    }

    printf("\n=== 进程状态 (/proc/self/status) ===\n");
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        /* 只打印关键字段 */
        if (strncmp(line, "Name:", 5) == 0 ||
            strncmp(line, "State:", 6) == 0 ||
            strncmp(line, "Pid:", 4) == 0 ||
            strncmp(line, "PPid:", 5) == 0 ||
            strncmp(line, "VmSize:", 7) == 0 ||
            strncmp(line, "VmRSS:", 6) == 0 ||
            strncmp(line, "Threads:", 9) == 0) {
            printf("%s", line);
        }
    }
    fclose(fp);
}

int main(void) {
    printf("===========================================\n");
    printf("  procfs 文件系统接口演示\n");
    printf("===========================================\n");

    /* 演示直接读取 /proc 文件 */
    printf("\n[1] 直接读取 /proc/meminfo");
    read_file("/proc/meminfo");

    printf("\n[2] 直接读取 /proc/cpuinfo (前20行)");
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        char line[256];
        int count = 0;
        while (fgets(line, sizeof(line), fp) && count++ < 20) {
            printf("%s", line);
        }
        fclose(fp);
    }

    /* 解析关键指标 */
    printf("\n\n[3] 解析 /proc/meminfo 关键指标");
    parse_meminfo();

    printf("\n[4] 解析 /proc/cpuinfo");
    parse_cpuinfo();

    printf("\n[5] 解析 /proc/self/status");
    parse_process_status();

    printf("\n===========================================\n");
    printf("  procfs 演示完成\n");
    printf("===========================================\n");

    return 0;
}
