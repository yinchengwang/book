# 执行引擎 → 生产级数据库演进路线图

> **目标**：从当前执行引擎框架演进到生产级数据库，对标 PostgreSQL 16/17 核心能力
> **总工作量**：约 17-22 人月（1.5-2 人年）
> **创建日期**：2026-07-15

---

## 总体路线图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          阶段 0：执行引擎核心                             │
│                        （3-4 人月，当前进行中）                            │
│   目标：完成 SELECT/INSERT/UPDATE/DELETE 基本执行能力                     │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          阶段 1：并发控制                                 │
│                          （4-5 人月）                                     │
│   目标：支持多用户并发访问，MVCC + 锁管理器 + 死锁检测                      │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          阶段 2：恢复能力                                 │
│                          （4-5 人月）                                     │
│   目标：保证数据安全，WAL 完整恢复 + PITR + 高可用                         │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          阶段 3：查询优化                                 │
│                          （3-4 人月）                                     │
│   目标：支持复杂查询，Selinger 代价模型 + 并行执行                         │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          阶段 4：运维能力                                 │
│                          （3-4 人月）                                     │
│   目标：可生产运维，监控 + VACUUM + 备份恢复工具                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 阶段对比：PostgreSQL vs 本项目

| 模块 | PostgreSQL | 本项目现状 | 阶段 0 完成后 | 生产级目标 |
|------|-----------|-----------|--------------|-----------|
| **执行引擎** | 77,267 行 | ~1,500 行 | ~20,000 行 | ~50,000 行 |
| **Access Method** | 162,264 行 | ~5,000 行 | ~15,000 行 | ~80,000 行 |
| **Buffer Pool** | 9,620 行 | ~2,000 行 | ~3,000 行 | ~6,000 行 |
| **并发控制** | ~30,000 行 | ~2,000 行 | ~2,000 行 | ~20,000 行 |
| **WAL/恢复** | 43,322 行 | ~3,000 行 | ~3,000 行 | ~25,000 行 |
| **查询优化** | ~40,000 行 | ~3,000 行 | ~5,000 行 | ~20,000 行 |
| **总计** | ~290,000 行 | ~13,500 行 | ~45,000 行 | ~200,000 行 |

---

# 阶段 0：执行引擎核心（当前）

**工期**：3-4 人月
**状态**：进行中（已完成基础设施层）
**目标**：完成 SELECT/INSERT/UPDATE/DELETE 基本执行能力

---

## 0.1 已完成部分

| 任务 | 状态 | 代码量 |
|------|------|--------|
| 内存上下文 (AllocSet) | ✅ 完成 | ~500 行 |
| 节点类型枚举 (NodeTag) | ✅ 完成 | ~200 行 |
| 表达式求值器 (ExprState) | ✅ 完成 | ~800 行 |
| Volcano 迭代器框架 | ✅ 完成 | ~500 行 |
| Result 节点 | ✅ 完成 | ~200 行 |

**已完成代码量**：~2,200 行

---

## 0.2 待完成：核心算子层

### 0.2.1 扫描节点（优先级最高）

#### 任务：SeqScan 节点实现

**目标**：实现全表扫描，对接 Access Method 层

**依赖**：
- `rel.h` - Relation 抽象
- `heapam.h` - Heap AM 接口
- `executor.h` - Volcano 框架

**关键接口对接**：

```c
// nodeSeqscan.c 需要调用：
SeqScanState *ExecInitSeqScan(SeqScan *node, EState *estate, int eflags) {
    // 1. 创建 SeqScanState
    // 2. 打开 Relation: relation_open(relid, REL_OPEN_READWRITE)
    // 3. 开始扫描: table_beginscan(rel, nkeys, key)
    // 4. 设置 ExecProcNode = ExecSeqScan
}

static TupleTableSlot *ExecSeqScan(PlanState *pstate) {
    SeqScanState *node = (SeqScanState *)pstate;
    
    // 调用 Access Method 层
    void *tuple = table_getnext(node->ss_scanDesc);
    
    if (tuple == NULL) {
        return NULL;  // 扫描结束
    }
    
    // 填充 TupleTableSlot
    ExecStoreBufferHeapTuple(tuple, node->ss_ScanTupleSlot, ...);
    return node->ss_ScanTupleSlot;
}
```

**验收标准**：
- `SELECT * FROM t` 能正确返回所有行
- Buffer Pool 统计正确（buf_hit, buf_read）
- 内存无泄漏

**预估工作量**：~1,500 行，2 周

---

#### 任务：IndexScan 节点实现

**目标**：实现索引扫描，对接 BTree AM

**关键接口对接**：

```c
// nodeIndexscan.c 需要调用：
IndexScanState *ExecInitIndexScan(IndexScan *node, EState *estate, int eflags) {
    // 1. 打开索引 Relation
    // 2. 打开堆 Relation
    // 3. 开始索引扫描: index_beginscan(index_rel, heap_rel, nkeys, key)
}

static TupleTableSlot *ExecIndexScan(PlanState *pstate) {
    IndexScanState *node = (IndexScanState *)pstate;
    
    // 调用 BTree AM
    void *tuple = index_getnext(node->iss_ScanDesc);
    
    // 填充 TupleTableSlot
    return node->iss_ScanTupleSlot;
}
```

**验收标准**：
- `SELECT * FROM t WHERE id = 1` 使用索引扫描
- 范围查询 `WHERE id > 10 AND id < 100` 正确执行

**预估工作量**：~1,500 行，2 周

---

### 0.2.2 连接节点

#### 任务：HashJoin 节点实现

**目标**：实现哈希连接，支持等值连接

**关键数据结构**：

```c
typedef struct HashJoinState {
    PlanState ps;
    
    // 构建端（内表）
    PlanState *hash_build_state;
    HashTable *hashtable;
    
    // 探测端（外表）
    PlanState *hash_probe_state;
    TupleTableSlot *outer_tuple;
    
    // 连接条件
    ExprState *hashclauses;
    ExprState *otherclauses;
} HashJoinState;
```

**执行流程**：

```
1. 构建阶段：
   - 扫描内表，计算哈希值
   - 插入到 HashTable

2. 探测阶段：
   - 扫描外表，计算哈希值
   - 查找 HashTable，匹配的行输出
```

**验收标准**：
- `SELECT * FROM t1 JOIN t2 ON t1.id = t2.id` 正确执行
- 支持多条件连接

**预估工作量**：~2,500 行，3 周

---

#### 任务：NestLoop 节点实现

**目标**：实现嵌套循环连接，支持非等值连接

**预估工作量**：~1,500 行，2 周

---

### 0.2.3 聚合节点

#### 任务：HashAgg 节点实现

**目标**：实现哈希聚合，支持 GROUP BY

**关键数据结构**：

```c
typedef struct AggState {
    PlanState ps;
    
    // 聚合函数
    int numaggs;
    Aggref **aggs;
    
    // 分组哈希表
    HashTable *hashtable;
    
    // 聚合状态
    TupleTableSlot *agg_slot;
    bool agg_done;
} AggState;
```

**执行流程**：

```
1. 聚合阶段：
   - 扫描输入，按分组键计算哈希
   - 更新聚合状态（SUM/COUNT/AVG/MAX/MIN）

2. 输出阶段：
   - 遍历哈希表，输出每个分组的结果
```

**验收标准**：
- `SELECT COUNT(*), SUM(x) FROM t GROUP BY y` 正确执行
- 支持 HAVING 子句

**预估工作量**：~2,000 行，2.5 周

---

### 0.2.4 修饰节点

#### 任务：Sort 节点实现

**目标**：实现排序，支持 ORDER BY

**预估工作量**：~1,500 行，1.5 周

---

#### 任务：Limit 节点实现

**目标**：实现 LIMIT/OFFSET

**预估工作量**：~500 行，1 周

---

### 0.2.5 DML 节点

#### 任务：ModifyTable 节点实现

**目标**：实现 INSERT/UPDATE/DELETE

**关键接口对接**：

```c
static TupleTableSlot *ExecModifyTable(PlanState *pstate) {
    ModifyTableState *node = (ModifyTableState *)pstate;
    
    switch (node->operation) {
        case CMD_INSERT:
            // 调用 heap_insert()
            heap_insert(rel, tuple, ...);
            break;
            
        case CMD_UPDATE:
            // 调用 heap_update()
            heap_update(rel, otid, tuple, ...);
            break;
            
        case CMD_DELETE:
            // 调用 heap_delete()
            heap_delete(rel, otid, ...);
            break;
    }
}
```

**验收标准**：
- `INSERT INTO t VALUES(...)` 正确插入
- `UPDATE t SET x = 1 WHERE y = 2` 正确更新
- `DELETE FROM t WHERE x = 1` 正确删除
- WAL 日志正确记录

**预估工作量**：~2,000 行，2.5 周

---

## 0.3 阶段 0 里程碑

| 里程碑 | 目标 | 验收 SQL |
|--------|------|----------|
| M0.1 | SeqScan 完成 | `SELECT * FROM t` |
| M0.2 | IndexScan 完成 | `SELECT * FROM t WHERE id = 1` |
| M0.3 | HashJoin 完成 | `SELECT * FROM t1 JOIN t2 ON t1.id = t2.id` |
| M0.4 | HashAgg 完成 | `SELECT COUNT(*) FROM t GROUP BY x` |
| M0.5 | ModifyTable 完成 | `INSERT/UPDATE/DELETE` |
| M0.6 | 端到端测试 | 复杂查询 |

---

## 0.4 阶段 0 代码量估算

| 模块 | 预估代码量 | 工期 |
|------|-----------|------|
| SeqScan | ~1,500 行 | 2 周 |
| IndexScan | ~1,500 行 | 2 周 |
| HashJoin | ~2,500 行 | 3 周 |
| NestLoop | ~1,500 行 | 2 周 |
| HashAgg | ~2,000 行 | 2.5 周 |
| Sort | ~1,500 行 | 1.5 周 |
| Limit | ~500 行 | 1 周 |
| ModifyTable | ~2,000 行 | 2.5 周 |
| 测试代码 | ~5,000 行 | - |
| **总计** | **~18,000 行** | **12-16 周** |

---

# 阶段 1：并发控制（关键）

**工期**：4-5 人月
**前置条件**：阶段 0 完成
**目标**：支持多用户并发访问

---

## 1.1 MVCC 实现

### 背景

PostgreSQL 的 MVCC 是核心并发机制：

```
每个元组有 xmin/xmax 两个字段：
- xmin: 插入该元组的事务 ID
- xmax: 删除/更新该元组的事务 ID

可见性判断规则：
- 当前事务可以看到 xmin 已提交且 xmax 未设置或未提交的元组
- 结合快照判断元组对当前事务是否可见
```

### 任务：完善 MVCC 可见性判断

**关键文件**：
- `engineering/src/db/concurrency/mvcc_visibility.c` - 已有框架
- `engineering/src/db/concurrency/mvcc_version.c` - 版本链

**核心实现**：

```c
// 判断元组对当前事务是否可见
bool XidInMVCCSnapshot(
    TransactionId xid,
    Snapshot snapshot,
    TransactionId xmin,
    TransactionId xmax
) {
    // 1. 检查 xid 是否在快照中
    // 2. 检查 xid 是否已提交
    // 3. 检查 xmin/xmax 状态
    
    if (TransactionIdIsCurrentTransactionId(xid)) {
        return true;  // 当前事务可见
    }
    
    if (TransactionIdIsInProgress(xid)) {
        return false;  // 进行中的事务不可见
    }
    
    if (TransactionIdDidCommit(xid)) {
        // 已提交：检查是否在快照之前
        return TransactionIdPrecedes(xid, snapshot->xmin);
    }
    
    return false;  // 已回滚不可见
}
```

**验收标准**：
- 并发事务隔离正确（读已提交、可重复读）
- 快照一致性读正确

**预估工作量**：~3,000 行，3 周

---

### 任务：实现事务快照管理

**核心数据结构**：

```c
typedef struct SnapshotData {
    TransactionId xmin;      // 快照最小事务 ID
    TransactionId xmax;      // 快照最大事务 ID
    TransactionId *xip;      // 活跃事务 ID 数组
    int xcnt;                // 活跃事务数
    TransactionId *subxip;   // 子事务数组
    int subcnt;              // 子事务数
} SnapshotData;
```

**预估工作量**：~2,000 行，2 周

---

## 1.2 锁管理器

### 背景

PostgreSQL 锁层次：

```
表级锁：
- ACCESS SHARE      - SELECT
- ROW SHARE         - SELECT FOR UPDATE
- ROW EXCLUSIVE     - INSERT/UPDATE/DELETE
- SHARE             - CREATE INDEX
- ACCESS EXCLUSIVE  - ALTER TABLE/DROP TABLE

行级锁：
- FOR UPDATE
- FOR SHARE
- FOR NO KEY UPDATE
- FOR KEY SHARE
```

### 任务：实现表级锁

**核心数据结构**：

```c
typedef struct LockManager {
    // 锁哈希表
    HashTable *lock_hash;
    
    // 等待队列
    WaitQueue *wait_queue;
    
    // 死锁检测图
    WaitGraph *wait_graph;
} LockManager;

typedef struct Lock {
    Oid relid;               // 表 OID
    LockMode mode;           // 锁模式
    TransactionId holder;    // 持有者
    LockStatus status;       // 锁状态
} Lock;
```

**核心接口**：

```c
// 获取表锁
int LockAcquire(
    LockManager *mgr,
    Oid relid,
    LockMode mode,
    bool wait,
    uint32_t timeout_ms
);

// 释放表锁
void LockRelease(
    LockManager *mgr,
    Oid relid,
    LockMode mode
);

// 检测死锁
bool DeadLockCheck(
    LockManager *mgr,
    TransactionId xid
);
```

**预估工作量**：~4,000 行，4 周

---

### 任务：实现行级锁

**预估工作量**：~3,000 行，3 周

---

### 任务：实现死锁检测

**算法**：等待图 + 深度优先搜索

```c
bool DeadLockCheck(LockManager *mgr, TransactionId xid) {
    // 1. 构建等待图
    // 2. DFS 检测环
    // 3. 返回是否有死锁
    
    WaitGraph *graph = BuildWaitGraph(mgr);
    return HasCycle(graph, xid);
}
```

**预估工作量**：~1,500 行，2 周

---

## 1.3 事务管理

### 任务：实现 2PC（两阶段提交）

**核心接口**：

```c
// 阶段 1：准备
int PrepareTransaction(TransactionState *txn) {
    // 1. 刷所有脏页
    // 2. 记录 PREPARE 日志
    // 3. 返回事务 ID
}

// 阶段 2：提交/回滚
int CommitPrepared(TransactionId xid) {
    // 记录 COMMIT PREPARED 日志
}

int RollbackPrepared(TransactionId xid) {
    // 记录 ROLLBACK PREPARED 日志
}
```

**预估工作量**：~2,000 行，2 周

---

## 1.4 阶段 1 里程碑

| 里程碑 | 目标 | 验收场景 |
|--------|------|----------|
| M1.1 | MVCC 可见性 | 并发读写一致性 |
| M1.2 | 快照管理 | 可重复读隔离级别 |
| M1.3 | 表级锁 | 并发 DDL 互斥 |
| M1.4 | 行级锁 | 并发 DML 正确 |
| M1.5 | 死锁检测 | 死锁自动回滚 |
| M1.6 | 2PC | 分布式事务正确 |

---

## 1.5 阶段 1 代码量估算

| 模块 | 预估代码量 | 工期 |
|------|-----------|------|
| MVCC 可见性 | ~3,000 行 | 3 周 |
| 快照管理 | ~2,000 行 | 2 周 |
| 表级锁 | ~4,000 行 | 4 周 |
| 行级锁 | ~3,000 行 | 3 周 |
| 死锁检测 | ~1,500 行 | 2 周 |
| 2PC | ~2,000 行 | 2 周 |
| 测试代码 | ~5,000 行 | - |
| **总计** | **~20,500 行** | **16-20 周** |

---

# 阶段 2：恢复能力（可靠性）

**工期**：4-5 人月
**前置条件**：阶段 1 完成
**目标**：保证数据安全

---

## 2.1 WAL 完整实现

### 任务：实现完整恢复流程

**核心流程**：

```
崩溃恢复流程：
1. 读取控制文件，找到检查点位置
2. 从检查点开始读取 WAL
3. Redo: 重放所有已提交事务的修改
4. Undo: 回滚所有未提交事务的修改（如果支持）
5. 恢复完成，开始新检查点
```

**关键实现**：

```c
void PerformWalRecovery(const char *data_dir) {
    // 1. 读取控制文件
    ControlFile *ctrl = ReadControlFile(data_dir);
    
    // 2. 定位检查点
    XLogRecPtr checkpoint_ptr = ctrl->checkPoint;
    
    // 3. 读取 WAL 记录
    XLogReader *reader = XLogReaderInit(data_dir);
    XLogRecord *record;
    
    while ((record = XLogReadRecord(reader)) != NULL) {
        // 4. 重放 WAL 记录
        RedoRecord(record);
        
        // 5. 遇到恢复结束标记
        if (record->type == XLOG_RECOVERY_END) {
            break;
        }
    }
    
    // 6. 写入新的检查点
    CreateCheckPoint(data_dir);
}
```

**预估工作量**：~5,000 行，4 周

---

### 任务：实现检查点优化

**策略**：

```
完全检查点：
- 刷所有脏页
- 记录检查点位置
- 清理旧 WAL 文件

增量检查点（优化）：
- 只刷上次检查点后的脏页
- 分散刷页负载
```

**预估工作量**：~2,000 行，2 周

---

## 2.2 备份恢复

### 任务：实现基础备份

**流程**：

```bash
# 1. 开始备份模式
pg_backup_start()

# 2. 复制数据文件
cp -r $DATA_DIR $BACKUP_DIR

# 3. 结束备份模式
pg_backup_stop()
```

**预估工作量**：~2,000 行，2 周

---

### 任务：实现 PITR（时间点恢复）

**流程**：

```
1. 恢复基础备份
2. 重放 WAL 到目标时间点
3. 恢复完成
```

**预估工作量**：~3,000 行，3 周

---

## 2.3 高可用

### 任务：实现流复制

**架构**：

```
主库：
- WAL Sender 进程
- 发送 WAL 记录到备库

备库：
- WAL Receiver 进程
- 接收并重放 WAL
- 提供只读查询
```

**核心接口**：

```c
// 主库端
void WalSenderMain(void) {
    while (true) {
        // 1. 读取 WAL 记录
        XLogRecord *rec = ReadWalRecord();
        
        // 2. 发送到备库
        SendWalRecord(rec);
        
        // 3. 等待备库确认（同步复制）
        WaitStandbyAck();
    }
}

// 备库端
void WalReceiverMain(void) {
    while (true) {
        // 1. 接收 WAL 记录
        XLogRecord *rec = ReceiveWalRecord();
        
        // 2. 重放
        RedoRecord(rec);
        
        // 3. 发送确认
        SendAck();
    }
}
```

**预估工作量**：~5,000 行，4 周

---

### 任务：实现主备切换

**流程**：

```
planned switch（计划切换）：
1. 主库停止写入
2. 等待备库同步完成
3. 备库提升为主库

failover（故障切换）：
1. 检测主库故障
2. 备库提升为主库
3. 处理脑裂风险
```

**预估工作量**：~2,000 行，2 周

---

## 2.4 阶段 2 里程碑

| 里程碑 | 目标 | 验收场景 |
|--------|------|----------|
| M2.1 | WAL 恢复 | 崩溃后数据不丢失 |
| M2.2 | 检查点 | 定期自动检查点 |
| M2.3 | 基础备份 | 全量备份正确 |
| M2.4 | PITR | 恢复到指定时间点 |
| M2.5 | 流复制 | 主备数据同步 |
| M2.6 | 主备切换 | 高可用切换 |

---

## 2.5 阶段 2 代码量估算

| 模块 | 预估代码量 | 工期 |
|------|-----------|------|
| WAL 恢复 | ~5,000 行 | 4 周 |
| 检查点优化 | ~2,000 行 | 2 周 |
| 基础备份 | ~2,000 行 | 2 周 |
| PITR | ~3,000 行 | 3 周 |
| 流复制 | ~5,000 行 | 4 周 |
| 主备切换 | ~2,000 行 | 2 周 |
| 测试代码 | ~5,000 行 | - |
| **总计** | **~24,000 行** | **17-21 周** |

---

# 阶段 3：查询优化（性能）

**工期**：3-4 人月
**前置条件**：阶段 0、1 完成
**目标**：支持复杂查询

---

## 3.1 Selinger 代价模型

### 任务：实现完整的代价估算

**代价公式**：

```
SeqScan 代价 = seq_page_cost * N_pages + cpu_tuple_cost * N_tuples

IndexScan 代价 = 
    index_page_cost * N_index_pages +
    cpu_index_tuple_cost * N_index_tuples +
    random_page_cost * N_heap_fetches

HashJoin 代价 = 
    构建代价 + 探测代价 + 输出代价
```

**核心实现**：

```c
Cost cost_seqscan(
    PlannerInfo *root,
    Path *path,
    RelOptInfo *rel
) {
    Cost startup = 0;
    Cost run_cost = 0;
    
    // 页面读取代价
    double pages = rel->pages;
    run_cost += seq_page_cost * pages;
    
    // 元组处理代价
    double tuples = rel->tuples;
    run_cost += cpu_tuple_cost * tuples;
    
    // 过滤代价
    if (path->pathkeys) {
        run_cost += cpu_operator_cost * tuples * nkeys;
    }
    
    return startup + run_cost;
}
```

**预估工作量**：~3,000 行，3 周

---

### 任务：统计信息收集

**核心数据结构**：

```c
typedef struct RelStatistics {
    double num_tuples;        // 元组数
    double num_pages;         // 页面数
    int rel_pages;            // 实际页面数
    
    // 列统计
    ColumnStatistics *columns;
    int ncolumns;
} RelStatistics;

typedef struct ColumnStatistics {
    Oid attid;                // 列号
    double ndistinct;         // 唯一值数
    double nullfrac;          // NULL 比例
    Datum min_value;          // 最小值
    Datum max_value;          // 最大值
    
    // 直方图
    Datum *histogram;
    int histogram_nvalues;
    
    // MCV（最常见值）
    Datum *mcv_values;
    double *mcv_freqs;
    int nmcv;
} ColumnStatistics;
```

**预估工作量**：~2,500 行，2.5 周

---

## 3.2 优化规则

### 任务：实现查询重写规则

**规则列表**：

```
1. 谓词下推（Predicate Pushdown）
   SELECT * FROM (SELECT * FROM t WHERE x = 1) WHERE y = 2
   → SELECT * FROM t WHERE x = 1 AND y = 2

2. 投影下推（Projection Pushdown）
   减少中间结果的列数

3. 连接顺序优化（Join Reordering）
   基于代价选择最佳连接顺序

4. 子查询展开（Subquery Pullup）
   SELECT * FROM t WHERE x IN (SELECT y FROM t2)
   → SELECT * FROM t JOIN t2 ON t.x = t2.y
```

**预估工作量**：~3,000 行，3 周

---

## 3.3 并行查询

### 任务：实现 Gather 节点

**架构**：

```
Gather
  ├── Worker 1: Parallel SeqScan
  ├── Worker 2: Parallel SeqScan
  └── Worker 3: Parallel SeqScan
```

**核心实现**：

```c
typedef struct GatherState {
    PlanState ps;
    
    // Worker 管理
    int num_workers;
    WorkerInfo *workers;
    
    // 结果收集
    TupleQueue *queue;
} GatherState;

static TupleTableSlot *ExecGather(PlanState *pstate) {
    GatherState *node = (GatherState *)pstate;
    
    // 从 Worker 队列读取结果
    TupleTableSlot *slot = TupleQueuePop(node->queue);
    
    return slot;
}
```

**预估工作量**：~3,000 行，3 周

---

### 任务：实现并行 SeqScan

**策略**：

```
将表划分为多个范围：
- Worker 1: pages [0, N/3)
- Worker 2: pages [N/3, 2N/3)
- Worker 3: pages [2N/3, N)
```

**预估工作量**：~2,000 行，2 周

---

### 任务：实现并行 HashJoin

**策略**：

```
1. 所有 Worker 共享一个 Hash 表
2. 每个 Worker 探测不同范围的元组
```

**预估工作量**：~2,500 行，2.5 周

---

## 3.4 阶段 3 里程碑

| 里程碑 | 目标 | 验收 SQL |
|--------|------|----------|
| M3.1 | 代价模型 | EXPLAIN 显示正确代价 |
| M3.2 | 统计信息 | ANALYZE 收集统计 |
| M3.3 | 查询重写 | 复杂查询优化 |
| M3.4 | 并行 SeqScan | 多核并行扫描 |
| M3.5 | 并行 HashJoin | 多核并行连接 |

---

## 3.5 阶段 3 代码量估算

| 模块 | 预估代码量 | 工期 |
|------|-----------|------|
| 代价模型 | ~3,000 行 | 3 周 |
| 统计信息 | ~2,500 行 | 2.5 周 |
| 查询重写 | ~3,000 行 | 3 周 |
| Gather 节点 | ~3,000 行 | 3 周 |
| 并行 SeqScan | ~2,000 行 | 2 周 |
| 并行 HashJoin | ~2,500 行 | 2.5 周 |
| 测试代码 | ~4,000 行 | - |
| **总计** | **~20,000 行** | **13-17 周** |

---

# 阶段 4：运维能力（可管理性）

**工期**：3-4 人月
**前置条件**：阶段 0、1、2 完成
**目标**：可生产运维

---

## 4.1 监控系统

### 任务：实现性能指标收集

**核心指标**：

```c
typedef struct PgStat {
    // 连接统计
    int num_backends;
    int max_backends;
    
    // 事务统计
    uint64 xact_commit;
    uint64 xact_rollback;
    
    // 块统计
    uint64 blks_read;
    uint64 blks_hit;
    
    // 元组统计
    uint64 tuples_returned;
    uint64 tuples_fetched;
    uint64 tuples_inserted;
    uint64 tuples_updated;
    uint64 tuples_deleted;
    
    // 锁统计
    int locks;
    int lock_waiters;
    
    // 检查点统计
    uint64 checkpoint_timed;
    uint64 checkpoint_requested;
    double checkpoint_write_time;
    double checkpoint_sync_time;
} PgStat;
```

**预估工作量**：~2,500 行，2.5 周

---

### 任务：实现告警机制

**告警规则**：

```
- 连接数接近上限
- 事务回滚率过高
- Buffer 命中率过低
- 锁等待时间过长
- 检查点频率异常
```

**预估工作量**：~1,500 行，1.5 周

---

## 4.2 维护工具

### 任务：实现 VACUUM

**目标**：回收已删除元组的空间

**流程**：

```
1. 扫描表，识别死元组（xmax 已提交且对所有人不可见）
2. 更新空闲空间映射（FSM）
3. 更新可见性映射（VM）
4. 清理索引
```

**核心实现**：

```c
void lazy_vacuum_rel(Relation rel) {
    // 1. 计算需要清理的范围
    BlockNumber nblocks = RelationGetNumberOfBlocks(rel);
    
    // 2. 扫描并标记死元组
    for (BlockNumber blkno = 0; blkno < nblocks; blkno++) {
        Page page = BufferGetPage(buf);
        
        // 检查每个元组
        for (int i = 0; i < PageGetMaxOffsetNumber(page); i++) {
            HeapTupleHeader htup = PageGetItem(page, lp);
            
            if (HeapTupleSatisfiesVacuum(htup) == HEAPTUPLE_DEAD) {
                // 标记为可回收
                MarkDeadTuple(page, i);
            }
        }
    }
    
    // 3. 清理索引
    lazy_vacuum_indexes(rel);
    
    // 4. 更新统计
    vacuum_update_stats(rel);
}
```

**预估工作量**：~4,000 行，4 周

---

### 任务：实现 ANALYZE

**目标**：收集统计信息

**预估工作量**：~2,000 行，2 周

---

### 任务：实现 REINDEX

**目标**：重建索引

**预估工作量**：~1,500 行，1.5 周

---

## 4.3 备份恢复工具

### 任务：实现 pg_dump/pg_restore

**预估工作量**：~3,000 行，3 周

---

### 任务：实现配置管理

**核心功能**：

```
- postgresql.conf 动态加载
- pg_hba.conf 热更新
- ALTER SYSTEM SET 命令
```

**预估工作量**：~1,500 行，1.5 周

---

## 4.4 阶段 4 里程碑

| 里程碑 | 目标 | 验收场景 |
|--------|------|----------|
| M4.1 | 性能指标 | pg_stat_* 视图可用 |
| M4.2 | 告警机制 | 异常自动告警 |
| M4.3 | VACUUM | 空间自动回收 |
| M4.4 | ANALYZE | 统计信息更新 |
| M4.5 | 备份恢复 | 全量/增量备份 |

---

## 4.5 阶段 4 代码量估算

| 模块 | 预估代码量 | 工期 |
|------|-----------|------|
| 性能指标 | ~2,500 行 | 2.5 周 |
| 告警机制 | ~1,500 行 | 1.5 周 |
| VACUUM | ~4,000 行 | 4 周 |
| ANALYZE | ~2,000 行 | 2 周 |
| REINDEX | ~1,500 行 | 1.5 周 |
| pg_dump/pg_restore | ~3,000 行 | 3 周 |
| 配置管理 | ~1,500 行 | 1.5 周 |
| 测试代码 | ~4,000 行 | - |
| **总计** | **~20,000 行** | **13-17 周** |

---

# 总结

## 总工作量汇总

| 阶段 | 代码量 | 工期 | 累计工期 |
|------|--------|------|----------|
| 阶段 0：执行引擎核心 | ~18,000 行 | 12-16 周 | 12-16 周 |
| 阶段 1：并发控制 | ~20,500 行 | 16-20 周 | 28-36 周 |
| 阶段 2：恢复能力 | ~24,000 行 | 17-21 周 | 45-57 周 |
| 阶段 3：查询优化 | ~20,000 行 | 13-17 周 | 58-74 周 |
| 阶段 4：运维能力 | ~20,000 行 | 13-17 周 | **71-91 周** |
| **总计** | **~102,500 行** | - | **17-22 人月** |

---

## 与 PostgreSQL 对比

| 维度 | PostgreSQL | 本项目完成时 |
|------|-----------|-------------|
| 执行引擎 | 77,267 行 | ~50,000 行 |
| Access Method | 162,264 行 | ~80,000 行 |
| Buffer Pool | 9,620 行 | ~6,000 行 |
| 并发控制 | ~30,000 行 | ~20,000 行 |
| WAL/恢复 | 43,322 行 | ~25,000 行 |
| 查询优化 | ~40,000 行 | ~20,000 行 |
| 运维工具 | ~30,000 行 | ~20,000 行 |
| **总计** | ~290,000 行 | ~200,000 行 |

**覆盖度**：约 70% 的核心功能

---

## 关键成功因素

1. **阶段性验收**：每个阶段完成后进行端到端验收
2. **自动化测试**：每个模块都有完整的单元测试和集成测试
3. **性能基准**：建立性能基准，监控关键指标
4. **文档同步**：代码和文档同步更新

---

## 下一步行动

1. 完成阶段 0 的 SeqScan 节点实现
2. 建立测试框架，验证每个里程碑
3. 开始阶段 1 的 MVCC 可见性判断

---

*路线图版本：v1.0*
*创建日期：2026-07-15*