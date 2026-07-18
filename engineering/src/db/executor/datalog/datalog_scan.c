/**
 * @file datalog_scan.c
 * @brief Datalog 扫描算子实现
 *
 * 实现半朴素求值（Semi-Naive Evaluation）的 Datalog 推理引擎，
 * 遵循 Volcano 迭代器协议（init / next / close）。
 *
 * 返回 2 列：predicate (const char*), arg0 (int64)
 */
#include "db/executor/datalog/datalog_scan.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * Datalog 内部数据结构
 * ======================================================================== */

/** 原子（谓词 + 参数） */
typedef struct datalog_atom_s {
    char     predicate[64];     /**< 谓词名 */
    uint32_t num_args;          /**< 参数数量 */
    int64_t  args[8];           /**< 参数值（常量或变量绑定） */
    bool     is_variable[8];    /**< 是否为变量 */
    char     var_names[8][32];  /**< 变量名 */
} datalog_atom_t;

/** 规则 */
typedef struct datalog_rule_s {
    datalog_atom_t  head;           /**< 规则头 */
    datalog_atom_t *body;           /**< 规则体（原子数组） */
    uint32_t        num_body_atoms; /**< 体原子数量 */
} datalog_rule_t;

/** 规则集合 */
typedef struct datalog_rule_set_s {
    datalog_rule_t *rules;
    uint32_t        num_rules;
} datalog_rule_set_t;

/** 事实 */
typedef struct datalog_fact_s {
    char     predicate[64];   /**< 谓词名 */
    int64_t  args[8];         /**< 参数值 */
    uint32_t num_args;        /**< 参数数量 */
    bool     derived;         /**< 是否为推导事实 */
} datalog_fact_t;

/** 事实集合 */
typedef struct datalog_fact_set_s {
    datalog_fact_t *facts;
    uint32_t        num_facts;
    uint32_t        capacity;
} datalog_fact_set_t;

/** Datalog 扫描内部状态 */
typedef struct datalog_scan_internal_s {
    datalog_rule_set_t *rule_set;       /**< 规则集合 */
    datalog_fact_set_t *edb;            /**< 外延数据库（事实） */
    datalog_fact_set_t *idb;            /**< 内涵数据库（推导结果） */
    datalog_fact_t     *current_facts;  /**< 当前结果集 */
    uint32_t            num_results;    /**< 结果数量 */
    uint32_t            current_index;  /**< 当前索引 */
    bool                done;           /**< 是否完成 */
    bool                evaluated;      /**< 是否已求值 */
    int                 max_iter;       /**< 最大迭代次数（0=默认1000） */
} datalog_scan_internal_t;

/* ========================================================================
 * 事实集合操作
 * ======================================================================== */

/** 创建事实集合 */
static datalog_fact_set_t *fact_set_create(uint32_t initial_capacity)
{
    datalog_fact_set_t *set = (datalog_fact_set_t *)calloc(1, sizeof(datalog_fact_set_t));
    if (set == NULL) return NULL;

    set->capacity = (initial_capacity > 0) ? initial_capacity : 16;
    set->facts = (datalog_fact_t *)calloc(set->capacity, sizeof(datalog_fact_t));
    if (set->facts == NULL) {
        free(set);
        return NULL;
    }
    set->num_facts = 0;
    return set;
}

/** 销毁事实集合 */
static void fact_set_destroy(datalog_fact_set_t *set)
{
    if (set == NULL) return;
    free(set->facts);
    free(set);
}

/** 检查事实是否已存在于集合中 */
static bool fact_set_contains(datalog_fact_set_t *set, const char *predicate,
                               const int64_t *args, uint32_t num_args)
{
    for (uint32_t i = 0; i < set->num_facts; i++) {
        datalog_fact_t *f = &set->facts[i];
        if (strcmp(f->predicate, predicate) != 0) continue;
        if (f->num_args != num_args) continue;
        bool match = true;
        for (uint32_t j = 0; j < num_args; j++) {
            if (f->args[j] != args[j]) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

/** 向事实集合添加事实（去重） */
static bool fact_set_add(datalog_fact_set_t *set, const char *predicate,
                          const int64_t *args, uint32_t num_args, bool derived)
{
    /* 去重检查 */
    if (fact_set_contains(set, predicate, args, num_args)) return false;

    /* 容量扩展 */
    if (set->num_facts >= set->capacity) {
        uint32_t new_cap = set->capacity * 2;
        datalog_fact_t *new_facts = (datalog_fact_t *)realloc(
            set->facts, new_cap * sizeof(datalog_fact_t));
        if (new_facts == NULL) return false;
        set->facts = new_facts;
        set->capacity = new_cap;
    }

    /* 添加事实 */
    datalog_fact_t *f = &set->facts[set->num_facts];
    strncpy(f->predicate, predicate, sizeof(f->predicate) - 1);
    f->predicate[sizeof(f->predicate) - 1] = '\0';
    memcpy(f->args, args, num_args * sizeof(int64_t));
    f->num_args = num_args;
    f->derived = derived;
    set->num_facts++;
    return true;
}

/* ========================================================================
 * 变量绑定操作
 * ======================================================================== */

#define MAX_VARS 16

/** 变量绑定表 */
typedef struct var_binding_s {
    char    name[32];       /**< 变量名 */
    int64_t value;          /**< 绑定值 */
    bool    bound;          /**< 是否已绑定 */
} var_binding_t;

/** 查找变量索引，不存在则创建 */
static int find_or_create_var(var_binding_t *bindings, int *num_vars,
                               const char *var_name)
{
    for (int i = 0; i < *num_vars; i++) {
        if (strcmp(bindings[i].name, var_name) == 0) return i;
    }
    /* 创建新绑定 */
    if (*num_vars >= MAX_VARS) return -1;
    int idx = (*num_vars)++;
    strncpy(bindings[idx].name, var_name, sizeof(bindings[idx].name) - 1);
    bindings[idx].name[sizeof(bindings[idx].name) - 1] = '\0';
    bindings[idx].bound = false;
    bindings[idx].value = 0;
    return idx;
}

/** 匹配原子到事实，更新绑定 */
static bool match_atom_to_fact(datalog_atom_t *atom, datalog_fact_t *fact,
                                var_binding_t *bindings, int *num_vars)
{
    if (strcmp(atom->predicate, fact->predicate) != 0) return false;
    if (atom->num_args != fact->num_args) return false;

    for (uint32_t i = 0; i < atom->num_args; i++) {
        if (atom->is_variable[i]) {
            /* 变量：查找或创建绑定 */
            int var_idx = find_or_create_var(bindings, num_vars, atom->var_names[i]);
            if (var_idx < 0) return false;

            if (bindings[var_idx].bound) {
                /* 已绑定，检查一致性 */
                if (bindings[var_idx].value != fact->args[i]) return false;
            } else {
                /* 首次绑定 */
                bindings[var_idx].bound = true;
                bindings[var_idx].value = fact->args[i];
            }
        } else {
            /* 常量：直接比较 */
            if (atom->args[i] != fact->args[i]) return false;
        }
    }
    return true;
}

/* ========================================================================
 * 规则体匹配（递归回溯）
 * ======================================================================== */

/**
 * 递归匹配规则体中的所有原子到事实集
 * @param rule         规则
 * @param atom_idx     当前匹配的体原子索引
 * @param delta        delta 事实集（本轮新增）
 * @param idb          内涵数据库
 * @param bindings     变量绑定表
 * @param num_vars     变量数量
 * @param results      结果事实集合
 * @param head         规则头原子（用于生成结果）
 * @param must_use_delta 必须使用 delta 中至少一个事实（半朴素核心约束）
 * @param used_delta   是否已使用 delta 中的事实
 */
static void match_body_recursive(datalog_rule_t *rule, uint32_t atom_idx,
                                  datalog_fact_set_t *delta,
                                  datalog_fact_set_t *idb,
                                  var_binding_t *bindings, int *num_vars,
                                  datalog_fact_set_t *results,
                                  datalog_atom_t *head,
                                  bool must_use_delta, bool used_delta)
{
    /* 所有体原子匹配完成，生成头事实 */
    if (atom_idx >= rule->num_body_atoms) {
        /* 半朴素约束：至少使用一个 delta 事实 */
        if (must_use_delta && !used_delta) return;

        /* 从绑定生成头事实的参数 */
        int64_t head_args[8];
        for (uint32_t i = 0; i < head->num_args; i++) {
            if (head->is_variable[i]) {
                int var_idx = find_or_create_var(bindings, num_vars, head->var_names[i]);
                if (var_idx < 0 || !bindings[var_idx].bound) return;
                head_args[i] = bindings[var_idx].value;
            } else {
                head_args[i] = head->args[i];
            }
        }
        fact_set_add(results, head->predicate, head_args, head->num_args, true);
        return;
    }

    datalog_atom_t *atom = &rule->body[atom_idx];

    /* 保存当前绑定状态以便回溯 */
    var_binding_t saved_bindings[MAX_VARS];
    int saved_num_vars = *num_vars;
    memcpy(saved_bindings, bindings, sizeof(var_binding_t) * MAX_VARS);

    /* 尝试匹配 delta 中的事实 */
    for (uint32_t f = 0; f < delta->num_facts; f++) {
        if (strcmp(atom->predicate, delta->facts[f].predicate) != 0) continue;
        if (atom->num_args != delta->facts[f].num_args) continue;

        /* 恢复绑定状态 */
        memcpy(bindings, saved_bindings, sizeof(var_binding_t) * MAX_VARS);
        *num_vars = saved_num_vars;

        if (match_atom_to_fact(atom, &delta->facts[f], bindings, num_vars)) {
            match_body_recursive(rule, atom_idx + 1, delta, idb,
                                  bindings, num_vars, results, head,
                                  must_use_delta, true);
        }
    }

    /* 尝试匹配 idb 中的事实（非 delta 部分） */
    for (uint32_t f = 0; f < idb->num_facts; f++) {
        if (strcmp(atom->predicate, idb->facts[f].predicate) != 0) continue;
        if (atom->num_args != idb->facts[f].num_args) continue;

        /* 恢复绑定状态 */
        memcpy(bindings, saved_bindings, sizeof(var_binding_t) * MAX_VARS);
        *num_vars = saved_num_vars;

        if (match_atom_to_fact(atom, &idb->facts[f], bindings, num_vars)) {
            match_body_recursive(rule, atom_idx + 1, delta, idb,
                                  bindings, num_vars, results, head,
                                  must_use_delta, used_delta);
        }
    }
}

/* ========================================================================
 * 半朴素求值
 * ======================================================================== */

/**
 * 执行半朴素求值
 * @param rule_set 规则集合
 * @param edb      外延数据库（输入事实）
 * @param idb      内涵数据库（输出，推导结果）
 * @param max_iter 最大迭代次数（0 表示使用默认值 1000）
 * @param num_results 输出推导结果数量
 * @return 推导出的事实数组，调用者负责释放
 */
static datalog_fact_t *semi_naive_evaluate(datalog_rule_set_t *rule_set,
                                            datalog_fact_set_t *edb,
                                            datalog_fact_set_t *idb,
                                            int max_iter,
                                            uint32_t *num_results)
{
    *num_results = 0;

    if (rule_set == NULL || rule_set->num_rules == 0) return NULL;

    /* 初始化 IDB = EDB */
    for (uint32_t i = 0; i < edb->num_facts; i++) {
        fact_set_add(idb, edb->facts[i].predicate,
                     edb->facts[i].args, edb->facts[i].num_args, false);
    }

    /* 初始 delta = EDB */
    datalog_fact_set_t *delta = fact_set_create(edb->num_facts + 16);
    if (delta == NULL) return NULL;
    for (uint32_t i = 0; i < edb->num_facts; i++) {
        fact_set_add(delta, edb->facts[i].predicate,
                     edb->facts[i].args, edb->facts[i].num_args, false);
    }

    /* 迭代直到不动点 */
    int max_iterations = (max_iter > 0) ? max_iter : 1000;  /* 默认 1000，防止无限循环 */
    while (delta->num_facts > 0 && max_iterations-- > 0) {
        datalog_fact_set_t *new_facts = fact_set_create(32);
        if (new_facts == NULL) {
            fact_set_destroy(delta);
            return NULL;
        }

        /* 对每条规则求值 */
        for (uint32_t r = 0; r < rule_set->num_rules; r++) {
            datalog_rule_t *rule = &rule_set->rules[r];

            var_binding_t bindings[MAX_VARS];
            int num_vars = 0;
            memset(bindings, 0, sizeof(bindings));

            /* 半朴素：必须使用 delta 中的至少一个事实 */
            match_body_recursive(rule, 0, delta, idb,
                                  bindings, &num_vars,
                                  new_facts, &rule->head,
                                  true, false);
        }

        /* 新 delta = new_facts - idb（去重后真正新增的事实） */
        fact_set_destroy(delta);
        delta = fact_set_create(new_facts->num_facts + 16);
        if (delta == NULL) {
            fact_set_destroy(new_facts);
            return NULL;
        }

        for (uint32_t i = 0; i < new_facts->num_facts; i++) {
            datalog_fact_t *nf = &new_facts->facts[i];
            if (fact_set_add(idb, nf->predicate, nf->args, nf->num_args, true)) {
                /* 真正新增的事实加入 delta */
                fact_set_add(delta, nf->predicate, nf->args, nf->num_args, true);
            }
        }

        fact_set_destroy(new_facts);
    }

    fact_set_destroy(delta);

    /* 收集推导结果（仅 derived=true 的事实） */
    uint32_t derived_count = 0;
    for (uint32_t i = 0; i < idb->num_facts; i++) {
        if (idb->facts[i].derived) derived_count++;
    }

    if (derived_count == 0) return NULL;

    datalog_fact_t *results = (datalog_fact_t *)calloc(derived_count, sizeof(datalog_fact_t));
    if (results == NULL) return NULL;

    uint32_t idx = 0;
    for (uint32_t i = 0; i < idb->num_facts; i++) {
        if (idb->facts[i].derived) {
            memcpy(&results[idx], &idb->facts[i], sizeof(datalog_fact_t));
            idx++;
        }
    }
    *num_results = derived_count;
    return results;
}

/* ========================================================================
 * Volcano 算子接口
 * ======================================================================== */

DatalogScanState *exec_datalog_scan_init(PlanState *parent,
    void *rule_set, void *edb, void *idb, int max_iter)
{
    DatalogScanState *state = (DatalogScanState *)calloc(1, sizeof(DatalogScanState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_DOCUMENT_SCAN;  /* 复用已有类型，Datalog 专用类型后续添加 */
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->rule_set = rule_set;
    state->edb = edb;
    state->idb = idb;
    state->max_iter = max_iter;

    /* 分配内部状态 */
    datalog_scan_internal_t *internal = (datalog_scan_internal_t *)calloc(
        1, sizeof(datalog_scan_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }
    internal->rule_set = (datalog_rule_set_t *)rule_set;
    internal->edb = (datalog_fact_set_t *)edb;
    internal->idb = (datalog_fact_set_t *)idb;
    internal->current_facts = NULL;
    internal->num_results = 0;
    internal->current_index = 0;
    internal->done = false;
    internal->evaluated = false;
    internal->max_iter = max_iter;
    internal->max_iter = max_iter;
    state->ps.state = internal;

    return state;
}

TupleTableSlot *exec_datalog_scan_next(DatalogScanState *state)
{
    if (state == NULL) return NULL;

    datalog_scan_internal_t *internal = (datalog_scan_internal_t *)state->ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时执行半朴素求值 */
    if (!internal->evaluated) {
        internal->evaluated = true;
        internal->current_facts = semi_naive_evaluate(
            internal->rule_set, internal->edb, internal->idb,
            internal->max_iter,
            &internal->num_results);

        if (internal->current_facts == NULL || internal->num_results == 0) {
            internal->done = true;
            return NULL;
        }
    }

    /* 遍历结果 */
    if (internal->current_index >= internal->num_results) {
        internal->done = true;
        return NULL;
    }

    datalog_fact_t *fact = &internal->current_facts[internal->current_index];
    internal->current_index++;

    /* 创建元组描述符：2 列 — predicate(const char*), arg0(int64) */
    ExecTupleDesc *desc = exec_make_tuple_desc(2);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    /* 填充列值 */
    /* 注意：predicate 字符串指针指向 fact 内部，上层需自行拷贝 */
    slot->tts_values[0] = (void *)fact->predicate;
    slot->tts_isnull[0] = false;

    /* arg0 存为 int64 指针（需要静态存储，用事实内部地址） */
    slot->tts_values[1] = (void *)&fact->args[0];
    slot->tts_isnull[1] = false;

    slot->tts_nvalid = 2;

    return slot;
}

void exec_datalog_scan_close(DatalogScanState *state)
{
    if (state == NULL) return;

    datalog_scan_internal_t *internal = (datalog_scan_internal_t *)state->ps.state;
    if (internal) {
        if (internal->current_facts) {
            free(internal->current_facts);
        }
        free(internal);
    }
    free(state);
}
