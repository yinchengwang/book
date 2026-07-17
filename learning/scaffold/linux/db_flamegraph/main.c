/**
 * @file main.c
 * @brief 数据库火焰图生成演示
 *
 * 演示 perf + FlameGraph 生成 CPU 火焰图
 * 展示数据库查询执行路径的热点可视化
 *
 * 编译：gcc -std=c11 -o db_flamegraph main.c
 * 运行：./db_flamegraph
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 模拟数据库查询执行的不同阶段 */
/* 阶段 1：SQL 解析 */
static void sim_sql_parse(void) {
    volatile int count = 0;
    for (int i = 0; i < 5000000; i++) {
        count++;
    }
}

/* 阶段 2：查询优化（热点 1）*/
static void sim_query_optimize(void) {
    volatile double val = 0.0;
    for (int i = 0; i < 8000000; i++) {
        val += i * 0.01;
    }
}

/* 阶段 3：索引扫描（热点 2）*/
static void sim_index_scan(void) {
    volatile int sum = 0;
    for (int i = 0; i < 15000000; i++) {
        sum += i % 1000;
    }
}

/* 阶段 4：条件过滤 */
static void sim_qual_filter(void) {
    volatile int matches = 0;
    for (int i = 0; i < 3000000; i++) {
        if (i % 3 == 0 && i % 7 != 0) {
            matches++;
        }
    }
}

/* 阶段 5：结果排序（热点 3）*/
static void sim_result_sort(void) {
    volatile int arr_size = 10000;
    volatile int data[10000];
    for (int i = 0; i < arr_size; i++) {
        data[i] = arr_size - i;
    }
    /* 冒泡排序（故意 O(n²) 制造热点）*/
    for (int i = 0; i < arr_size; i++) {
        for (int j = 0; j < arr_size - i - 1; j++) {
            if (data[j] > data[j + 1]) {
                volatile int tmp = data[j];
                data[j] = data[j + 1];
                data[j + 1] = tmp;
            }
        }
    }
}

/* 执行完整查询 */
static void execute_full_query(int iteration) {
    /* 解析 */
    sim_sql_parse();

    /* 优化 */
    sim_query_optimize();

    /* 索引扫描 */
    for (int i = 0; i < 3; i++) {
        sim_index_scan();
    }

    /* 过滤 */
    sim_qual_filter();

    /* 排序 */
    if (iteration % 2 == 0) {
        sim_result_sort();  /* 热点：只在偶数轮排序 */
    }
}

int main(void) {
    printf("===========================================\n");
    printf("  数据库火焰图生成演示\n");
    printf("===========================================\n");

    /* 1. FlameGraph 流程说明 */
    printf("\n\n=== 1. FlameGraph 生成流程 ===\n");
    printf(
        "火焰图 (FlameGraph) 由 Brendan Gregg 发明，\n"
        "用于可视化 CPU 采样栈，快速定位性能热点。\n\n"
        "生成流程:\n"
        "  1. perf record -F 99 -g -- <program>\n"
        "  2. perf script > out.perf\n"
        "  3. stackcollapse-perf.pl out.perf > out.folded\n"
        "  4. flamegraph.pl out.folded > flamegraph.svg\n"
    );

    /* 2. 执行查询工作负载（产生不同深度的调用栈）*/
    printf("\n\n=== 2. 执行查询工作负载 ===\n");
    printf("[说明] 执行 5 轮查询，制造 CPU 热点供 perf 采样\n\n");

    for (int round = 0; round < 5; round++) {
        printf("  --- 查询 %d ---\n", round + 1);
        execute_full_query(round + 1);
    }

    printf("\n[说明] 工作负载完成，perf 可捕获以下热点:\n");
    printf("  - sim_index_scan()  — 索引扫描（最多 CPU）\n");
    printf("  - sim_result_sort() — 排序（O(n²) 热点）\n");
    printf("  - sim_query_optimize() — 查询优化\n");

    /* 3. perf 采样命令 */
    printf("\n\n=== 3. perf 采样命令 ===\n");

    printf("\n# 方式 1: 采样本程序\n");
    printf("perf record -F 99 -g -o perf_db.data ./db_flamegraph\n\n");

    printf("# 方式 2: 采样后直接生成火焰图\n");
    printf("perf script -i perf_db.data | \\\n");
    printf("  FlameGraph/stackcollapse-perf.pl | \\\n");
    printf("  FlameGraph/flamegraph.pl > db_flamegraph.svg\n\n");

    printf("# 方式 3: 采样后台数据库进程\n");
    printf("perf record -F 99 -g -p <db_server_pid> sleep 30\n");

    /* 4. FlameGraph 参数说明 */
    printf("\n\n=== 4. FlameGraph 参数速查 ===\n");
    printf("  --colors <scheme>   颜色方案 (hot/io/java/perl/js/mem/green/blue/aqua/orange/red)\n");
    printf("  --title <text>      图表标题\n");
    printf("  --inverted          反转（icicle 图表）\n");
    printf("  --reverse           反转栈顺序\n");
    printf("  --width <pixels>    宽度（默认 1200）\n");
    printf("  --minwidth <pct>    最小宽度阈值（默认 0.1）\n");

    /* 5. 火焰图阅读方法 */
    printf("\n\n=== 5. 火焰图阅读方法 ===\n");
    printf(
        "\n火焰图自底向上表示调用栈:\n"
        "  - X 轴：字母顺序排列，宽度表示比例\n"
        "  - Y 轴：调用栈深度\n"
        "  - 颜色：随机，帮助区分栈块\n"
        "  - 热点：平坦的顶部区域表示高 CPU 占比\n\n"
        "常见优化决策:\n"
        "  - 宽栈顶 = 该函数的 CPU 占比高 → 优化目标\n"
        "  - 多个窄栈 = 分散的 CPU 开销 → 可能不需要优化\n"
        "  - 深层栈 = 函数调用链长 → 考虑减少层次\n"
    );

    /* 6. 冷火焰图 */
    printf("\n\n=== 6. 冷火焰图（Off-CPU 分析）===\n");
    printf(
        "\n常规火焰图分析 CPU 时间（在哪忙），\n"
        "冷火焰图分析等待时间（在哪等）：\n\n"
        "生成: perf record -e sched:sched_switch -g -a sleep 10\n"
        "适用: 分析锁等待、I/O 等待、调度延迟\n"
    );

    printf("\n===========================================\n");
    printf("  数据库火焰图演示完成\n");
    printf("===========================================\n");

    return 0;
}
