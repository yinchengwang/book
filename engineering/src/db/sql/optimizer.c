/**
 * @file optimizer.c
 * @brief 查询优化规则实现
 *
 * 实现 8 条核心查询优化规则。
 * 当前为框架版本，验证规则应用流程。
 */
#include "db/sql/optimizer.h"
#include "db/sql/cost.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 默认配置
 * ======================================================================== */

/**
 * @brief 获取默认优化器配置
 *
 * 默认启用所有优化规则。
 */
OptimizerConfig get_default_optimizer_config(void) {
    OptimizerConfig config = {
        .enable_predicate_pushdown = true,
        .enable_projection_pushdown = true,
        .enable_join_order = true,
        .enable_constant_folding = true,
        .enable_subquery_unnest = true,
        .enable_agg_pushdown = true,
        .enable_sort_elimination = true,
        .enable_limit_pushdown = true
    };
    return config;
}

/* ========================================================================
 * 核心 API
 * ======================================================================== */

/**
 * @brief 优化查询计划
 *
 * 根据配置应用所有启用的优化规则。
 * 规则应用顺序：
 *   1. 常量折叠（先处理表达式）
 *   2. 谓词下推（减少数据量）
 *   3. 投影下推（减少数据量）
 *   4. 子查询展开（转换查询结构）
 *   5. 连接顺序优化（基于代价）
 *   6. 聚合下推（优化聚合）
 *   7. 排序优化（消除冗余）
 *   8. 限制下推（减少数据量）
 */
Plan *optimize_plan(Plan *plan, OptimizerConfig *config) {
    if (plan == NULL) {
        return NULL;
    }

    /* 使用默认配置 */
    OptimizerConfig default_config;
    if (config == NULL) {
        default_config = get_default_optimizer_config();
        config = &default_config;
    }

    /* 按顺序应用启用的优化规则 */
    if (config->enable_constant_folding) {
        plan = opt_constant_folding(plan);
    }

    if (config->enable_predicate_pushdown) {
        plan = opt_predicate_pushdown(plan);
    }

    if (config->enable_projection_pushdown) {
        plan = opt_projection_pushdown(plan);
    }

    if (config->enable_subquery_unnest) {
        plan = opt_subquery_unnest(plan);
    }

    if (config->enable_join_order) {
        plan = opt_join_order(plan);
    }

    if (config->enable_agg_pushdown) {
        plan = opt_agg_pushdown(plan);
    }

    if (config->enable_sort_elimination) {
        plan = opt_sort_elimination(plan);
    }

    if (config->enable_limit_pushdown) {
        plan = opt_limit_pushdown(plan);
    }

    return plan;
}

/**
 * @brief 应用单条优化规则
 */
Plan *apply_opt_rule(Plan *plan, OptRule rule) {
    if (plan == NULL) {
        return NULL;
    }

    switch (rule) {
        case OPT_RULE_PREDICATE_PUSHDOWN:
            return opt_predicate_pushdown(plan);
        case OPT_RULE_PROJECTION_PUSHDOWN:
            return opt_projection_pushdown(plan);
        case OPT_RULE_JOIN_ORDER:
            return opt_join_order(plan);
        case OPT_RULE_CONSTANT_FOLDING:
            return opt_constant_folding(plan);
        case OPT_RULE_SUBQUERY_UNNEST:
            return opt_subquery_unnest(plan);
        case OPT_RULE_AGG_PUSHDOWN:
            return opt_agg_pushdown(plan);
        case OPT_RULE_SORT_ELIMINATION:
            return opt_sort_elimination(plan);
        case OPT_RULE_LIMIT_PUSHDOWN:
            return opt_limit_pushdown(plan);
        default:
            return plan;
    }
}

/* ========================================================================
 * 各优化规则实现（框架版本）
 * ======================================================================== */

/**
 * @brief 谓词下推
 *
 * 将 WHERE 条件尽可能下推到扫描节点。
 * 当前为框架实现，返回原计划。
 */
Plan *opt_predicate_pushdown(Plan *plan) {
    if (plan == NULL) {
        return NULL;
    }

    /* 递归处理子树 */
    if (plan->lefttree) {
        plan->lefttree = opt_predicate_pushdown(plan->lefttree);
    }
    if (plan->righttree) {
        plan->righttree = opt_predicate_pushdown(plan->righttree);
    }

    /* TODO: 实现谓词下推逻辑
     * 1. 识别 Filter 节点
     * 2. 提取谓词条件
     * 3. 检查谓词是否可以下推（只引用一个表）
     * 4. 将谓词移动到 SeqScan/IndexScan 节点
     */

    return plan;
}

/**
 * @brief 投影下推
 *
 * 将 SELECT 列表尽可能下推到扫描节点。
 * 当前为框架实现，返回原计划。
 */
Plan *opt_projection_pushdown(Plan *plan) {
    if (plan == NULL) {
        return NULL;
    }

    /* 递归处理子树 */
    if (plan->lefttree) {
        plan->lefttree = opt_projection_pushdown(plan->lefttree);
    }
    if (plan->righttree) {
        plan->righttree = opt_projection_pushdown(plan->righttree);
    }

    /* TODO: 实现投影下推逻辑
     * 1. 分析上层节点需要的列
     * 2. 计算每个扫描节点需要的最小列集
     * 3. 修改扫描节点的目标列列表
     */

    return plan;
}

/**
 * @brief 连接顺序优化
 *
 * 基于代价模型选择最优连接顺序。
 * 当前为框架实现，返回原计划。
 */
Plan *opt_join_order(Plan *plan) {
    if (plan == NULL) {
        return NULL;
    }

    /* 递归处理子树 */
    if (plan->lefttree) {
        plan->lefttree = opt_join_order(plan->lefttree);
    }
    if (plan->righttree) {
        plan->righttree = opt_join_order(plan->righttree);
    }

    /* TODO: 实现连接顺序优化逻辑
     * 1. 识别 Join 节点
     * 2. 收集统计信息
     * 3. 使用动态规划枚举所有连接顺序
     * 4. 选择代价最小的顺序
     */

    return plan;
}

/**
 * @brief 常量折叠
 *
 * 编译期计算常量表达式。
 * 当前为框架实现，返回原计划。
 */
Plan *opt_constant_folding(Plan *plan) {
    if (plan == NULL) {
        return NULL;
    }

    /* 递归处理子树 */
    if (plan->lefttree) {
        plan->lefttree = opt_constant_folding(plan->lefttree);
    }
    if (plan->righttree) {
        plan->righttree = opt_constant_folding(plan->righttree);
    }

    /* TODO: 实现常量折叠逻辑
     * 1. 遍历表达式树
     * 2. 识别常量子表达式
     * 3. 计算常量值并替换
     */

    return plan;
}

/**
 * @brief 子查询展开
 *
 * 将相关子查询转换为连接操作。
 * 当前为框架实现，返回原计划。
 */
Plan *opt_subquery_unnest(Plan *plan) {
    if (plan == NULL) {
        return NULL;
    }

    /* 递归处理子树 */
    if (plan->lefttree) {
        plan->lefttree = opt_subquery_unnest(plan->lefttree);
    }
    if (plan->righttree) {
        plan->righttree = opt_subquery_unnest(plan->righttree);
    }

    /* TODO: 实现子查询展开逻辑
     * 1. 识别 SubLink/SubPlan 节点
     * 2. 分析子查询与外查询的相关性
     * 3. 将子查询转换为 Join
     */

    return plan;
}

/**
 * @brief 聚合下推
 *
 * 将聚合操作尽可能下推到扫描节点。
 * 当前为框架实现，返回原计划。
 */
Plan *opt_agg_pushdown(Plan *plan) {
    if (plan == NULL) {
        return NULL;
    }

    /* 递归处理子树 */
    if (plan->lefttree) {
        plan->lefttree = opt_agg_pushdown(plan->lefttree);
    }
    if (plan->righttree) {
        plan->righttree = opt_agg_pushdown(plan->righttree);
    }

    /* TODO: 实现聚合下推逻辑
     * 1. 识别 Agg 节点
     * 2. 检查聚合是否可以下推
     * 3. 创建部分聚合节点
     */

    return plan;
}

/**
 * @brief 排序优化
 *
 * 消除不必要的排序操作。
 * 当前为框架实现，返回原计划。
 */
Plan *opt_sort_elimination(Plan *plan) {
    if (plan == NULL) {
        return NULL;
    }

    /* 递归处理子树 */
    if (plan->lefttree) {
        plan->lefttree = opt_sort_elimination(plan->lefttree);
    }
    if (plan->righttree) {
        plan->righttree = opt_sort_elimination(plan->righttree);
    }

    /* TODO: 实现排序优化逻辑
     * 1. 识别 Sort 节点
     * 2. 检查下层节点是否已经提供有序数据
     * 3. 如果有序，消除 Sort 节点
     */

    return plan;
}

/**
 * @brief 限制下推
 *
 * 将 LIMIT 下推到扫描节点。
 * 当前为框架实现，返回原计划。
 */
Plan *opt_limit_pushdown(Plan *plan) {
    if (plan == NULL) {
        return NULL;
    }

    /* 递归处理子树 */
    if (plan->lefttree) {
        plan->lefttree = opt_limit_pushdown(plan->lefttree);
    }
    if (plan->righttree) {
        plan->righttree = opt_limit_pushdown(plan->righttree);
    }

    /* TODO: 实现限制下推逻辑
     * 1. 识别 Limit 节点
     * 2. 将限制值下推到扫描节点
     * 3. 扫描节点可以在获取足够行后停止
     */

    return plan;
}
