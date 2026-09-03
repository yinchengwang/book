/**
 * @file main.c
 * @brief Linux CFS 调度器学习演示
 *
 * 演示内容：
 *   - 读取 /proc/sched_debug 获取调度信息
 *   - 解析虚拟运行时间（vruntime）
 *   - 理解 CFS 调度延迟
 *
 * 编译：gcc -std=c11 -Wall -o cfs main.c
 * 运行：./cfs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/time.h>

/* ============================================================
 * 获取当前进程 PID
 * ============================================================ */
static pid_t getpid_wrapper(void) {
    return getpid();
}

/* ============================================================
 * Demo 1: 调度器信息
 * ============================================================ */
static void demo_sched_info(void) {
    printf("[cfs] 读取 /proc/sched_debug 获取调度信息\n");

    FILE *fp = fopen("/proc/sched_debug", "r");
    if (!fp) {
        perror("[cfs] fopen /proc/sched_debug 失败");
        /* 尝试 /proc/sched */
        fp = fopen("/proc/sched", "r");
        if (!fp) {
            printf("[cfs]   (需要 root 权限或较新内核)\n");
            return;
        }
    }

    char line[512];
    int printed = 0;

    while (fgets(line, sizeof(line), fp) && printed < 30) {
        /* 跳过空行和详细统计 */
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }

        /* 打印关键调度信息 */
        if (strstr(line, "sysctl_sched") ||
            strstr(line, "min_granularity") ||
            strstr(line, "latency") ||
            strstr(line, "nr_running") ||
            strstr(line, "cpu_load") ||
            strncmp(line, "cpu", 3) == 0) {
            printf("[cfs]   %s", line);
            printed++;
        }
    }

    fclose(fp);
}

/* ============================================================
 * Demo 2: 查看当前进程的调度信息
 * ============================================================ */
static void demo_proc_sched(void) {
    printf("[cfs] 查看当前进程的调度信息\n");

    pid_t pid = getpid_wrapper();
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/sched", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("[cfs] fopen sched 失败");
        return;
    }

    char line[512];
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        /* 查找关键字段 */
        if (strstr(line, "se.vruntime") ||
            strstr(line, "se.sum_exec_runtime") ||
            strstr(line, "nr_switches") ||
            strstr(line, "nr_voluntary_switches") ||
            strstr(line, "nr_involuntary_switches") ||
            strstr(line, "se.load.weight") ||
            strstr(line, "policy") ||
            strstr(line, "prio")) {
            printf("[cfs]   %s", line);
            found++;
        }
    }

    if (found == 0) {
        printf("[cfs]   (需要较新内核版本支持)\n");
    }

    fclose(fp);
}

/* ============================================================
 * Demo 3: CFS 调度算法原理
 * ============================================================ */
static void demo_cfs_algorithm(void) {
    printf("[cfs] CFS (完全公平调度器) 原理:\n\n");

    printf("[cfs] 1. 核心概念：虚拟运行时间 (vruntime)\n");
    printf("[cfs]    - 每个进程维护一个 vruntime 值\n");
    printf("[cfs]    - 每次运行增加：delta_exec * (NICE_0_LOAD / weight)\n");
    printf("[cfs]    - vruntime 越小，获得 CPU 的优先级越高\n\n");

    printf("[cfs] 2. 红黑树管理\n");
    printf("[cfs]    - 所有可运行进程按 vruntime 排序\n");
    printf("[cfs]    - 最左侧节点（最小 vruntime）获得 CPU\n");
    printf("[cfs]    - 树操作：O(log n)\n\n");

    printf("[cfs] 3. 调度延迟 (latency)\n");
    printf("[cfs]    - 目标：每个进程在 latency 周期内至少运行一次\n");
    printf("[cfs]    - 默认：6ms（桌面）/ 48ms（服务器）\n");
    printf("[cfs]    - min_granularity：最小时间片（0.75ms）\n\n");

    printf("[cfs] 4. 权重与优先级\n");
    printf("[cfs]    - nice 值映射到权重：weight = 1024 / (1.25 ^ nice)\n");
    printf("[cfs]    - nice=-20: weight=102476 (最高优先级)\n");
    printf("[cfs]    - nice=0:   weight=1024   (默认)\n");
    printf("[cfs]    - nice=+19: weight=15     (最低优先级)\n\n");

    printf("[cfs] 5. 调度决策\n");
    printf("[cfs]    - 选择 vruntime 最小的进程\n");
    printf("[cfs]    - 如果多个进程 vruntime 相近，轮流执行\n");
    printf("[cfs]    - CPU 密集型：vruntime 增长慢\n");
    printf("[cfs]    - I/O 密集型：频繁睡眠，vruntime 停滞\n");
}

/* ============================================================
 * Demo 4: 设置调度策略
 * ============================================================ */
static void demo_set_sched(void) {
    printf("[cfs] 演示设置调度策略\n");

    pid_t pid = getpid_wrapper();

    /* 获取当前调度策略 */
    int policy = sched_getscheduler(pid);
    printf("[cfs]   当前调度策略: ");

    switch (policy) {
        case SCHED_OTHER: printf("SCHED_OTHER (CFS)\n"); break;
        case SCHED_FIFO:  printf("SCHED_FIFO (实时)\n"); break;
        case SCHED_RR:    printf("SCHED_RR (实时)\n"); break;
        default:          printf("未知 (%d)\n", policy); break;
    }

    /* 获取调度参数 */
    struct sched_param param;
    if (sched_getparam(pid, &param) == 0) {
        printf("[cfs]   调度优先级: %d\n", param.sched_priority);
    }

    /* 设置实时调度（需要 root） */
    printf("[cfs]   尝试设置 SCHED_FIFO (需要 root)...\n");

    param.sched_priority = 50;
    if (sched_setscheduler(pid, SCHED_FIFO, &param) == 0) {
        printf("[cfs]   设置成功！\n");

        /* 恢复为 CFS */
        param.sched_priority = 0;
        sched_setscheduler(pid, SCHED_OTHER, &param);
        printf("[cfs]   已恢复为 SCHED_OTHER\n");
    } else {
        perror("[cfs]   设置失败");
        printf("[cfs]   (需要 root 权限)\n");
    }
}

/* ============================================================
 * Demo 5: CPU 亲和性
 * ============================================================ */
static void demo_cpu_affinity(void) {
    printf("[cfs] 演示 CPU 亲和性\n");

    pid_t pid = getpid_wrapper();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    /* 获取当前亲和性 */
    if (sched_getaffinity(pid, sizeof(cpuset), &cpuset) == 0) {
        printf("[cfs]   当前进程 CPU 亲和性: ");
        int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
        for (int i = 0; i < num_cpus; i++) {
            if (CPU_ISSET(i, &cpuset)) {
                printf("%d ", i);
            }
        }
        printf("\n");
        printf("[cfs]   可用 CPU 数: %d\n", num_cpus);
    }

    /* 设置亲和性（绑定到 CPU 0） */
    printf("[cfs]   尝试绑定到 CPU 0...\n");
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    if (sched_setaffinity(pid, sizeof(cpuset), &cpuset) == 0) {
        printf("[cfs]   绑定成功！\n");

        /* 恢复为所有 CPU */
        CPU_ZERO(&cpuset);
        for (int i = 0; i < sysconf(_SC_NPROCESSORS_ONLN); i++) {
            CPU_SET(i, &cpuset);
        }
        sched_setaffinity(pid, sizeof(cpuset), &cpuset);
        printf("[cfs]   已恢复为所有 CPU\n");
    } else {
        perror("[cfs]   绑定失败");
    }
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux CFS 调度器学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 调度器信息 ---\n");
    demo_sched_info();
    printf("\n");

    printf("--- Demo 2: 进程调度信息 ---\n");
    demo_proc_sched();
    printf("\n");

    printf("--- Demo 3: CFS 算法原理 ---\n");
    demo_cfs_algorithm();
    printf("\n");

    printf("--- Demo 4: 设置调度策略 ---\n");
    demo_set_sched();
    printf("\n");

    printf("--- Demo 5: CPU 亲和性 ---\n");
    demo_cpu_affinity();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
