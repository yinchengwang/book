# SQL Executor 状态报告

本文档记录 SQL Executor SELECT 功能的当前状态和后续工作。

---

## 现状分析

### 2.1.1 分析结果 ✅

| 组件 | 文件 | 状态 | 说明 |
|------|------|------|------|
| SQL Parser | `sql_parser.c` | 基础可用 | 关键字识别已完成 |
| SQL Executor | `sql_executor.c` | 部分实现 | `exec_seq_scan` 返回 NULL |
| Heap Access Method | `heapam.c` | 可用 | `heap_insert`/`heap_fetch` 已实现 |
| 集成测试 | `sql_integration.cpp` | 可用 | CLI 端到端测试框架 |

### exec_seq_scan 当前实现

```c
TupleTableSlot *exec_seq_scan(SeqScanState *node)
{
    ScanState *ss = &node->ss;
    PlanState *ps = &ss->ps;

    /* 获取下一个元组 */
    TupleTableSlot *slot = ss->scanrelid ? NULL : NULL;  /* TODO: 实际调用存储层 */

    if (slot) {
        return slot;
    }

    return NULL;  /* 扫描结束 */
}
```

**问题：** 直接返回 NULL，未调用 `heapam.c` 获取数据。

---

## 后续工作

### 2.1.2 实现 SELECT 解析

**目标：** 支持 `SELECT * FROM table WHERE column = value`

**需要完成：**
1. 在 `sql_parser.c` 中扩展 SELECT 语句解析
2. 构建查询计划树（Scan plan）
3. 将表名和 WHERE 条件传递到执行器

### 2.1.3 实现 Table Scan 执行器

**目标：** 将 `exec_seq_scan` 与 `heapam.c` 集成

**需要完成：**
1. 在 `SeqScanState` 中添加 Relation 引用
2. 调用 `heap_fetch(rel, ...)` 获取元组
3. 将元组填充到 TupleTableSlot

**参考实现：**
```c
TupleTableSlot *exec_seq_scan(SeqScanState *node)
{
    Relation rel = node->ss.scanrel;

    /* 循环获取元组 */
    while (heap_fetch(rel, &snapshot, &tid, &slot)) {
        /* 评估 WHERE 条件 */
        if (eval_qual(node->qual, ctx)) {
            return slot;
        }
    }

    return NULL;  /* 扫描结束 */
}
```

### 2.1.4 实现 Index Scan 执行器（可选）

**目标：** 支持索引加速查询

**需要完成：**
1. 在 `sql_parser.c` 中识别索引列上的 WHERE 条件
2. 构建 IndexScan plan
3. 调用 `btree_index_lookup` 获取数据

### 2.1.5 实现 Result Set 构建和返回

**目标：** 将查询结果返回给客户端

**需要完成：**
1. 填充 TupleDesc（列信息）
2. 通过 DestReceiver 发送结果
3. 处理 NULL 值和类型转换

### 2.1.6 集成测试

**目标：** 端到端验证 `SELECT * FROM users WHERE id = 1`

**测试用例：**
```sql
CREATE TABLE users (id INT, name TEXT);
INSERT INTO users VALUES (1, 'Alice'), (2, 'Bob');
SELECT * FROM users WHERE id = 1;
-- 期望输出: id=1, name=Alice
```

---

## 依赖关系

```
sql_parser.c ──→ sql_executor.c ──→ heapam.c
                      │
                      └──→ btreeam.c (for index scan)
```

---

## 风险与注意事项

1. **Parser 与 Executor 接口：** 需要确认 AST → Plan 的转换
2. **事务上下文：** `heap_fetch` 需要正确的 MVCC snapshot
3. **错误处理：** 需要处理表不存在、列不存在等情况

---

*创建时间: 2026-07-12*
*最后更新: 2026-07-12*
