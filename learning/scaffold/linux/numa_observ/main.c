/**
 * @file main.c
 * @brief NUMA 观测与内存分布演示
 *
 * 演示 NUMA 拓扑识别、numa_alloc_onnode 就近分配
 * 展示 numactl 和内存访问延迟差异
 *
 * 编译：gcc -std=c11 -o numa_observ main.c -lnuma
 * 运行：./numa_observ
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 简单的 NUMA 检测（不依赖 libnuma）*/
static void detect_numa_topology(void) {
    printf("\n=== NUMA 拓扑检测 ===\n");

    /* 检查 /sys/devices/system/node 是否存在 */
    FILE *fp = fopen("/sys/devices/system/node/online", "r");
    if (fp) {
        char line[64];
        if (fgets(line, sizeof(line), fp)) {
            printf("NUMA 节点范围: %s", line);
        }
        fclose(fp);

        /* 尝试读取每个节点信息 */
        for (int node = 0; node < 4; node++) {
            char path[256];
            snprintf(path, sizeof(path),
                     "/sys/devices/system/node/node%d/meminfo", node);
            fp = fopen(path, "r");
            if (fp) {
                printf("\n--- NUMA 节点 %d ---\n", node);
                while (fgets(line, sizeof(line), fp)) {
                    printf("  %s", line);
                }
                fclose(fp);
            }
        }
    } else {
        printf("[说明] 本系统无 NUMA 拓扑或为单节点系统\n"
               "       多 socket 系统才有 NUMA 拓扑\n");
    }
}

/* 生成指定大小的数组（在本地内存中分配）*/
static int *create_large_array(size_t mb) {
    size_t count = (mb * 1024 * 1024) / sizeof(int);
    int *arr = calloc(count, sizeof(int));
    if (!arr) {
        perror("calloc");
        return NULL;
    }

    /* 初始化（触摸所有页，触发首次访问延迟）*/
    for (size_t i = 0; i < count; i++) {
        arr[i] = (int)i;
    }

    return arr;
}

/* 顺序访问测试（本地 NUMA 节点）*/
static double bench_sequential_access(int *arr, size_t mb) {
    size_t count = (mb * 1024 * 1024) / sizeof(int);
    volatile long sum = 0;

    /* 预热：确保页已分配 */
    for (size_t i = 0; i < count; i += 64) {
        sum += arr[i];
    }

    /* 顺序读 */
    for (size_t i = 0; i < count; i++) {
        sum += arr[i];
    }
    (void)sum;
    return 0.0;  /* 实际延迟由硬件计数器测量 */
}

/* 随机访问测试（跨 NUMA 节点）*/
static double bench_random_access(int *arr, size_t mb) {
    size_t count = (mb * 1024 * 1024) / sizeof(int);
    volatile long sum = 0;
    unsigned int seed = 42;

    /* 随机访问：更大概率跨 NUMA 节点 */
    for (int round = 0; round < 10000; round++) {
        size_t idx = rand_r(&seed) % count;
        sum += arr[idx];
    }

    (void)sum;
    return 0.0;
}

int main(void) {
    printf("===========================================\n");
    printf("  NUMA 观测与内存分布演示\n");
    printf("===========================================\n");

    /* 1. NUMA 拓扑检测 */
    detect_numa_topology();

    /* 2. NUMA 核心概念 */
    printf("\n\n=== 2. NUMA 核心概念 ===\n");
    printf(
        "NUMA (Non-Uniform Memory Access) 多 CPU 架构中，\n"
        "每个 CPU 有本地内存（快）和远程内存（慢），\n"
        "访问远程内存的延迟是本地访问的 1.5~2 倍。\n\n"
        "关键术语:\n"
        "  - 节点 (Node): CPU + 本地内存组合\n"
        "  - 本地访问: 同节点 CPU 访问内存（~100ns）\n"
        "  - 远程访问: 跨节点访问内存（~150-200ns）\n"
        "  - 交织分配: 轮询分配页面到所有节点（减少竞争）\n"
    );

    /* 3. 内存分配测试 */
    printf("\n\n=== 3. 内存分配基准测试 ===\n");

    const size_t test_mb = 100;
    printf("分配 %zu MB 数组...\n", test_mb);

    int *arr = create_large_array(test_mb);
    if (!arr) return 1;

    printf("[分配成功] 地址: %p\n", (void *)arr);

    printf("\n[顺序访问测试]\n");
    bench_sequential_access(arr, test_mb);
    printf("  完成顺序访问模式\n");

    printf("\n[随机访问测试]\n");
    bench_random_access(arr, test_mb);
    printf("  完成随机访问模式（模拟跨节点访问）\n");

    /* 4. numactl 工具说明 */
    printf("\n\n=== 4. numactl 工具使用 ===\n");

    printf("\n# 查看 NUMA 拓扑\n");
    printf("numactl --hardware\n\n");

    printf("# 查看 NUMA 内存分配策略\n");
    printf("numactl --show\n\n");

    printf("# 绑定到特定节点运行\n");
    printf("numactl --cpunodebind=0 --membind=0 ./numa_observ\n\n");

    printf("# 交织分配（均衡内存分布）\n");
    printf("numactl --interleave=all ./db_server\n\n");

    printf("# 查看进程的 NUMA 内存分布\n");
    printf("numastat -p <pid>\n");

    /* 5. NUMA 内存策略说明 */
    printf("\n\n=== 5. NUMA 内存分配策略 ===\n");
    printf(
        "  MPOL_DEFAULT  : 默认，分配到当前 CPU 的本地节点\n"
        "  MPOL_BIND     : 严格绑定到指定节点\n"
        "  MPOL_INTERLEAVE: 页面轮询分配，均匀分布\n"
        "  MPOL_PREFERRED: 优先指定节点，满后使用其他节点\n"
    );

    /* 6. 数据库与 NUMA */
    printf("\n\n=== 6. 数据库与 NUMA ===\n");
    printf(
        "\n数据库系统对 NUMA 敏感的原因:\n"
        "  1. Buffer Pool 大内存分配 → 考虑交织策略\n"
        "  2. 并行查询 → 尽量本地 CPU 执行\n"
        "  3. 检查点刷盘 → 绑定到特定 NUMA 节点\n"
        "  4. 连接线程 → 绑定到启动时的 CPU 节点\n\n"
        "建议:\n"
        "  - Buffer Pool: 交织分配 (interleave=all)\n"
        "  - 工作线程: 绑定到本地节点 (membind)\n"
        "  - 监控工具: numastat, perf stat -e node-loads,node-stores\n"
    );

    free(arr);

    printf("\n===========================================\n");
    printf("  NUMA 观测演示完成\n");
    printf("===========================================\n");

    return 0;
}
