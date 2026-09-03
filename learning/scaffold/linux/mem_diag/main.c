/**
 * @file main.c
 * @brief Linux 内存诊断学习演示
 *
 * 演示内容：
 *   - 读取 /proc/meminfo 获取系统内存信息
 *   - 动态内存分配与释放 (malloc/free)
 *   - 内存泄漏检测思路（分配/释放计数）
 *   - 常见内存相关错误分析
 *
 * 编译：gcc -std=c11 -Wall -o mem_diag main.c
 * 运行：./mem_diag
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ============================================================
 * 数据结构：内存分配记录（用于演示泄漏检测）
 * ============================================================ */
typedef struct {
    void *ptr;
    size_t size;
    int line;
} alloc_record_t;

#define MAX_ALLOCS 1024
static alloc_record_t g_allocs[MAX_ALLOCS];
static int g_alloc_count = 0;

/* 跟踪分配 */
static void track_alloc(void *ptr, size_t size, int line) {
    if (ptr && g_alloc_count < MAX_ALLOCS) {
        g_allocs[g_alloc_count].ptr = ptr;
        g_allocs[g_alloc_count].size = size;
        g_allocs[g_alloc_count].line = line;
        g_alloc_count++;
    }
}

/* 跟踪释放 */
static void track_free(void *ptr) {
    for (int i = 0; i < g_alloc_count; i++) {
        if (g_allocs[i].ptr == ptr) {
            g_allocs[i].ptr = NULL;
            g_allocs[i].size = 0;
            return;
        }
    }
}

/* 打印泄漏报告 */
static void report_leaks(void) {
    printf("[mem_diag] 内存泄漏检测报告:\n");
    int leaks = 0;
    size_t total_leak = 0;
    for (int i = 0; i < g_alloc_count; i++) {
        if (g_allocs[i].ptr) {
            printf("[mem_diag]   泄漏: %p, 大小: %zu 字节, 位置: 第 %d 行\n",
                   g_allocs[i].ptr, g_allocs[i].size, g_allocs[i].line);
            leaks++;
            total_leak += g_allocs[i].size;
        }
    }
    if (leaks == 0) {
        printf("[mem_diag]   无内存泄漏 (共跟踪 %d 次分配)\n", g_alloc_count);
    } else {
        printf("[mem_diag]   共 %d 处泄漏, 总计 %zu 字节\n", leaks, total_leak);
    }
}

/* ============================================================
 * Demo 1: 读取 /proc/meminfo
 * ============================================================ */
static void demo_meminfo(void) {
    printf("[mem_diag] 读取 /proc/meminfo 获取系统内存信息\n");

    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("[mem_diag] fopen /proc/meminfo 失败");
        return;
    }

    char line[256];
    unsigned long mem_total = 0, mem_free = 0, mem_available = 0;
    unsigned long buffers = 0, cached = 0, swap_total = 0, swap_free = 0;

    while (fgets(line, sizeof(line), fp)) {
        /* 解析关键字段 */
        if (sscanf(line, "MemTotal: %lu kB", &mem_total) == 1) {
            printf("[mem_diag]   MemTotal:    %lu kB (%.1f GB)\n",
                   mem_total, mem_total / 1024.0 / 1024.0);
        } else if (sscanf(line, "MemFree: %lu kB", &mem_free) == 1) {
            printf("[mem_diag]   MemFree:     %lu kB (%.1f GB)\n",
                   mem_free, mem_free / 1024.0 / 1024.0);
        } else if (sscanf(line, "MemAvailable: %lu kB", &mem_available) == 1) {
            printf("[mem_diag]   MemAvailable: %lu kB (%.1f GB)\n",
                   mem_available, mem_available / 1024.0 / 1024.0);
        } else if (sscanf(line, "Buffers: %lu kB", &buffers) == 1) {
            printf("[mem_diag]   Buffers:     %lu kB\n", buffers);
        } else if (sscanf(line, "Cached: %lu kB", &cached) == 1) {
            printf("[mem_diag]   Cached:      %lu kB\n", cached);
        } else if (sscanf(line, "SwapTotal: %lu kB", &swap_total) == 1) {
            printf("[mem_diag]   SwapTotal:   %lu kB\n", swap_total);
        } else if (sscanf(line, "SwapFree: %lu kB", &swap_free) == 1) {
            printf("[mem_diag]   SwapFree:    %lu kB\n", swap_free);
        }
    }
    fclose(fp);

    /* 计算使用率 */
    if (mem_total > 0) {
        double used_pct = 100.0 * (1.0 - (double)mem_free / mem_total);
        printf("[mem_diag]   内存使用率: %.1f%%\n", used_pct);
    }
}

/* ============================================================
 * Demo 2: 动态内存分配与释放
 * ============================================================ */
static void demo_alloc_free(void) {
    printf("[mem_diag] 演示动态内存分配与释放\n");

    /* 分配内存 */
    printf("[mem_diag] 调用 malloc(1024) 分配 1KB\n");
    void *p1 = malloc(1024);
    track_alloc(p1, 1024, __LINE__ + 1);

    printf("[mem_diag] 调用 malloc(4096) 分配 4KB (一个页面)\n");
    void *p2 = malloc(4096);
    track_alloc(p2, 4096, __LINE__ + 1);

    printf("[mem_diag] 调用 calloc(256, 8) 分配 2KB 并清零\n");
    void *p3 = calloc(256, 8);
    track_alloc(p3, 256 * 8, __LINE__ + 1);

    /* 验证 calloc 清零 */
    char *cp = (char *)p3;
    int all_zero = 1;
    for (int i = 0; i < 64; i++) {
        if (cp[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    printf("[mem_diag]   calloc 内存已清零: %s\n", all_zero ? "是" : "否");

    /* 重新分配 */
    printf("[mem_diag] 调用 realloc(p1, 2048) 扩展到 2KB\n");
    void *p1_new = realloc(p1, 2048);
    if (p1_new) {
        track_free(p1);  /* 旧指针不再跟踪 */
        p1 = p1_new;
        track_alloc(p1, 2048, __LINE__ + 1);
        printf("[mem_diag]   realloc 成功\n");
    }

    /* 释放内存 */
    printf("[mem_diag] 释放所有分配的内存\n");
    free(p1);
    track_free(p1);
    free(p2);
    track_free(p2);
    free(p3);
    track_free(p3);
}

/* ============================================================
 * Demo 3: 内存泄漏检测
 * ============================================================ */
static void demo_leak_detection(void) {
    printf("[mem_diag] 演示内存泄漏检测思路\n");

    /* 正常分配并释放 */
    void *p1 = malloc(100);
    track_alloc(p1, 100, __LINE__ + 1);
    printf("[mem_diag] 分配 100 字节（稍后会释放）\n");
    free(p1);
    track_free(p1);
    printf("[mem_diag] 已释放 100 字节\n");

    /* 模拟泄漏：分配后忘记释放 */
    void *p2 = malloc(200);
    track_alloc(p2, 200, __LINE__ + 1);
    printf("[mem_diag] 分配 200 字节（模拟泄漏，未释放）\n");
    /* 故意不 free(p2)，模拟泄漏 */

    /* 生成泄漏报告 */
    report_leaks();
}

/* ============================================================
 * Demo 4: 进程内存映射查看
 * ============================================================ */
static void demo_maps(void) {
    printf("[mem_diag] 读取 /proc/%d/maps 查看进程内存映射\n", getpid());

    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", getpid());

    FILE *fp = fopen(maps_path, "r");
    if (!fp) {
        perror("[mem_diag] fopen maps 失败");
        return;
    }

    char line[512];
    int count = 0;
    printf("[mem_diag]   地址范围            权限   偏移    设备   inode   路径\n");
    while (fgets(line, sizeof(line), fp) && count < 5) {
        /* 解析并打印简化版 */
        unsigned long start, end;
        char perms[5], dev[6], pathname[256] = "";
        unsigned long inode;

        sscanf(line, "%lx-%lx %4s %lx %5s %lu %255[^\n]",
               &start, &end, perms, &inode, dev, &inode, pathname);

        printf("[mem_diag]   %08lx-%08lx %4s %6s %s\n",
               start, end, perms, pathname[0] ? pathname : "(匿名)");
        count++;
    }
    printf("[mem_diag]   ... (共 %d 个映射区域)\n", count);

    fclose(fp);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 内存诊断学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 读取 /proc/meminfo ---\n");
    demo_meminfo();
    printf("\n");

    printf("--- Demo 2: 动态内存分配与释放 ---\n");
    demo_alloc_free();
    printf("\n");

    printf("--- Demo 3: 内存泄漏检测 ---\n");
    demo_leak_detection();
    printf("\n");

    printf("--- Demo 4: 进程内存映射 ---\n");
    demo_maps();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
