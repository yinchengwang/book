/**
 * @file optimizer_cost.c
 * @brief 查询优化器代价模型实现
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <db/optimizer_cost.h>

/* ============================================================
 * 全局代价参数
 * ============================================================ */

static scan_cost_params_t g_cost_params = {
    .page_fetch_cost = 1.0,
    .tuple_fetch_cost = 0.01,
    .cpu_tuple_cost = 0.01,
    .cpu_index_tuple_cost = 0.005,
    .cpu_operator_cost = 0.0025,
    .page_priority = 0.0
};

/* ============================================================
 * 代价模型配置实现
 * ============================================================ */

scan_cost_params_t cost_defaults_scan(void)
{
    scan_cost_params_t params = {
        .page_fetch_cost = 1.0,
        .tuple_fetch_cost = 0.01,
        .cpu_tuple_cost = 0.01,
        .cpu_index_tuple_cost = 0.005,
        .cpu_operator_cost = 0.0025,
        .page_priority = 0.0
    };
    return params;
}

scan_cost_params_t cost_defaults_join(void)
{
    return cost_defaults_scan();
}

void cost_set_params(const scan_cost_params_t *params)
{
    if (params != NULL) {
        g_cost_params = *params;
    }
}

void cost_reset_params(void)
{
    g_cost_params = cost_defaults_scan();
}

/* ============================================================
 * 辅助函数
 * ============================================================ */

static void cost_init(cost_estimate_t *cost)
{
    if (cost == NULL) return;

    cost->total_cost = 0.0;
    cost->startup_cost = 0.0;
    cost->total_cost_ex = 0.0;
    cost->disk_io_cost = 0.0;
    cost->cpu_cost = 0.0;
    cost->memory_cost = 0.0;
    cost->network_cost = 0.0;
    cost->rows_estimated = 0.0;
    cost->width_estimated = 0.0;
}

static void cost_add(cost_estimate_t *dest, const cost_estimate_t *src)
{
    if (dest == NULL || src == NULL) return;

    dest->total_cost += src->total_cost;
    dest->startup_cost += src->startup_cost;
    dest->total_cost_ex += src->total_cost_ex;
    dest->disk_io_cost += src->disk_io_cost;
    dest->cpu_cost += src->cpu_cost;
    dest->memory_cost += src->memory_cost;
    dest->network_cost += src->network_cost;
}

/* ============================================================
 * 扫描代价估算实现
 * ============================================================ */

void cost_seq_scan(cost_estimate_t *cost,
                   double pages,
                   double rows,
                   double tuple_width)
{
    cost_init(cost);

    if (pages <= 0 || rows <= 0) {
        return;
    }

    /* 顺序扫描代价 = 页面读取代价 + 行处理代价 */
    cost->disk_io_cost = pages * g_cost_params.page_fetch_cost;
    cost->cpu_cost = rows * g_cost_params.cpu_tuple_cost;

    cost->total_cost = cost->disk_io_cost + cost->cpu_cost;
    cost->rows_estimated = rows;
    cost->width_estimated = tuple_width;

    /* 启动代价为0（顺序扫描无需准备） */
    cost->startup_cost = 0.0;
    cost->total_cost_ex = cost->total_cost;
}

void cost_index_scan(cost_estimate_t *cost,
                     double index_pages,
                     double heap_pages,
                     double index_rows,
                     double heap_rows,
                     double index_selectivity,
                     double index_depth)
{
    cost_init(cost);

    if (index_pages <= 0 || heap_pages <= 0) {
        return;
    }

    /* 索引扫描代价 = 索引扫描代价 + Heap 扫描代价 */

    /* 索引扫描部分：
     * - 读取索引页面
     * - 遍历索引树
     * - 处理索引项
     */
    double index_io_cost = index_pages * g_cost_params.page_fetch_cost;
    double index_cpu_cost = index_rows * g_cost_params.cpu_index_tuple_cost;
    double tree_traverse_cost = index_depth * COST_CPU_ROW;

    /* Heap 扫描部分：
     * - 随机读取 Heap 页面
     * - 处理返回的行
     */
    double heap_pages_fetched = heap_pages * index_selectivity;
    double heap_io_cost = heap_pages_fetched * COST_PAGE_RANDOM_SCAN;
    double heap_cpu_cost = heap_rows * g_cost_params.cpu_tuple_cost;

    cost->disk_io_cost = index_io_cost + heap_io_cost;
    cost->cpu_cost = index_cpu_cost + heap_cpu_cost + tree_traverse_cost;

    cost->total_cost = cost->disk_io_cost + cost->cpu_cost;
    cost->rows_estimated = heap_rows;
    cost->startup_cost = tree_traverse_cost;
    cost->total_cost_ex = cost->total_cost;
}

void cost_bitmap_scan(cost_estimate_t *cost,
                      double index_pages,
                      double heap_pages,
                      double rows,
                      double index_selectivity)
{
    cost_init(cost);

    if (index_pages <= 0) {
        return;
    }

    /* 位图扫描代价 = 索引扫描 + 位图构建 + Heap 批量读取 */

    /* 索引扫描代价 */
    double index_io_cost = index_pages * g_cost_params.page_fetch_cost;
    double index_cpu_cost = index_pages * g_cost_params.cpu_index_tuple_cost;

    /* 位图构建代价（每页一个位） */
    double bitmap_cost = heap_pages * COST_CPU_ROW;

    /* Heap 批量读取（按位图顺序读取，减少随机I/O） */
    double heap_pages_fetched = heap_pages * index_selectivity;
    double heap_io_cost = heap_pages_fetched * COST_PAGE_SEQ_SCAN;
    double heap_cpu_cost = rows * g_cost_params.cpu_tuple_cost;

    cost->disk_io_cost = index_io_cost + heap_io_cost;
    cost->cpu_cost = index_cpu_cost + bitmap_cost + heap_cpu_cost;

    cost->total_cost = cost->disk_io_cost + cost->cpu_cost;
    cost->rows_estimated = rows;
    cost->startup_cost = index_io_cost;
    cost->total_cost_ex = cost->total_cost;
}

void cost_index_create(cost_estimate_t *cost,
                       double table_rows,
                       double key_width)
{
    cost_init(cost);

    if (table_rows <= 0) {
        return;
    }

    /* 索引创建代价 = 排序代价 + 树构建代价 */

    /* 排序代价（估算） */
    double log2_n = log(table_rows) / log(2.0);
    double sort_cost = table_rows * log2_n * COST_SORT_ROW;

    /* 树构建代价 */
    double build_cost = table_rows * COST_CPU_ROW;

    cost->cpu_cost = sort_cost + build_cost;
    cost->total_cost = cost->cpu_cost;
    cost->rows_estimated = table_rows;
    cost->startup_cost = 0.0;
    cost->total_cost_ex = cost->total_cost;
}

/* ============================================================
 * 连接代价估算实现
 * ============================================================ */

void cost_nested_loop(cost_estimate_t *cost,
                      double outer_rows,
                      double inner_rows,
                      double inner_pages,
                      double join_cost_per_row)
{
    cost_init(cost);

    if (outer_rows <= 0 || inner_rows <= 0) {
        return;
    }

    /* 嵌套循环连接代价：
     * - 外表扫描代价（假设已在cost中）
     * - 内表重复扫描代价：outer_rows * inner_pages * random_page_cost
     * - 连接条件检查代价：outer_rows * inner_rows * join_cost_per_row
     */

    double inner_scan_cost = outer_rows * inner_pages * COST_PAGE_RANDOM_SCAN;
    double join_cpu_cost = outer_rows * inner_rows * join_cost_per_row;

    cost->disk_io_cost = inner_scan_cost;
    cost->cpu_cost = join_cpu_cost;

    cost->total_cost = cost->disk_io_cost + cost->cpu_cost;
    cost->rows_estimated = outer_rows * inner_rows;
    cost->startup_cost = 0.0;
    cost->total_cost_ex = cost->total_cost;
}

void cost_hash_join(cost_estimate_t *cost,
                    double outer_rows,
                    double inner_rows,
                    double hash_buckets,
                    double build_pages,
                    double probe_pages)
{
    cost_init(cost);

    if (outer_rows <= 0 || inner_rows <= 0) {
        return;
    }

    /* 哈希连接代价：
     * - 构建侧：扫描内表 + 哈希计算 + 写入哈希表
     * - 探测侧：扫描外表 + 哈希计算 + 查找 + 连接
     */

    double build_scan_cost = build_pages * COST_PAGE_SEQ_SCAN;
    double build_hash_cost = inner_rows * COST_HASH;
    double build_cpu_cost = inner_rows * COST_CPU_ROW;

    double probe_scan_cost = probe_pages * COST_PAGE_SEQ_SCAN;
    double probe_hash_cost = outer_rows * COST_HASH;
    double probe_cpu_cost = outer_rows * COST_CPU_ROW;

    cost->disk_io_cost = build_scan_cost + probe_scan_cost;
    cost->cpu_cost = build_hash_cost + build_cpu_cost +
                     probe_hash_cost + probe_cpu_cost;

    cost->total_cost = cost->disk_io_cost + cost->cpu_cost;
    cost->rows_estimated = outer_rows * inner_rows;
    cost->startup_cost = build_scan_cost + build_hash_cost;
    cost->total_cost_ex = cost->total_cost;
}

void cost_merge_join(cost_estimate_t *cost,
                     double outer_rows,
                     double inner_rows,
                     double outer_pages,
                     double inner_pages,
                     bool sorted)
{
    cost_init(cost);

    if (outer_rows <= 0 || inner_rows <= 0) {
        return;
    }

    /* 归并连接代价：
     * - 如果未排序，需要排序代价
     * - 归并扫描代价
     * - 连接条件检查代价
     */

    double sort_cost = 0.0;

    if (!sorted) {
        /* 排序代价 */
        double outer_sort = outer_rows * log(outer_rows) / log(2.0) * COST_SORT_ROW;
        double inner_sort = inner_rows * log(inner_rows) / log(2.0) * COST_SORT_ROW;
        sort_cost = outer_sort + inner_sort;
    }

    double scan_cost = (outer_pages + inner_pages) * COST_PAGE_SEQ_SCAN;
    double join_cpu_cost = (outer_rows + inner_rows) * COST_COMPARE;

    cost->cpu_cost = sort_cost + join_cpu_cost;
    cost->disk_io_cost = scan_cost;

    cost->total_cost = cost->disk_io_cost + cost->cpu_cost;
    cost->rows_estimated = outer_rows + inner_rows;
    cost->startup_cost = sort_cost;
    cost->total_cost_ex = cost->total_cost;
}

/* ============================================================
 * 聚合代价估算实现
 * ============================================================ */

void cost_hash_agg(cost_estimate_t *cost,
                   double rows,
                   double groups,
                   double hash_width)
{
    cost_init(cost);

    if (rows <= 0) {
        return;
    }

    /* 哈希聚合代价：
     * - 扫描输入 + 哈希计算 + 聚合
     * - 如果组数多，可能需要溢写
     */

    double scan_cost = rows * COST_CPU_ROW;
    double hash_cost = rows * COST_HASH;
    double agg_cost = rows * COST_CPU_EXPR;

    /* 估算溢写代价（组数/内存比例） */
    double overflow_ratio = groups / 1000.0;
    double overflow_cost = rows * overflow_ratio * 0.01;

    cost->cpu_cost = scan_cost + hash_cost + agg_cost + overflow_cost;
    cost->total_cost = cost->cpu_cost;
    cost->rows_estimated = groups > 0 ? groups : 1;
    cost->startup_cost = 0.0;
    cost->total_cost_ex = cost->total_cost;
}

void cost_sort_agg(cost_estimate_t *cost,
                   double rows,
                   double groups,
                   double tuple_width)
{
    cost_init(cost);

    if (rows <= 0) {
        return;
    }

    /* 排序聚合代价：
     * - 排序代价
     * - 扫描聚合代价
     */

    double sort_cost = rows * log(rows) / log(2.0) * COST_SORT_ROW;
    double scan_cost = rows * COST_CPU_ROW;
    double agg_cost = rows * COST_CPU_EXPR;

    cost->cpu_cost = sort_cost + scan_cost + agg_cost;
    cost->total_cost = cost->cpu_cost;
    cost->rows_estimated = groups > 0 ? groups : 1;
    cost->startup_cost = sort_cost;
    cost->total_cost_ex = cost->total_cost;
}

/* ============================================================
 * 排序代价估算实现
 * ============================================================ */

void cost_sort(cost_estimate_t *cost,
               double rows,
               double tuple_width,
               size_t work_mem,
               bool enable_sort)
{
    cost_init(cost);

    if (rows <= 0) {
        return;
    }

    if (!enable_sort) {
        /* 不排序的代价（简单扫描） */
        cost->cpu_cost = rows * COST_CPU_ROW;
        cost->total_cost = cost->cpu_cost;
        cost->rows_estimated = rows;
        return;
    }

    /* 排序代价估算 */

    double log_n = log(rows) / log(2.0);

    /* 内存内排序代价 */
    double passes = 1.0;
    if (work_mem > 0) {
        double mem_sort_capacity = work_mem / tuple_width;
        if (mem_sort_capacity < rows) {
            passes = ceil(rows / mem_sort_capacity);
        }
    }

    double sort_cpu = rows * log_n * COST_SORT_ROW * passes;
    double io_cost = 0.0;

    if (passes > 1) {
        /* 需要溢写，外排 */
        io_cost = rows * passes * g_cost_params.page_fetch_cost * 0.1;
    }

    cost->cpu_cost = sort_cpu;
    cost->disk_io_cost = io_cost;
    cost->total_cost = cost->cpu_cost + cost->disk_io_cost;
    cost->rows_estimated = rows;
    cost->startup_cost = 0.0;
    cost->total_cost_ex = cost->total_cost;
}

/* ============================================================
 * 工具函数实现
 * ============================================================ */

int cost_compare(const cost_estimate_t *a, const cost_estimate_t *b)
{
    if (a == NULL || b == NULL) {
        return (a != NULL) - (b != NULL);
    }

    if (a->total_cost < b->total_cost) return -1;
    if (a->total_cost > b->total_cost) return 1;
    return 0;
}

void cost_print(const cost_estimate_t *cost, const char *label)
{
    if (cost == NULL) {
        printf("Cost: NULL\n");
        return;
    }

    printf("=== Cost Estimate: %s ===\n", label ? label : "");
    printf("  Total Cost:      %.2f\n", cost->total_cost);
    printf("  Startup Cost:    %.2f\n", cost->startup_cost);
    printf("  Disk I/O Cost:   %.2f\n", cost->disk_io_cost);
    printf("  CPU Cost:        %.2f\n", cost->cpu_cost);
    printf("  Memory Cost:     %.2f\n", cost->memory_cost);
    printf("  Rows Estimated:  %.0f\n", cost->rows_estimated);
    printf("  Width Estimated: %.0f\n", cost->width_estimated);
    printf("========================\n");
}
