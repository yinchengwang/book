# Gap#7 索引框架增强设计

> **日期:** 2026-09-02
> **状态:** 已批准

## 1. 目标

增强索引框架，实现：
- 统一索引管理层 (IndexManager)
- 执行引擎集成 (IndexScan 算子)
- 索引选择优化 (代价模型)

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                      IndexManager                           │
│  ┌────────────────┐  ┌──────────────────────────────────┐  │
│  │  IndexCatalog  │  │       IndexLifecycle             │  │
│  │  (索引目录)    │  │  (create/drop/rebuild)          │  │
│  └────────────────┘  └──────────────────────────────────┘  │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│                   IndexRouter                               │
│  ┌────────────────┐  ┌──────────────────────────────────┐  │
│  │ CostModel      │  │     IndexSelector               │  │
│  │ (代价模型)     │  │  (select best index)            │  │
│  └────────────────┘  └──────────────────────────────────┘  │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│                  Gap#3 ExecNode                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              IndexScanExec                          │   │
│  │  (索引扫描算子：利用 IndexRouter 选择索引执行查询)  │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## 3. Phase 1: 统一索引管理层

### 3.1 IndexCatalog (索引目录)

```c
typedef enum {
    INDEX_TYPE_BTREE = 0,
    INDEX_TYPE_HASH,
    INDEX_TYPE_HNSW,
    INDEX_TYPE_IVF,
    INDEX_TYPE_FULLTEXT,
    INDEX_TYPE_GIN,
    INDEX_TYPE_COUNT
} index_type_t;

typedef enum {
    INDEX_STATE_BUILDING = 0,
    INDEX_STATE_READY,
    INDEX_STATE_DELETING
} index_state_t;

typedef struct index_entry {
    int index_id;
    char name[MAX_INDEX_NAME_LEN];
    index_type_t type;
    index_state_t state;
    table_id_t table_id;
    column_id_t *columns;
    int column_count;
    void *index_impl;           // 实际索引实现
    index_config_t config;
    time_t created_at;
    size_t size_bytes;
} index_entry_t;

typedef struct index_catalog {
    index_entry_t **entries;
    int capacity;
    int count;
    pthread_rwlock_t rwlock;
} index_catalog_t;
```

### 3.2 IndexLifecycle (生命周期管理)

```c
/**
 * @brief 创建索引
 */
int index_manager_create(index_manager_t *mgr,
                        const char *name,
                        index_type_t type,
                        table_id_t table_id,
                        const column_id_t *columns,
                        int column_count,
                        const index_config_t *config);

/**
 * @brief 删除索引
 */
int index_manager_drop(index_manager_t *mgr, int index_id);

/**
 * @brief 重建索引
 */
int index_manager_rebuild(index_manager_t *mgr, int index_id);

/**
 * @brief 获取索引信息
 */
const index_entry_t *index_manager_get(index_manager_t *mgr, int index_id);
```

### 3.3 IndexManager (统一入口)

```c
typedef struct index_manager {
    index_catalog_t *catalog;
    table_manager_t *table_mgr;
    storage_backend_t *storage;
    index_factory_t *factory;    // 索引工厂，创建各类型索引
} index_manager_t;
```

## 4. Phase 2: 执行引擎集成

### 4.1 IndexScanExec (索引扫描算子)

```c
typedef struct {
    ExecNode base;
    index_manager_t *mgr;
    int index_id;
    table_id_t table_id;
    void *scan_range;           // 查询条件
    VectorBlock *result;
} IndexScanExec;

ExecNode *exec_create_index_scan(index_manager_t *mgr,
                                 int index_id,
                                 table_id_t table_id,
                                 const void *scan_range);
```

### 4.2 索引下推优化

- 将 Filter 条件下推到索引扫描
- 利用索引过滤数据，减少扫描量
- 支持范围查询 (BETWEEN, >, <) 和等值查询 (=)

## 5. Phase 3: 索引选择优化

### 5.1 代价模型

```c
typedef struct index_cost {
    int index_id;
    double cost;                // 总代价
    double startup_cost;        // 启动代价
    double total_cost;          // 执行代价
    size_t rows_estimated;      // 估算行数
} index_cost_t;

/**
 * @brief 计算使用索引的代价
 */
double index_cost_estimate(const index_entry_t *idx,
                          const query_condition_t *cond,
                          const table_stats_t *stats);
```

### 5.2 索引选择器

```c
typedef struct index_selector {
    index_manager_t *mgr;
    cost_model_t *cost_model;
} index_selector_t;

/**
 * @brief 为查询选择最优索引
 */
int index_selector_find_best(const index_selector_t *sel,
                            table_id_t table_id,
                            const query_condition_t *cond,
                            index_cost_t *best_cost);
```

## 6. 文件结构

```
engineering/
├── include/db/index/
│   ├── index_manager.h          # 新增：统一索引管理器
│   ├── index_catalog.h          # 新增：索引目录
│   ├── index_selector.h         # 新增：索引选择器
│   └── index_cost.h             # 新增：代价模型
├── src/db/index/
│   ├── index_manager.c          # 新增：索引管理器实现
│   ├── index_catalog.c          # 新增：索引目录实现
│   ├── index_selector.c         # 新增：索引选择器实现
│   └── index_cost.c             # 新增：代价模型实现
├── include/db/executor/
│   └── exec_index_scan.h        # 新增：索引扫描算子
└── src/db/executor/operators/
    └── index_scan_exec.c        # 新增：索引扫描实现
```

## 7. 实现顺序

| Phase | 内容 | 依赖 |
|-------|------|------|
| 1 | 统一索引管理层 | 无 |
| 2 | 执行引擎集成 | Phase 1 |
| 3 | 索引选择优化 | Phase 1, 2 |

## 8. 成功标准

- [ ] IndexManager 提供统一的 create/drop/rebuild 接口
- [ ] IndexCatalog 维护所有索引的元数据
- [ ] IndexScanExec 能利用索引执行查询
- [ ] IndexSelector 基于代价模型选择最优索引
- [ ] 与 Gap#3 Executor Framework 集成
- [ ] 单元测试覆盖
- [ ] 集成测试通过
