# Gap#7 索引框架增强实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现统一索引管理层、执行引擎集成、索引选择优化

**Architecture:** IndexManager (目录+生命周期) → IndexScanExec (索引扫描) → IndexSelector (代价选择)

**Tech Stack:** C 语言，CMake 构建，GTest 单元测试，pthread 线程

## Global Constraints

- 复用现有索引实现 (BTree/HNSW/IVF/Hash 等)
- 遵循现有代码风格 (extern "C"、命名下划线分隔)
- 所有新文件加入对应 CMakeLists.txt
- 与 Gap#3 Executor Framework 集成

---

### Task 1: IndexCatalog 索引目录

**Files:**
- Create: `engineering/include/db/index/index_catalog.h`
- Create: `engineering/src/db/index/index_catalog.c`
- Modify: `engineering/src/db/index/CMakeLists.txt`

**Interfaces:**
- Consumes: index_config_t (已有)
- Produces: index_catalog_t, index_entry_t, index_type_t

- [ ] **Step 1: 创建 index_catalog.h — 索引目录头文件**

```c
// engineering/include/db/index/index_catalog.h
#ifndef DB_INDEX_CATALOG_H
#define DB_INDEX_CATALOG_H

#include "index_config.h"
#include <pthread.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 索引类型 */
typedef enum {
    INDEX_TYPE_BTREE = 0,
    INDEX_TYPE_HASH,
    INDEX_TYPE_HNSW,
    INDEX_TYPE_IVF,
    INDEX_TYPE_FULLTEXT,
    INDEX_TYPE_GIN,
    INDEX_TYPE_COUNT
} index_type_t;

/* 索引状态 */
typedef enum {
    INDEX_STATE_BUILDING = 0,
    INDEX_STATE_READY,
    INDEX_STATE_DELETING
} index_state_t;

/* 索引名称最大长度 */
#define MAX_INDEX_NAME_LEN 128

/* 索引条目 */
typedef struct index_entry {
    int index_id;
    char name[MAX_INDEX_NAME_LEN];
    index_type_t type;
    index_state_t state;
    int table_id;
    int *columns;
    int column_count;
    void *index_impl;
    index_config_t config;
    time_t created_at;
    size_t size_bytes;
} index_entry_t;

/* 索引目录 */
typedef struct index_catalog {
    index_entry_t **entries;
    int capacity;
    int count;
    pthread_rwlock_t rwlock;
} index_catalog_t;

/**
 * @brief 创建索引目录
 */
index_catalog_t *index_catalog_create(int initial_capacity);

/**
 * @brief 销毁索引目录
 */
void index_catalog_destroy(index_catalog_t *catalog);

/**
 * @brief 添加索引条目
 */
int index_catalog_add(index_catalog_t *catalog, index_entry_t *entry);

/**
 * @brief 移除索引条目
 */
int index_catalog_remove(index_catalog_t *catalog, int index_id);

/**
 * @brief 获取索引条目
 */
index_entry_t *index_catalog_get(index_catalog_t *catalog, int index_id);

/**
 * @brief 按表获取所有索引
 */
int index_catalog_get_by_table(index_catalog_t *catalog,
                               int table_id,
                               index_entry_t **results,
                               int max_results);

/**
 * @brief 获取索引类型名称
 */
const char *index_type_to_string(index_type_t type);

/**
 * @brief 从字符串获取索引类型
 */
index_type_t index_type_from_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_CATALOG_H */
```

- [ ] **Step 2: 创建 index_catalog.c — 索引目录实现**

```c
// engineering/src/db/index/index_catalog.c
#include "db/index/index_catalog.h"
#include <stdlib.h>
#include <string.h>

index_catalog_t *index_catalog_create(int initial_capacity) {
    index_catalog_t *cat = calloc(1, sizeof(index_catalog_t));
    if (!cat) return NULL;

    cat->entries = calloc(initial_capacity, sizeof(index_entry_t *));
    if (!cat->entries) {
        free(cat);
        return NULL;
    }

    cat->capacity = initial_capacity;
    cat->count = 0;
    pthread_rwlock_init(&cat->rwlock, NULL);
    return cat;
}

void index_catalog_destroy(index_catalog_t *cat) {
    if (!cat) return;

    pthread_rwlock_wrlock(&cat->rwlock);
    for (int i = 0; i < cat->count; i++) {
        if (cat->entries[i]) {
            if (cat->entries[i]->columns) free(cat->entries[i]->columns);
            free(cat->entries[i]);
        }
    }
    free(cat->entries);
    pthread_rwlock_unlock(&cat->rwlock);
    pthread_rwlock_destroy(&cat->rwlock);
    free(cat);
}

int index_catalog_add(index_catalog_t *cat, index_entry_t *entry) {
    if (!cat || !entry) return -1;

    pthread_rwlock_wrlock(&cat->rwlock);

    if (cat->count >= cat->capacity) {
        int new_cap = cat->capacity * 2;
        index_entry_t **new_entries = realloc(cat->entries,
            new_cap * sizeof(index_entry_t *));
        if (!new_entries) {
            pthread_rwlock_unlock(&cat->rwlock);
            return -1;
        }
        cat->entries = new_entries;
        cat->capacity = new_cap;
    }

    cat->entries[cat->count++] = entry;
    pthread_rwlock_unlock(&cat->rwlock);
    return 0;
}

int index_catalog_remove(index_catalog_t *cat, int index_id) {
    if (!cat) return -1;

    pthread_rwlock_wrlock(&cat->rwlock);

    for (int i = 0; i < cat->count; i++) {
        if (cat->entries[i] && cat->entries[i]->index_id == index_id) {
            if (cat->entries[i]->columns) free(cat->entries[i]->columns);
            free(cat->entries[i]);
            // 移动数组元素
            for (int j = i; j < cat->count - 1; j++) {
                cat->entries[j] = cat->entries[j + 1];
            }
            cat->count--;
            pthread_rwlock_unlock(&cat->rwlock);
            return 0;
        }
    }

    pthread_rwlock_unlock(&cat->rwlock);
    return -1;
}

index_entry_t *index_catalog_get(index_catalog_t *cat, int index_id) {
    if (!cat) return NULL;

    pthread_rwlock_rdlock(&cat->rwlock);

    for (int i = 0; i < cat->count; i++) {
        if (cat->entries[i] && cat->entries[i]->index_id == index_id) {
            pthread_rwlock_unlock(&cat->rwlock);
            return cat->entries[i];
        }
    }

    pthread_rwlock_unlock(&cat->rwlock);
    return NULL;
}

int index_catalog_get_by_table(index_catalog_t *cat,
                               int table_id,
                               index_entry_t **results,
                               int max_results) {
    if (!cat || !results) return 0;

    pthread_rwlock_rdlock(&cat->rwlock);

    int found = 0;
    for (int i = 0; i < cat->count && found < max_results; i++) {
        if (cat->entries[i] && cat->entries[i]->table_id == table_id) {
            results[found++] = cat->entries[i];
        }
    }

    pthread_rwlock_unlock(&cat->rwlock);
    return found;
}

const char *index_type_to_string(index_type_t type) {
    static const char *names[] = {
        "btree", "hash", "hnsw", "ivf", "fulltext", "gin"
    };
    if (type >= 0 && type < INDEX_TYPE_COUNT) {
        return names[type];
    }
    return "unknown";
}

index_type_t index_type_from_string(const char *str) {
    if (!str) return INDEX_TYPE_BTREE;
    if (strcmp(str, "btree") == 0 || strcmp(str, "b tree") == 0) return INDEX_TYPE_BTREE;
    if (strcmp(str, "hash") == 0) return INDEX_TYPE_HASH;
    if (strcmp(str, "hnsw") == 0) return INDEX_TYPE_HNSW;
    if (strcmp(str, "ivf") == 0) return INDEX_TYPE_IVF;
    if (strcmp(str, "fulltext") == 0 || strcmp(str, "text") == 0) return INDEX_TYPE_FULLTEXT;
    if (strcmp(str, "gin") == 0) return INDEX_TYPE_GIN;
    return INDEX_TYPE_BTREE;
}
```

- [ ] **Step 3: 更新 CMakeLists.txt**

- [ ] **Step 4: 提交**

```bash
cd D:/code/book/engineering
git add include/db/index/index_catalog.h src/db/index/index_catalog.c
git commit -m "feat(index): Add index catalog for index metadata management"
```

---

### Task 2: IndexManager 统一索引管理器

**Files:**
- Create: `engineering/include/db/index/index_manager.h`
- Create: `engineering/src/db/index/index_manager.c`
- Modify: `engineering/src/db/index/CMakeLists.txt`

**Interfaces:**
- Consumes: index_catalog.h, index_config.h
- Produces: index_manager_t, index_manager_create/drop/rebuild

- [ ] **Step 1: 创建 index_manager.h**

```c
// engineering/include/db/index/index_manager.h
#ifndef DB_INDEX_MANAGER_H
#define DB_INDEX_MANAGER_H

#include "index_catalog.h"
#include "index_config.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct index_manager index_manager_t;

/**
 * @brief 创建索引管理器
 */
index_manager_t *index_manager_create(void);

/**
 * @brief 销毁索引管理器
 */
void index_manager_destroy(index_manager_t *mgr);

/**
 * @brief 创建索引
 */
int index_manager_create_index(index_manager_t *mgr,
                              const char *name,
                              index_type_t type,
                              int table_id,
                              const int *columns,
                              int column_count,
                              const index_config_t *config);

/**
 * @brief 删除索引
 */
int index_manager_drop_index(index_manager_t *mgr, int index_id);

/**
 * @brief 重建索引
 */
int index_manager_rebuild_index(index_manager_t *mgr, int index_id);

/**
 * @brief 获取索引条目
 */
const index_entry_t *index_manager_get_index(index_manager_t *mgr, int index_id);

/**
 * @brief 获取表的索引列表
 */
int index_manager_get_table_indexes(index_manager_t *mgr,
                                   int table_id,
                                   index_entry_t **results,
                                   int max_results);

/**
 * @brief 获取目录
 */
index_catalog_t *index_manager_get_catalog(index_manager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_MANAGER_H */
```

- [ ] **Step 2: 创建 index_manager.c**

```c
// engineering/src/db/index/index_manager.c
#include "db/index/index_manager.h"
#include <stdlib.h>
#include <string.h>

struct index_manager {
    index_catalog_t *catalog;
    int next_index_id;
};

index_manager_t *index_manager_create(void) {
    index_manager_t *mgr = calloc(1, sizeof(index_manager_t));
    if (!mgr) return NULL;

    mgr->catalog = index_catalog_create(16);
    if (!mgr->catalog) {
        free(mgr);
        return NULL;
    }

    mgr->next_index_id = 1;
    return mgr;
}

void index_manager_destroy(index_manager_t *mgr) {
    if (!mgr) return;
    if (mgr->catalog) index_catalog_destroy(mgr->catalog);
    free(mgr);
}

int index_manager_create_index(index_manager_t *mgr,
                              const char *name,
                              index_type_t type,
                              int table_id,
                              const int *columns,
                              int column_count,
                              const index_config_t *config) {
    if (!mgr || !name) return -1;

    index_entry_t *entry = calloc(1, sizeof(index_entry_t));
    if (!entry) return -1;

    entry->index_id = mgr->next_index_id++;
    strncpy(entry->name, name, MAX_INDEX_NAME_LEN - 1);
    entry->type = type;
    entry->state = INDEX_STATE_BUILDING;
    entry->table_id = table_id;
    entry->created_at = time(NULL);

    if (columns && column_count > 0) {
        entry->columns = malloc(column_count * sizeof(int));
        if (!entry->columns) {
            free(entry);
            return -1;
        }
        memcpy(entry->columns, columns, column_count * sizeof(int));
        entry->column_count = column_count;
    }

    if (config) {
        entry->config = *config;
    }

    // TODO: 实际创建索引实例 (entry->index_impl)
    // 根据 type 调用对应的索引创建函数

    entry->state = INDEX_STATE_READY;

    return index_catalog_add(mgr->catalog, entry);
}

int index_manager_drop_index(index_manager_t *mgr, int index_id) {
    if (!mgr) return -1;

    index_entry_t *entry = index_catalog_get(mgr->catalog, index_id);
    if (!entry) return -1;

    entry->state = INDEX_STATE_DELETING;

    // TODO: 清理索引实例 (entry->index_impl)

    return index_catalog_remove(mgr->catalog, index_id);
}

int index_manager_rebuild_index(index_manager_t *mgr, int index_id) {
    if (!mgr) return -1;

    index_entry_t *entry = index_catalog_get(mgr->catalog, index_id);
    if (!entry) return -1;

    entry->state = INDEX_STATE_BUILDING;

    // TODO: 重建索引逻辑

    entry->state = INDEX_STATE_READY;
    return 0;
}

const index_entry_t *index_manager_get_index(index_manager_t *mgr, int index_id) {
    if (!mgr) return NULL;
    return index_catalog_get(mgr->catalog, index_id);
}

int index_manager_get_table_indexes(index_manager_t *mgr,
                                   int table_id,
                                   index_entry_t **results,
                                   int max_results) {
    if (!mgr) return 0;
    return index_catalog_get_by_table(mgr->catalog, table_id, results, max_results);
}

index_catalog_t *index_manager_get_catalog(index_manager_t *mgr) {
    if (!mgr) return NULL;
    return mgr->catalog;
}
```

- [ ] **Step 3: 更新 CMakeLists.txt**

- [ ] **Step 4: 提交**

---

### Task 3: IndexScanExec 索引扫描算子

**Files:**
- Create: `engineering/include/db/executor/exec_index_scan.h`
- Create: `engineering/src/db/executor/operators/index_scan_exec.c`
- Modify: `engineering/src/db/executor/operators/CMakeLists.txt`

**Interfaces:**
- Consumes: index_manager.h, exec_node.h (Gap#3)
- Produces: exec_create_index_scan()

- [ ] **Step 1: 创建 exec_index_scan.h**

```c
// engineering/include/db/executor/exec_index_scan.h
#ifndef DB_EXECUTOR_EXEC_INDEX_SCAN_H
#define DB_EXECUTOR_EXEC_INDEX_SCAN_H

#include "db/executor/exec_node.h"
#include "db/index/index_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建索引扫描 ExecNode
 *
 * @param mgr 索引管理器
 * @param index_id 索引ID
 * @param table_id 表ID
 * @param scan_range 扫描条件
 * @return ExecNode* 索引扫描节点
 */
ExecNode *exec_create_index_scan(index_manager_t *mgr,
                                 int index_id,
                                 int table_id,
                                 const void *scan_range);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_INDEX_SCAN_H */
```

- [ ] **Step 2: 创建 index_scan_exec.c**

```c
// engineering/src/db/executor/operators/index_scan_exec.c
#include "db/executor/exec_index_scan.h"
#include "db/executor/exec_node.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    index_manager_t *mgr;
    int index_id;
    int table_id;
    const void *scan_range;
    index_entry_t *entry;
    int state;
} IndexScanState;

static int index_scan_open(ExecNode *node) {
    IndexScanState *s = (IndexScanState *)node->state;
    if (!s || !s->mgr) return -1;

    s->entry = (index_entry_t *)index_manager_get_index(s->mgr, s->index_id);
    if (!s->entry) return -1;

    if (s->entry->state != INDEX_STATE_READY) return -1;

    s->state = 0;
    return 0;
}

static VectorBlock *index_scan_next(ExecNode *node) {
    IndexScanState *s = (IndexScanState *)node->state;
    if (!s || !s->entry) return NULL;

    // TODO: 根据索引类型执行扫描
    // - BTree: 范围查询
    // - HNSW: 向量最近邻搜索
    // - Hash: 等值查询

    return NULL;
}

static void index_scan_reset(ExecNode *node) {
    IndexScanState *s = (IndexScanState *)node->state;
    if (s) s->state = 0;
}

static void index_scan_close(ExecNode *node) {
    IndexScanState *s = (IndexScanState *)node->state;
    if (s) s->state = -1;
}

ExecNode *exec_create_index_scan(index_manager_t *mgr,
                                 int index_id,
                                 int table_id,
                                 const void *scan_range) {
    if (!mgr) return NULL;

    IndexScanState *state = calloc(1, sizeof(IndexScanState));
    if (!state) return NULL;

    state->mgr = mgr;
    state->index_id = index_id;
    state->table_id = table_id;
    state->scan_range = scan_range;

    ExecNode *node = calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = PLAN_SCAN_INDEX;
    node->state = state;
    node->open = index_scan_open;
    node->next = index_scan_next;
    node->reset = index_scan_reset;
    node->close = index_scan_close;

    return node;
}
```

- [ ] **Step 3: 更新 CMakeLists.txt**

- [ ] **Step 4: 提交**

---

### Task 4: IndexCost 代价模型

**Files:**
- Create: `engineering/include/db/index/index_cost.h`
- Create: `engineering/src/db/index/index_cost.c`
- Modify: `engineering/src/db/index/CMakeLists.txt`

**Interfaces:**
- Consumes: index_catalog.h
- Produces: index_cost_t, index_cost_estimate()

- [ ] **Step 1: 创建 index_cost.h**

```c
// engineering/include/db/index/index_cost.h
#ifndef DB_INDEX_COST_H
#define DB_INDEX_COST_H

#include "index_catalog.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 查询条件类型 */
typedef enum {
    COND_EQ = 0,       /* 等值 (=) */
    COND_LT,          /* 小于 (<) */
    COND_LE,          /* 小于等于 (<=) */
    COND_GT,          /* 大于 (>) */
    COND_GE,          /* 大于等于 (>=) */
    COND_RANGE,       /* 范围 (BETWEEN) */
    COND_TEXT         /* 全文搜索 */
} condition_type_t;

/* 查询条件 */
typedef struct {
    condition_type_t type;
    int column_id;
    void *value;
    void *value2;      /* 用于范围查询 */
} query_condition_t;

/* 表统计信息 */
typedef struct {
    size_t row_count;
    size_t page_count;
    double avg_row_width;
    size_t distinct_values;
} table_stats_t;

/* 代价估算结果 */
typedef struct {
    int index_id;
    double startup_cost;
    double total_cost;
    size_t rows_estimated;
} index_cost_t;

/**
 * @brief 估算使用索引的代价
 */
double index_cost_estimate(const index_entry_t *idx,
                          const query_condition_t *cond,
                          const table_stats_t *stats);

/**
 * @brief 计算索引选择性
 */
double index_selectivity(const index_entry_t *idx,
                        const query_condition_t *cond,
                        const table_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_COST_H */
```

- [ ] **Step 2: 创建 index_cost.c**

```c
// engineering/src/db/index/index_cost.c
#include "db/index/index_cost.h"
#include <math.h>

double index_cost_estimate(const index_entry_t *idx,
                          const query_condition_t *cond,
                          const table_stats_t *stats) {
    if (!idx || !cond || !stats || stats->row_count == 0) {
        return 0.0;
    }

    double selectivity = index_selectivity(idx, cond, stats);
    double rows = stats->row_count * selectivity;

    // 启动代价: 索引扫描初始化
    double startup = 10.0;

    // 索引类型调整因子
    double type_factor = 1.0;
    switch (idx->type) {
        case INDEX_TYPE_HASH:    type_factor = 0.5; break;  // Hash 最快
        case INDEX_TYPE_BTREE:  type_factor = 1.0; break;
        case INDEX_TYPE_GIN:    type_factor = 1.5; break;
        case INDEX_TYPE_HNSW:
        case INDEX_TYPE_IVF:    type_factor = 2.0; break;  // 向量索引较慢
        case INDEX_TYPE_FULLTEXT: type_factor = 3.0; break;
        default: type_factor = 1.0;
    }

    // 总代价: 启动代价 + 行代价 * 类型因子
    double cost = startup + rows * type_factor;
    return cost;
}

double index_selectivity(const index_entry_t *idx,
                        const query_condition_t *cond,
                        const table_stats_t *stats) {
    if (!cond || !stats || stats->row_count == 0) {
        return 1.0;
    }

    double distinct = (stats->distinct_values > 0) ? stats->distinct_values : 1.0;
    double base_selectivity = 1.0 / distinct;

    switch (cond->type) {
        case COND_EQ:
            return base_selectivity;
        case COND_LT:
        case COND_LE:
        case COND_GT:
        case COND_GE:
            return 0.25;  // 假设四分之一
        case COND_RANGE:
            return 0.1;   // 假设十分之一
        case COND_TEXT:
            return 0.5;   // 全文搜索通常返回较多结果
        default:
            return 1.0;
    }
}
```

- [ ] **Step 3: 更新 CMakeLists.txt**

- [ ] **Step 4: 提交**

---

### Task 5: IndexSelector 索引选择器

**Files:**
- Create: `engineering/include/db/index/index_selector.h`
- Create: `engineering/src/db/index/index_selector.c`
- Modify: `engineering/src/db/index/CMakeLists.txt`

**Interfaces:**
- Consumes: index_manager.h, index_cost.h
- Produces: index_selector_t, index_selector_find_best()

- [ ] **Step 1: 创建 index_selector.h**

```c
// engineering/include/db/index/index_selector.h
#ifndef DB_INDEX_SELECTOR_H
#define DB_INDEX_SELECTOR_H

#include "index_manager.h"
#include "index_cost.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct index_selector index_selector_t;

/**
 * @brief 创建索引选择器
 */
index_selector_t *index_selector_create(index_manager_t *mgr);

/**
 * @brief 销毁索引选择器
 */
void index_selector_destroy(index_selector_t *sel);

/**
 * @brief 为查询选择最优索引
 *
 * @param sel 选择器
 * @param table_id 表ID
 * @param cond 查询条件
 * @param stats 表统计信息
 * @param best_cost 输出最优代价
 * @return int 最优索引ID，-1 表示无合适索引
 */
int index_selector_find_best(index_selector_t *sel,
                            int table_id,
                            const query_condition_t *cond,
                            const table_stats_t *stats,
                            index_cost_t *best_cost);

/**
 * @brief 评估所有可用索引
 *
 * @return int 评估的索引数量
 */
int index_selector_evaluate_all(index_selector_t *sel,
                               int table_id,
                               const query_condition_t *cond,
                               const table_stats_t *stats,
                               index_cost_t *costs,
                               int max_costs);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_SELECTOR_H */
```

- [ ] **Step 2: 创建 index_selector.c**

```c
// engineering/src/db/index/index_selector.c
#include "db/index/index_selector.h"
#include <stdlib.h>
#include <string.h>

#define MAX_INDEX_COSTS 32

struct index_selector {
    index_manager_t *mgr;
};

index_selector_t *index_selector_create(index_manager_t *mgr) {
    if (!mgr) return NULL;

    index_selector_t *sel = calloc(1, sizeof(index_selector_t));
    if (!sel) return NULL;

    sel->mgr = mgr;
    return sel;
}

void index_selector_destroy(index_selector_t *sel) {
    free(sel);
}

int index_selector_find_best(index_selector_t *sel,
                            int table_id,
                            const query_condition_t *cond,
                            const table_stats_t *stats,
                            index_cost_t *best_cost) {
    if (!sel || !cond || !best_cost) return -1;

    index_cost_t costs[MAX_INDEX_COSTS];
    int count = index_selector_evaluate_all(sel, table_id, cond, stats,
                                           costs, MAX_INDEX_COSTS);

    if (count <= 0) return -1;

    // 选择代价最小的索引
    double min_cost = costs[0].total_cost;
    int best_idx = 0;

    for (int i = 1; i < count; i++) {
        if (costs[i].total_cost < min_cost) {
            min_cost = costs[i].total_cost;
            best_idx = i;
        }
    }

    *best_cost = costs[best_idx];
    return best_cost->index_id;
}

int index_selector_evaluate_all(index_selector_t *sel,
                               int table_id,
                               const query_condition_t *cond,
                               const table_stats_t *stats,
                               index_cost_t *costs,
                               int max_costs) {
    if (!sel || !cond || !costs || max_costs <= 0) return 0;

    index_catalog_t *catalog = index_manager_get_catalog(sel->mgr);
    if (!catalog) return 0;

    index_entry_t *entries[32];
    int count = index_manager_get_table_indexes(sel->mgr, table_id,
                                                entries, 32);
    if (count <= 0) return 0;

    int result_count = 0;
    for (int i = 0; i < count && result_count < max_costs; i++) {
        index_entry_t *idx = entries[i];
        if (idx->state != INDEX_STATE_READY) continue;

        // 检查索引列是否包含查询列
        bool column_match = false;
        for (int j = 0; j < idx->column_count; j++) {
            if (idx->columns[j] == cond->column_id) {
                column_match = true;
                break;
            }
        }

        if (!column_match) continue;

        // 计算代价
        double cost = index_cost_estimate(idx, cond, stats);

        costs[result_count].index_id = idx->index_id;
        costs[result_count].startup_cost = 10.0;
        costs[result_count].total_cost = cost;
        costs[result_count].rows_estimated =
            stats ? (size_t)(stats->row_count / (stats->distinct_values ?: 1)) : 0;

        result_count++;
    }

    return result_count;
}
```

- [ ] **Step 3: 更新 CMakeLists.txt**

- [ ] **Step 4: 提交**

---

### Task 6: 单元测试

**Files:**
- Create: `engineering/test/db/index/index_manager_test.cpp`
- Modify: `engineering/test/db/index/CMakeLists.txt`

- [ ] **Step 1: 创建单元测试**

```cpp
#include <gtest/gtest.h>
#include "db/index/index_manager.h"
#include "db/index/index_catalog.h"
#include "db/index/index_cost.h"
#include "db/index/index_selector.h"

class IndexManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = index_manager_create();
    }
    void TearDown() override {
        if (mgr) index_manager_destroy(mgr);
    }
    index_manager_t *mgr = nullptr;
};

// 测试创建索引管理器
TEST_F(IndexManagerTest, CreateAndDestroy) {
    ASSERT_NE(mgr, nullptr);
}

// 测试创建索引
TEST_F(IndexManagerTest, CreateIndex) {
    int columns[] = {1};
    index_config_t config = index_config_default();

    int ret = index_manager_create_index(mgr, "test_idx", INDEX_TYPE_BTREE,
                                          1, columns, 1, &config);
    EXPECT_EQ(ret, 0);

    const index_entry_t *entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->name, "test_idx");
    EXPECT_EQ(entry->type, INDEX_TYPE_BTREE);
    EXPECT_EQ(entry->state, INDEX_STATE_READY);
}

// 测试删除索引
TEST_F(IndexManagerTest, DropIndex) {
    int columns[] = {1};
    index_config_t config = index_config_default();

    index_manager_create_index(mgr, "test_idx", INDEX_TYPE_BTREE,
                               1, columns, 1, &config);
    EXPECT_EQ(index_manager_drop_index(mgr, 1), 0);
    EXPECT_EQ(index_manager_get_index(mgr, 1), nullptr);
}

// 测试索引目录
TEST_F(IndexManagerTest, CatalogOperations) {
    index_catalog_t *cat = index_manager_get_catalog(mgr);
    ASSERT_NE(cat, nullptr);

    int columns[] = {1, 2};
    index_config_t config = index_config_default();

    index_manager_create_index(mgr, "idx1", INDEX_TYPE_HASH, 1, columns, 1, &config);
    index_manager_create_index(mgr, "idx2", INDEX_TYPE_BTREE, 1, columns + 1, 1, &config);

    index_entry_t *results[10];
    int count = index_manager_get_table_indexes(mgr, 1, results, 10);
    EXPECT_EQ(count, 2);
}

// 测试代价估算
TEST_F(IndexManagerTest, CostEstimation) {
    index_entry_t idx = {0};
    idx.type = INDEX_TYPE_BTREE;

    query_condition_t cond = {COND_EQ, 1, NULL, NULL};

    table_stats_t stats = {10000, 100, 100, 1000};

    double cost = index_cost_estimate(&idx, &cond, &stats);
    EXPECT_GT(cost, 0.0);

    double selectivity = index_selectivity(&idx, &cond, &stats);
    EXPECT_GT(selectivity, 0.0);
    EXPECT_LE(selectivity, 1.0);
}

// 测试索引选择器
TEST_F(IndexManagerTest, IndexSelector) {
    index_selector_t *sel = index_selector_create(mgr);
    ASSERT_NE(sel, nullptr);

    index_cost_t best_cost;
    int best_id = index_selector_find_best(sel, 1, NULL, NULL, &best_cost);
    // 无索引时返回 -1

    index_selector_destroy(sel);
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

- [ ] **Step 3: 编译测试**

- [ ] **Step 4: 提交**

---

### Task 7: 集成测试

**Files:**
- Create: `engineering/test/db/index/index_integration_test.cpp`
- Modify: `engineering/test/db/index/CMakeLists.txt`

- [ ] **Step 1: 创建集成测试**

```cpp
#include <gtest/gtest.h>
#include "db/index/index_manager.h"
#include "db/index/index_selector.h"
#include "db/executor/exec_index_scan.h"

class IndexIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = index_manager_create();
        ASSERT_NE(mgr, nullptr);
    }
    void TearDown() override {
        if (mgr) index_manager_destroy(mgr);
    }
    index_manager_t *mgr = nullptr;
};

// 测试完整工作流: 创建索引 -> 选择索引 -> 执行扫描
TEST_F(IndexIntegrationTest, FullWorkflow) {
    // 1. 创建多个索引
    int cols1[] = {1};
    int cols2[] = {1};
    index_config_t config = index_config_default();

    EXPECT_EQ(index_manager_create_index(mgr, "hash_idx", INDEX_TYPE_HASH,
                                         1, cols1, 1, &config), 0);
    EXPECT_EQ(index_manager_create_index(mgr, "btree_idx", INDEX_TYPE_BTREE,
                                        1, cols2, 1, &config), 0);

    // 2. 验证索引已创建
    ASSERT_NE(index_manager_get_index(mgr, 1), nullptr);
    ASSERT_NE(index_manager_get_index(mgr, 2), nullptr);

    // 3. 使用选择器选择最优索引
    index_selector_t *sel = index_selector_create(mgr);
    ASSERT_NE(sel, nullptr);

    query_condition_t cond = {COND_EQ, 1, NULL, NULL};
    table_stats_t stats = {1000000, 10000, 50, 10000};
    index_cost_t best_cost;

    int best_id = index_selector_find_best(sel, 1, &cond, &stats, &best_cost);
    EXPECT_GE(best_id, 1);

    index_selector_destroy(sel);

    // 4. 创建索引扫描节点
    ExecNode *scan = exec_create_index_scan(mgr, best_id, 1, &cond);
    EXPECT_NE(scan, nullptr);

    if (scan) {
        exec_destroy(scan);
    }
}
```

- [ ] **Step 2: 编译测试并验证**

- [ ] **Step 3: 提交**

---

## 任务依赖关系

```
Task 1: IndexCatalog    ← 基础
Task 2: IndexManager    ← 依赖 Task 1
Task 3: IndexScanExec   ← 依赖 Task 2
Task 4: IndexCost       ← 依赖 Task 1
Task 5: IndexSelector   ← 依赖 Task 2, 4
Task 6: 单元测试        ← 依赖 Task 1-5
Task 7: 集成测试        ← 依赖 Task 1-6
```

## 成功标准

- [ ] Task 1: IndexCatalog 提供索引元数据管理
- [ ] Task 2: IndexManager 提供统一的 create/drop/rebuild 接口
- [ ] Task 3: IndexScanExec 与 Gap#3 ExecNode 集成
- [ ] Task 4: IndexCost 提供代价估算
- [ ] Task 5: IndexSelector 基于代价选择最优索引
- [ ] Task 6: 单元测试全部通过
- [ ] Task 7: 集成测试通过
