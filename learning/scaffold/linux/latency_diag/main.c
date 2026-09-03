/**
 * @file main.c
 * @brief 延迟诊断与瓶颈定位演示
 *
 * 演示延迟分解方法论，分析各阶段耗时组成
 * 展示数据库查询延迟诊断系统
 *
 * 编译：gcc -std=c11 -o latency_diag main.c
 * 运行：./latency_diag
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* 延迟阶段枚举 */
typedef enum {
    L_PARSE,        /* 解析 */
    L_PLAN,         /* 计划 */
    L_BUF_READ,     /* Buffer Pool 读 */
    L_DISK_READ,    /* 磁盘读 */
    L_EXEC,         /* 执行 */
    L_SORT,         /* 排序 */
    L_FORMAT,       /* 格式化 */
    L_NETWORK       /* 网络传输 */
} LatencyPhase;

/* 延迟统计项 */
typedef struct {
    const char *name;
    double total_us;
    int count;
    double min_us;
    double max_us;
} LatencySlot;

/* 延迟诊断系统 */
typedef struct {
    LatencySlot slots[8];
    struct timespec last_start;
    int current_phase;
} LatencyDiagnostic;

static LatencyDiagnostic g_diag;

/* 初始化延迟诊断 */
static void diag_init(LatencyDiagnostic *d) {
    memset(d, 0, sizeof(*d));
    const char *names[] = {
        "解析", "计划", "Buf-读", "磁盘-读",
        "执行", "排序", "格式化", "网络"
    };
    for (int i = 0; i < 8; i++) {
        d->slots[i].name = names[i];
        d->slots[i].min_us = 1e9;
    }
}

/* 开始测量 */
static void diag_begin(LatencyDiagnostic *d, LatencyPhase phase) {
    d->current_phase = phase;
    clock_gettime(CLOCK_MONOTONIC, &d->last_start);
}

/* 结束测量 */
static double diag_end(LatencyDiagnostic *d) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double elapsed_us = (now.tv_sec - d->last_start.tv_sec) * 1e6 +
                       (now.tv_nsec - d->last_start.tv_nsec) / 1e3;

    LatencySlot *s = &d->slots[d->current_phase];
    s->total_us += elapsed_us;
    s->count++;
    if (elapsed_us < s->min_us) s->min_us = elapsed_us;
    if (elapsed_us > s->max_us) s->max_us = elapsed_us;

    return elapsed_us;
}

/* 打印延迟分布 */
static void diag_print(const LatencyDiagnostic *d) {
    double grand_total = 0;
    for (int i = 0; i < 8; i++) {
        grand_total += d->slots[i].total_us;
    }

    printf("\n=== 延迟诊断报告 ===\n");
    printf("%-12s %8s %8s %10s %10s %8s\n",
           "阶段", "次数", "总耗时", "平均us", "占比%", "最大us");
    printf("%-12s %8s %8s %10s %10s %8s\n",
           "---", "---", "---", "-----", "----", "----");

    for (int i = 0; i < 8; i++) {
        const LatencySlot *s = &d->slots[i];
        if (s->count == 0) continue;

        double avg = s->total_us / s->count;
        double pct = grand_total > 0 ? 100.0 * s->total_us / grand_total : 0;

        printf("%-12s %8d %8.0fms %10.1f %9.1f%% %8.0fus\n",
               s->name, s->count,
               s->total_us / 1000.0,
               avg, pct, s->max_us);
    }

    printf("\n总耗时: %.2f ms\n", grand_total / 1000.0);

    /* 找出瓶颈 */
    double max_pct = 0;
    const char *bottleneck = NULL;
    for (int i = 0; i < 8; i++) {
        const LatencySlot *s = &d->slots[i];
        if (s->count == 0) continue;
        double pct = grand_total > 0 ? 100.0 * s->total_us / grand_total : 0;
        if (pct > max_pct) {
            max_pct = pct;
            bottleneck = s->name;
        }
    }

    if (bottleneck) {
        printf("主要瓶颈: %s (%.1f%% 总耗时)\n", bottleneck, max_pct);
    }
}

/* 模拟查询工作负载 */
static void sim_query_workload(int query_id) {
    /* 阶段 1: 解析 */
    diag_begin(&g_diag, L_PARSE);
    {
        volatile int x = 0;
        for (int i = 0; i < 500000; i++) x++;
    }
    diag_end(&g_diag);

    /* 阶段 2: 计划 */
    diag_begin(&g_diag, L_PLAN);
    {
        volatile int x = 0;
        for (int i = 0; i < 300000; i++) x++;
    }
    diag_end(&g_diag);

    /* 阶段 3: Buffer Pool 读 */
    diag_begin(&g_diag, L_BUF_READ);
    {
        volatile int x = 0;
        for (int i = 0; i < 1000000; i++) x++;
    }
    diag_end(&g_diag);

    /* 阶段 4: 磁盘读（模拟高延迟）*/
    diag_begin(&g_diag, L_DISK_READ);
    {
        /* 10% 概率模拟磁盘 I/O */
        if (query_id % 10 == 0) {
            usleep(5000);  /* 5ms 模拟磁盘延迟 */
        } else {
            usleep(100);   /* 0.1ms 缓存命中 */
        }
    }
    diag_end(&g_diag);

    /* 阶段 5: 执行 */
    diag_begin(&g_diag, L_EXEC);
    {
        volatile int x = 0;
        for (int i = 0; i < 2000000; i++) x++;
    }
    diag_end(&g_diag);

    /* 阶段 6: 排序 */
    diag_begin(&g_diag, L_SORT);
    {
        if (query_id % 3 == 0) {
            volatile int x = 0;
            for (int i = 0; i < 1500000; i++) x++;  /* 热点 */
        }
    }
    diag_end(&g_diag);
}

int main(void) {
    printf("===========================================\n");
    printf("  延迟诊断与瓶颈定位演示\n");
    printf("===========================================\n");

    diag_init(&g_diag);

    /* 1. 延迟分解方法论 */
    printf("\n=== 1. 延迟分解方法论 ===\n");
    printf(
        "USE 方法（Utilization, Saturation, Errors）用于定位瓶颈:\n"
        "  1. 利用率：资源忙碌时间的百分比\n"
        "  2. 饱和度：等待队列长度\n"
        "  3. 错误：错误计数\n\n"
        "RED 方法（Rate, Errors, Duration）用于服务监控:\n"
        "  1. 速率：每秒请求数\n"
        "  2. 错误：失败请求数\n"
        "  3. 延迟：请求处理时长\n\n"
        "延迟分解法：\n"
        "  将一次请求的总延迟分解为各阶段耗时，\n"
        "  找出占比最大的阶段作为优化目标。\n"
    );

    /* 2. 执行工作负载并诊断 */
    printf("\n=== 2. 查询工作负载诊断 ===\n");
    printf("执行 50 条模拟查询...\n");

    for (int q = 0; q < 50; q++) {
        sim_query_workload(q);
    }

    /* 3. 打印诊断报告 */
    diag_print(&g_diag);

    /* 4. 延迟优化建议 */
    printf("\n\n=== 3. 延迟优化建议 ===\n");
    printf(
        "\n常见延迟瓶颈与解决方案:\n"
        "  磁盘读:  启用 Buffer Pool / 加大缓存\n"
        "  执行:    SQL 优化 / 减少扫描行\n"
        "  排序:    索引 / 内存排序 / 减少排序列\n"
        "  网络:    连接池 / 数据压缩 / 批量返回\n"
        "  解析:    预编译语句 / 查询缓存\n"
    );

    /* 5. 延迟异常模式 */
    printf("\n\n=== 4. 延迟异常模式 ===\n");
    printf(
        "  P99 突增    → 周期性后台任务（检查点/VACUUM）\n"
        "  峰值偏移    → 新版本/索引变更导致的退化\n"
        "  周期性震荡  → 缓存淘汰/垃圾回收\n"
        "  尾延迟异常  → 少数慢请求拖慢整体\n"
    );

    /* 6. 生产环境诊断工具 */
    printf("\n\n=== 5. 生产环境延迟诊断工具 ===\n");
    printf(
        "  - Prometheus + Grafana: 时序延迟监控\n"
        "  - Jaeger / Zipkin: 分布式追踪\n"
        "  - perf 延迟采样: perf sched latency\n"
        "  - bpftrace: funclatency.bt\n"
        "  - eBPF 自定义追踪: 数据库内部追踪点\n"
    );

    printf("\n===========================================\n");
    printf("  延迟诊断演示完成\n");
    printf("===========================================\n");

    return 0;
}
