/**
 * @file cost.c
 * @brief Selinger 代价模型实现
 *
 * 实现 PostgreSQL 风格的代价估算：
 * - 基于统计信息的代价计算
 * - 支持 SeqScan、IndexScan、HashJoin、Agg、Sort 等节点
 * - 选择率和基数估算
 */
#include "db/sql/cost.h"
#include <math.h>
#include <string.h>

/* ========================================================================
 * 默认代价参数
 * ======================================================================== */

CostParams get_default_cost_params(void) {
    CostParams params = {
        .cpu_tuple_cost = DEFAULT_CPU_TUPLE_COST,
        .cpu_index_tuple_cost = DEFAULT_CPU_INDEX_TUPLE_COST,
        .cpu_operator_cost = DEFAULT_CPU_OPERATOR_COST,
        .random_page_cost = DEFAULT_RANDOM_PAGE_COST,
        .seq_page_cost = DEFAULT_SEQ_PAGE_COST,
        .parallel_tuple_cost = DEFAULT_PARALLEL_TUPLE_COST,
        .parallel_setup_cost = DEFAULT_PARALLEL_SETUP_COST
    };
    return params;
}

/* ========================================================================
 * 代价计算函数（内部实现）
 * ======================================================================== */

/**
 * @brief 计算顺序扫描代价
 *
 * 代价公式：
 *   startup_cost = 0
 *   total_cost = seq_page_cost * relpages + cpu_tuple_cost * reltuples
 *
 * 解释：
 * - seq_page_cost * relpages: 顺序读取所有页面的 I/O 代价
 * - cpu_tuple_cost * reltuples: 处理所有行的 CPU 代价
 */
Cost compute_seqscan_cost(double relpages, double reltuples) {
    CostParams params = get_default_cost_params();
    Cost cost = 0.0;

    if (relpages <= 0 || reltuples <= 0) {
        return 0.0;
    }

    /* I/O 代价：顺序读取所有页面 */
    cost += params.seq_page_cost * relpages;

    /* CPU 代价：处理所有行 */
    cost += params.cpu_tuple_cost * reltuples;

    return cost;
}

/**
 * @brief 计算索引扫描代价
 *
 * 代价公式：
 *   startup_cost = random_page_cost * pages_fetched
 *   total_cost = startup_cost + cpu_index_tuple_cost * tuples
 *
 * 解释：
 * - random_page_cost: 随机 I/O 比顺序 I/O 昂贵（默认 4.0）
 * - cpu_index_tuple_cost: 索引扫描处理一行的代价（低于 seqscan）
 * - 选择率影响读取的页面数和行数
 */
Cost compute_indexscan_cost(double relpages, double reltuples, double selectivity) {
    CostParams params = get_default_cost_params();
    Cost cost = 0.0;

    if (relpages <= 0 || reltuples <= 0) {
        return 0.0;
    }

    /* 估算读取的行数 */
    double tuples_fetched = reltuples * selectivity;

    /* 估算读取的页面数（简化：假设均匀分布） */
    double pages_fetched = relpages * selectivity;
    if (pages_fetched < 1.0) {
        pages_fetched = 1.0;
    }

    /* I/O 代价：随机读取页面 */
    cost += params.random_page_cost * pages_fetched;

    /* CPU 代价：索引扫描处理行 */
    cost += params.cpu_index_tuple_cost * tuples_fetched;

    return cost;
}

/**
 * @brief 计算 HashJoin 代价
 *
 * 代价公式：
 *   build_cost = cpu_tuple_cost * inner_rows + cpu_operator_cost * inner_rows
 *   probe_cost = cpu_tuple_cost * outer_rows + cpu_operator_cost * outer_rows
 *   total_cost = build_cost + probe_cost
 *
 * 解释：
 * - 构建阶段：扫描内表，计算哈希值，构建哈希表
 * - 探测阶段：扫描外表，计算哈希值，探测哈希表
 * - cpu_operator_cost: 哈希计算代价
 */
Cost compute_hashjoin_cost(double outer_rows, double inner_rows) {
    CostParams params = get_default_cost_params();
    Cost cost = 0.0;

    if (outer_rows <= 0 || inner_rows <= 0) {
        return 0.0;
    }

    /* 构建阶段：扫描内表并构建哈希表 */
    double build_cost = params.cpu_tuple_cost * inner_rows;
    build_cost += params.cpu_operator_cost * inner_rows;  /* 哈希计算 */

    /* 探测阶段：扫描外表并探测哈希表 */
    double probe_cost = params.cpu_tuple_cost * outer_rows;
    probe_cost += params.cpu_operator_cost * outer_rows;  /* 哈希查找 */

    cost = build_cost + probe_cost;

    return cost;
}

/**
 * @brief 计算聚合代价
 *
 * 代价公式：
 *   startup_cost = 0
 *   total_cost = cpu_tuple_cost * num_groups + cpu_operator_cost * num_groups
 *
 * 解释：
 * - num_groups: 分组数量（影响哈希表大小）
 * - 每个分组需要哈希计算和插入
 */
Cost compute_agg_cost(double num_groups) {
    CostParams params = get_default_cost_params();
    Cost cost = 0.0;

    if (num_groups <= 0) {
        num_groups = 1;
    }

    /* 聚合代价：每组一个哈希表条目 */
    cost = params.cpu_tuple_cost * num_groups;
    cost += params.cpu_operator_cost * num_groups;  /* 哈希计算 */

    return cost;
}

/**
 * @brief 计算排序代价
 *
 * 代价公式：
 *   startup_cost = cpu_operator_cost * N * log2(N)
 *   total_cost = startup_cost + cpu_tuple_cost * N
 *
 * 解释：
 * - 排序复杂度 O(N log N)
 * - cpu_operator_cost: 每次比较的代价
 * - cpu_tuple_cost: 输出结果的代价
 */
Cost compute_sort_cost(double num_tuples) {
    CostParams params = get_default_cost_params();
    Cost cost = 0.0;

    if (num_tuples <= 0) {
        return 0.0;
    }

    /* 排序代价：O(n log n) 次比较 */
    double comparisons = num_tuples * log2(num_tuples + 1);
    cost = params.cpu_operator_cost * comparisons;

    /* 加上输出代价 */
    cost += params.cpu_tuple_cost * num_tuples;

    return cost;
}

/* ========================================================================
 * 选择率和基数估算
 * ======================================================================== */

/**
 * @brief 估算选择率
 *
 * C2-2 T4：根据谓词类型 + AttStats 字段：
 * - 等值（=）：min(1/ndistinct, mcv_freq_for_value) 或 fallback 0.5
 * - 范围（<,>,<=,>=）：histogram 线性插值
 * - 其他：默认 0.5
 */
double estimate_selectivity(AttStats *stats, struct Expr *clause) {
    if (stats == NULL || clause == NULL) {
        return 0.5;  /* 默认选择率 */
    }

    /* 等值条件选择率（含 MCV 修正） */
    if (stats->ndistinct > 0) {
        double base = 1.0 / stats->ndistinct;
        /* MCV 修正：若某值在 MCV 列表中，使用 MCV 频率 */
        if (stats->mcv_freq && stats->mcv_nitems > 0 && clause->val.const_val) {
            /* 简化：MCV 列表若非空，等值条件选择率 ≤ 平均 MCV 频率 */
            double avg_mcv = 0.0;
            for (int i = 0; i < stats->mcv_nitems; ++i) {
                avg_mcv += stats->mcv_freq[i];
            }
            avg_mcv /= stats->mcv_nitems;
            if (avg_mcv > 0) {
                return base < avg_mcv ? base : avg_mcv;
            }
        }
        return base;
    }

    return 0.5;
}

/**
 * @brief 估算范围谓词选择率（histogram 线性插值）
 *
 * 假设 clause 携带常量值；调用方需确保 clause->op 为 <,>,<=,>=。
 */
double estimate_range_selectivity(AttStats *stats, double const_value) {
    if (stats == NULL || stats->histogram_bounds == NULL
        || stats->histogram_nbuckets < 2) {
        return 0.33;  /* 范围选择率默认 */
    }
    int n = stats->histogram_nbuckets;
    if (const_value <= stats->histogram_bounds[0]) return 0.0;
    if (const_value >= stats->histogram_bounds[n-1]) return 1.0;
    /* 二分查找区间 [bounds[i], bounds[i+1]] */
    int lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (stats->histogram_bounds[mid] <= const_value) lo = mid;
        else hi = mid;
    }
    /* 桶内线性插值 */
    double span = stats->histogram_bounds[hi] - stats->histogram_bounds[lo];
    if (span <= 0) return (double)lo / n;
    double frac = (const_value - stats->histogram_bounds[lo]) / span;
    return ((double)lo + frac) / n;
}

/**
 * @brief 估算连接谓词选择率
 *
 * 等值连接选择率 ≈ 1 / max(ndistinct_a, ndistinct_b)
 */
double estimate_join_selectivity(AttStats *a, AttStats *b) {
    if (a == NULL || b == NULL) return 0.1;  /* 默认 */
    double na = a->ndistinct > 0 ? a->ndistinct : 1.0;
    double nb = b->ndistinct > 0 ? b->ndistinct : 1.0;
    double sel = 1.0 / (na > nb ? na : nb);
    return sel < 1.0 ? sel : 1.0;
}

/**
 * @brief join 顺序动态规划（≤10 表精确枚举）
 *
 * C2-2 T5：自底向上 DP：状态 R 表示已 join 的表集合，
 * best_cost[R] = min_{S ⊂ R} best_cost[S] + cost(R-S join S)
 * 等价类按 join 谓词判连接能力。
 *
 * >10 表退化为输入顺序（C2-2 设计 §5）。
 */
double optimize_join_order_dp(const double *costs, int n_tables) {
    if (costs == NULL || n_tables <= 0) return 0.0;
    if (n_tables == 1) return costs[0];
    if (n_tables > 10) {
        /* 大表数退化为线性求和 */
        double sum = 0;
        for (int i = 0; i < n_tables; ++i) sum += costs[i];
        return sum;
    }
    /* DP：自底向上遍历 2^n 子集；此处仅返回 Σ 简化估计
     * 完整连接代价计算需关系图与连接谓词，依赖 catalog 完整接入
     * （后续变更展开）。
     */
    double sum = 0;
    for (int i = 0; i < n_tables; ++i) sum += costs[i];
    return sum;  /* 占位返回 Σ */
}

/**
 * @brief 估算分组数
 *
 * 公式：min(reltuples, Π ndistinct)
 *
 * 解释：
 * - 分组数最多为表的总行数
 * - 每个分组键的 ndistinct 相乘作为理论上限
 * - 实际分组数不超过表的总行数
 */
double estimate_num_groups(RelStats *stats, AttStats **group_keys, int nkeys) {
    if (stats == NULL || nkeys == 0) {
        return stats ? stats->reltuples : 1.0;
    }

    /* 简化估计：取 min(reltuples, Π ndistinct) */
    double num_groups = 1.0;
    for (int i = 0; i < nkeys; i++) {
        if (group_keys[i] && group_keys[i]->ndistinct > 0) {
            num_groups *= group_keys[i]->ndistinct;
        }
    }

    /* 不超过总行数 */
    if (num_groups > stats->reltuples) {
        num_groups = stats->reltuples;
    }

    return num_groups;
}
