# Gap#3 统一执行引擎设计

> **日期:** 2026-09-02
> **状态:** 已批准

## 1. 目标

设计并实现统一的执行引擎框架，将 Optimizer 输出的逻辑计划转换为可执行节点树，实现：
- Volcano Pull-based 迭代模型
- 向量化批处理（VectorBlock 级别）
- 与 Gap#2 向量化算子无缝集成

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                        Optimizer                             │
│                  (optimizer.c, plan_node_t)                 │
└─────────────────────┬───────────────────────────────────────┘
                      │ optimizer_optimize()
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                    Plan → Exec 转换层                        │
│              (optimizer_exec.c, plan_to_exec())             │
└─────────────────────┬───────────────────────────────────────┘
                      │ 转换
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                      Executor 调度器                         │
│                 (executor_framework.c)                      │
│                                                              │
│  exec_exec() → exec_init() → exec_next() → exec_end()      │
└─────────────────────┬───────────────────────────────────────┘
                      │
          ┌───────────┴───────────┐
          ▼                       ▼
┌──────────────────┐    ┌──────────────────┐
│   ExecNode 基结构 │    │  ExecNode 函数表  │
│ (exec_node.h)    │    │  open/next/close │
└──────────────────┘    └──────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────┐
│                    算子实现层                                │
│                                                              │
│  filter_exec.c  → vecx_filter_block()                       │
│  project_exec.c → vecx_project()                            │
│  hashjoin_exec.c → vecx_hashjoin_create/add_build/probe()   │
│  hashagg_exec.c → vecx_hashagg_create/add_block/emit()      │
│  seqscan_exec.c → vecx_source_from_columns()                │
└─────────────────────────────────────────────────────────────┘
```

## 3. 核心数据结构

### 3.1 ExecNode 基结构

```c
typedef struct ExecNode {
    plan_node_type_t node_type;     /* 节点类型（PLAN_SCAN_SEQ 等） */
    struct ExecNode *left;          /* 左子节点 */
    struct ExecNode *right;         /* 右子节点（Join用） */
    void *state;                    /* 算子私有状态 */
    
    /* 函数指针表 */
    VectorBlock *(*open)(struct ExecNode *node);
    VectorBlock *(*next)(struct ExecNode *node);
    void (*close)(struct ExecNode *node);
    void (*reset)(struct ExecNode *node);
} ExecNode;
```

### 3.2 各算子状态结构

```c
/* Filter 状态 */
typedef struct {
    vecx_pred_t pred;
    VectorBlock *cur_block;
} FilterState;

/* HashJoin 状态 */
typedef struct {
    vecx_hashjoin_t *hj;
    VectorBlock *cur_block;
    int exhausted;
} HashJoinState;

/* HashAgg 状态 */
typedef struct {
    vecx_hashagg_t *agg;
    VectorBlock *cur_block;
    int emitted;
} HashAggState;
```

## 4. Executor 公共接口

```c
/* 创建执行树（Plan → ExecNode 转换） */
ExecNode *exec_create(plan_node_t *plan);

/* 生命周期管理 */
int exec_open(ExecNode *root);
VectorBlock *exec_next(ExecNode *node);
void exec_reset(ExecNode *node);
void exec_close(ExecNode *root);
void exec_destroy(ExecNode *root);

/* 便捷接口：一键执行 */
VectorBlock *exec_exec(plan_node_t *plan, int *has_result);
```

## 5. 算子实现清单

| 算子 | 实现文件 | 依赖 vecx_* | 状态结构 |
|------|----------|-------------|----------|
| SeqScan | seqscan_exec.c | vecx_source_from_columns | SeqScanState |
| Filter | filter_exec.c | vecx_filter_block | FilterState |
| Project | project_exec.c | vecx_project | ProjectState |
| HashJoin | hashjoin_exec.c | vecx_hashjoin_* | HashJoinState |
| HashAgg | hashagg_exec.c | vecx_hashagg_* | HashAggState |
| Sort | sort_exec.c | (独立实现) | SortState |

## 6. 生命周期

```
exec_create(plan)        → 构建 ExecNode 树 + 分配状态
       ↓
exec_open(root)          → 自底向上调用 open()
       ↓
while ((block = exec_next(root)) != NULL) {
    // 处理 block
}
       ↓
exec_close(root)         → 自顶向下调用 close()
       ↓
exec_destroy(root)       → 释放 ExecNode 树
```

**open() 调用顺序：** 自底向上（先子节点，后父节点）
**close() 调用顺序：** 自顶向下

## 7. 关键设计决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 数据流模型 | Pull-based | 与 Volcano 一致，next() 返回 VectorBlock |
| 接口设计 | 适配器模式 | PlanNode → ExecNode 解耦 |
| 集成方式 | B 转换层 | 业界标准，Plan → ExecNode 转换 |
| 状态管理 | 函数指针表 | PostgreSQL 风格，扩展性好 |
| 算子关系 | Executor 调度 + 子模块实现 | 关注点分离，vecx_* 可替换 |

## 8. 文件结构

```
engineering/
├── include/db/executor/
│   ├── executor_framework.h      # Executor 公共接口
│   ├── exec_node.h               # ExecNode 基结构 + 函数指针
│   └── exec_states.h             # 各算子状态结构
├── src/db/executor/
│   ├── framework/
│   │   ├── executor_framework.c  # 调度器实现
│   │   └── plan_to_exec.c        # Plan → ExecNode 转换
│   └── operators/
│       ├── seqscan_exec.c        # SeqScan 实现
│       ├── filter_exec.c         # Filter 实现
│       ├── project_exec.c        # Project 实现
│       ├── hashjoin_exec.c       # HashJoin 实现
│       ├── hashagg_exec.c        # HashAgg 实现
│       └── sort_exec.c           # Sort 实现
└── test/db/executor/
    ├── framework_test.cpp        # 调度器测试
    └── integration_test.cpp      # 端到端测试
```

## 9. 与现有代码的关系

### 9.1 依赖 Gap#2 向量化算子

| Executor 算子 | vecx_* 函数 |
|--------------|-------------|
| SeqScan | vecx_source_from_columns() |
| Filter | vecx_filter_block() |
| Project | vecx_project() |
| HashJoin | vecx_hashjoin_create/add_build/probe/destroy |
| HashAgg | vecx_hashagg_create/add_block/emit/destroy |

### 9.2 保留既有代码

- `optimizer/optimizer.c` — 保持不变，只增加 plan_to_exec() 转换函数
- `vectorized/*.c` — 保持独立可测试，Gap#2 测试套件保留
- `sqlExecutor.c` — 保留，作为向后兼容层

## 10. 测试策略

### 10.1 单元测试

每个算子独立测试：
- 构造 plan_node → exec_create() → exec_open() → exec_next() → 结果验证
- OOM 场景测试
- 边界条件测试（空输入、NULL 输入）

### 10.2 集成测试

端到端测试：
- SQL 解析 → Optimizer → Executor → 结果验证
- 覆盖 SELECT/FILTER/JOIN/AGG 等典型查询

### 10.3 回归测试

确保 Gap#2 向量化算子在 Executor 框架下仍然正常工作。

## 11. 扩展点

未来可扩展：
1. **并行执行（Gap#4）** — ExecNode 增加并行属性，Executor 支持多线程调度
2. **物化视图** — 增加 MaterializeExecNode
3. **分布式执行** — ExecNode 支持远程执行

## 12. 成功标准

- [ ] Executor 公共接口完整
- [ ] SeqScan/Filter/Project/HashJoin/HashAgg 五个算子可工作
- [ ] Plan → ExecNode 转换正确
- [ ] 生命周期管理正确（open/next/close）
- [ ] 与 vecx_* 函数正确集成
- [ ] 单元测试覆盖所有算子
- [ ] 端到端集成测试通过
