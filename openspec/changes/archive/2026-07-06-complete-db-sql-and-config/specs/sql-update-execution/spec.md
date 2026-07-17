# SQL UPDATE 执行规格

## Overview

实现 UPDATE 语句的执行逻辑，包括行扫描、条件匹配、新值计算和元组更新。

## Execution Strategy

采用"标记-删除-插入"模式（PostgreSQL 标准做法）：
1. 扫描匹配行
2. 标记旧元组为已删除（写入 WAL）
3. 构造新元组
4. 插入新元组
5. 更新索引

## Data Structures

### UpdateState

```c
typedef struct UpdateState_s {
    ExecNode       base;
    Relation       relation;       /**< 目标表 */
    EState        *estate;
    List          *set_clause;     /**< SET 子句列表 */
    List          *qual;           /**< WHERE 条件 */
    List          *indexes;        /**< 需要更新的索引 */
    int            updated;        /**< 已更新行数 */
} UpdateState;
```

### SetClause

```c
typedef struct SetClause_s {
    int         col_index;         /**< 列索引 */
    char        *col_name;         /**< 列名 */
    sql_node_t  *value;            /**< 新值表达式 */
} SetClause;
```

## API

### 创建执行器

```c
/**
 * @brief 创建 UPDATE 算子
 * @param estate 执行状态
 * @param rel 目标 Relation
 * @param set_clause SET 子句列表
 * @param qual WHERE 条件
 * @return UPDATE 算子
 */
UpdateState *exec_init_update(EState *estate, Relation rel,
                              List *set_clause, List *qual);
```

### 执行接口

```c
/**
 * @brief 执行 UPDATE
 * @param state UPDATE 算子
 * @return 影响的行数
 */
int exec_update(UpdateState *state);

/**
 * @brief 更新单行
 * @param state UPDATE 算子
 * @param tuple 原始元组
 * @return 0 成功，-1 失败
 */
int update_tuple(UpdateState *state, HeapTuple tuple);
```

## Execution Flow

```
update_table(rel, set_clause, qual):
    1. 检查 SET 子句列是否有效
    2. 打开表扫描
    3. updated = 0
    4. while ((tuple = heap_getnext(scan)) != NULL):
           if (eval_qual(qual, tuple)):
               // 4a. 锁定元组（可选）
               // 4b. 标记旧元组为已删除
               heap_delete(rel, &tuple->ctid)
               // 4c. 构造新元组
               new_tuple = form_new_tuple(tuple, set_clause)
               // 4d. 插入新元组
               heap_insert(rel, new_tuple)
               // 4e. 更新索引（如果有）
               for idx in indexes:
                   idx_update(idx, old_tuple, new_tuple)
               // 4f. 记录到 WAL
               wal_write_update(..., old_tuple, new_tuple)
               updated++
    5. 关闭扫描
    6. return updated
```

### form_new_tuple

```c
/**
 * @brief 根据 SET 子句构造新元组
 * @param old_tuple 原始元组
 * @param set_clause SET 子句列表
 * @return 新元组
 */
HeapTuple form_new_tuple(HeapTuple old_tuple, List *set_clause) {
    // 1. 复制原始元组数据
    new_data = memcpy(malloc(old_size), old_tuple->data, old_size);

    // 2. 应用 SET 子句
    for clause in set_clause:
        // 求值表达式
        value = eval_expr(clause->value, old_tuple, meta);
        // 写入新数据
        write_column(new_data, clause->col_index, value);

    // 3. 返回新元组
    return make_tuple(new_data, size);
}
```

## Conflict Handling

### 乐观锁

```c
// 如果表有 xmin/xmax 列，使用 MVCC
if (table_has_mvcc(rel)) {
    // 检查元组是否被其他事务修改
    if (tuple->xmax != current_txn_id && tuple->xmax is active) {
        skip_tuple();  // 跳过或抛异常
    }
}
```

### 索引冲突

```c
// 唯一索引冲突
if (unique_index_violation(idx, new_tuple)) {
    throw ERROR: "duplicate key value violates unique constraint";
}
```

## Acceptance Criteria

- [ ] `UPDATE t SET name = 'new' WHERE id = 1` 单行更新
- [ ] `UPDATE t SET age = age + 1 WHERE id > 10` 表达式更新
- [ ] `UPDATE t SET a = 1, b = 2 WHERE id = 1` 多列更新
- [ ] `UPDATE t SET name = 'new'` 无 WHERE，更新所有行
- [ ] `UPDATE t SET name = 'new' WHERE 1=0` WHERE 不匹配，0 行更新
- [ ] 唯一索引冲突正确报错
- [ ] 事务回滚能撤销更新
- [ ] WAL 正确记录更新前后状态
