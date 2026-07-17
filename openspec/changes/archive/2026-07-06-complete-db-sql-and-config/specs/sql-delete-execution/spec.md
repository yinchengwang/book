# SQL DELETE 执行规格

## Overview

实现 DELETE 语句的执行逻辑，包括行扫描、条件匹配和元组删除。

## Execution Strategy

采用 MVCC 安全删除：
1. 扫描匹配行
2. 标记元组为已删除（设置 xmax）
3. 写入 WAL
4. 后续 VACUUM 清理

## Data Structures

### DeleteState

```c
typedef struct DeleteState_s {
    ExecNode       base;
    Relation       relation;       /**< 目标表 */
    EState        *estate;
    List          *qual;           /**< WHERE 条件 */
    List          *indexes;        /**< 需要更新的索引 */
    int            deleted;        /**< 已删除行数 */
} DeleteState;
```

## API

### 创建执行器

```c
/**
 * @brief 创建 DELETE 算子
 * @param estate 执行状态
 * @param rel 目标 Relation
 * @param qual WHERE 条件
 * @return DELETE 算子
 */
DeleteState *exec_init_delete(EState *estate, Relation rel, List *qual);
```

### 执行接口

```c
/**
 * @brief 执行 DELETE
 * @param state DELETE 算子
 * @return 影响的行数
 */
int exec_delete(DeleteState *state);
```

## Execution Flow

```
delete_from_table(rel, qual):
    1. 打开表扫描
    2. deleted = 0
    3. while ((tuple = heap_getnext(scan)) != NULL):
           if (eval_qual(qual, tuple)):
               // 3a. 锁定元组（可选）
               // 3b. 检查 MVCC 可见性
               if (!tuple_visible(tuple, current_txn)):
                   continue;
               // 3c. 标记元组为已删除
               heap_delete(rel, &tuple->ctid)
               // 3d. 更新索引（移除索引项）
               for idx in indexes:
                   idx_delete(idx, tuple)
               // 3e. 记录到 WAL
               wal_write_delete(..., tuple)
               deleted++
    4. 关闭扫描
    5. return deleted
```

### heap_delete 实现

```c
/**
 * @brief 删除元组
 * @param rel Relation
 * @param ctid 元组指针（输入时为当前位置，输出时为新位置）
 * @return 0 成功
 */
int heap_delete(Relation rel, ItemPointer ctid) {
    // 1. 读取当前元组
    Buffer buf = ReadBuffer(rel, ItemPointerGetBlockNumber(ctid));
    Page page = BufferGetPage(buf);

    // 2. 获取 LinePointer
    OffsetNumber offset = ItemPointerGetOffsetNumber(ctid);
    HeapLinePointer lp = PageGetLinePointer(page, offset);

    // 3. 标记为已删除
    lp->lp_flags = HEAP_LP_DEAD;

    // 4. 设置删除事务信息（如果有 MVCC）
    // HeapTupleHeaderSetXmax(tuple->t_data, current_txn_id);
    // HeapTupleHeaderSetDeleted(tuple->t_data);

    // 5. 标记页面为脏
    MarkBufferDirty(buf);

    // 6. 释放锁
    UnlockReleaseBuffer(buf);

    return 0;
}
```

## MVCC Considerations

### 可见性规则

| 状态 | 行为 |
|------|------|
| 元组未删除 (xmax = 0) | 可见 |
| xmax = 当前事务 | 不可见（自己的 DELETE） |
| xmax 已提交 | 不可见 |
| xmax 未提交 | 不可见（未提交事务） |

### 并发删除

```c
// 检测并发删除
if (tuple->xmax != 0 && tuple->xmax != current_txn) {
    if (txn_is_committed(tuple->xmax)) {
        continue;  // 已被其他事务删除
    }
}
```

## Acceptance Criteria

- [ ] `DELETE FROM t WHERE id = 1` 删除单行
- [ ] `DELETE FROM t WHERE id > 10` 删除多行
- [ ] `DELETE FROM t` 无 WHERE，删除所有行
- [ ] `DELETE FROM t WHERE 1=0` 无匹配，0 行删除
- [ ] 事务回滚能撤销删除
- [ ] WAL 正确记录删除的元组
- [ ] 已删除元组在后续 SELECT 中不可见
- [ ] 索引正确更新（移除删除的键）
