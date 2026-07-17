# SQL SELECT 执行规格

## Overview

实现 SELECT 语句的完整执行逻辑，包括表扫描、过滤、投影和结果返回。

## Execution Model

采用火山模型（Volcano Model），算子树自顶向下 pull 模式：

```
     Projection
         │
    ┌────┴────┐
    │         │
 Filter    Filter
    │         │
SeqScan   SeqScan
    │         │
Table     Table
```

## Data Structures

### ExecNode

```c
typedef struct ExecNode_s {
    const char *type;           /**< 算子类型 */
    ExecOps    *ops;            /**< 算子操作函数 */
    EState     *state;          /**< 执行状态 */
} ExecNode;
```

### EState (执行状态)

```c
typedef struct EState_s {
    sql_executor_t *executor;   /**< 所属执行器 */
    txn_t          *txn;        /**< 执行事务 */
    Relation       *relations;  /**< 打开的 Relation */
    int             nrelations; /**< Relation 数量 */
    MemoryContext   context;    /**< 内存上下文 */
} EState;
```

### SeqScanState

```c
typedef struct SeqScanState_s {
    ExecNode       base;
    Relation       relation;
    TableScanDesc  scan;
    List           *qual;       /**< 过滤条件列表 */
    List           *targetlist; /**< 目标列列表 */
    ProjectionInfo *proj;       /**< 投影信息 */
} SeqScanState;
```

### ProjectionInfo

```c
typedef struct ProjectionInfo_s {
    int         *col_idx;       /**< 要输出的列索引 */
    int         ncols;          /**< 输出列数 */
    char        *buffer;        /**< 结果行缓冲区 */
} ProjectionInfo;
```

## API

### 创建执行节点

```c
/**
 * @brief 创建顺序扫描算子
 * @param estate 执行状态
 * @param rel 扫描的 Relation
 * @param qual 过滤条件（可为 NULL）
 * @param targetlist 目标列（NULL 表示 SELECT *）
 * @return 扫描算子
 */
SeqScanState *exec_init_seqscan(EState *estate, Relation rel,
                                List *qual, List *targetlist);

/**
 * @brief 创建投影算子
 * @param estate 执行状态
 * @param child 子算子
 * @param proj 投影信息
 * @return 投影算子
 */
ProjectionState *exec_init_proj(EState *estate, ExecNode *child,
                                ProjectionInfo *proj);
```

### 执行接口

```c
/**
 * @brief 执行算子树，返回下一行
 * @param node 根算子
 * @return 行数据，NULL 表示无更多行
 */
void *exec_exec(ExecNode *node);

/**
 * @brief 关闭算子，释放资源
 * @param node 算子
 */
void exec_end(ExecNode *node);

/**
 * @brief 重新扫描（用于子查询）
 * @param node 算子
 */
void exec_rescan(ExecNode *node);
```

## SELECT 执行流程

```
1. 解析 SQL，生成 AST
2. 语义分析：解析表名、列名，构建 TupleDesc
3. 生成执行计划：
   a. 创建 EState
   b. 打开 Relation
   c. 构建 SeqScanState + Filter + Projection
4. 执行循环：
   while ((row = exec_exec(root)) != NULL) {
       add_to_result(row);
   }
5. 清理资源
```

## Filter Evaluation

过滤条件求值：

```c
/**
 * @brief 求值过滤条件
 * @param qual 条件 AST
 * @param row 行数据
 * @param meta 表元数据
 * @return true 满足条件
 */
bool eval_qual(sql_node_t *qual, const void *row, table_meta_t *meta);

/**
 * @brief 递归求值表达式
 * @param expr 表达式 AST
 * @param row 行数据
 * @param meta 表元数据
 * @return 布尔值
 */
bool eval_expr(sql_node_t *expr, const void *row, table_meta_t *meta);
```

## Result Format

```c
typedef struct {
    char      **columns;   /**< 列名 */
    int       *col_types;  /**< 列类型 */
    int        ncolumns;   /**< 列数 */
    char      **rows;      /**< 行数据 */
    int        nrow;       /**< 行数 */
    int        row_capacity;
} sql_query_result_t;
```

## Acceptance Criteria

- [ ] `SELECT * FROM t` 返回表中所有行
- [ ] `SELECT id, name FROM t` 只返回指定列
- [ ] `SELECT id FROM t WHERE id > 10` 正确过滤
- [ ] `SELECT COUNT(*)` 返回行数
- [ ] `SELECT id FROM t ORDER BY id` 结果排序
- [ ] `SELECT DISTINCT name FROM t` 去重
- [ ] 空结果集返回空数组，不报错
- [ ] 表不存在返回错误信息
