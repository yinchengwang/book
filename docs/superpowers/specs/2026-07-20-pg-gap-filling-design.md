# 关系型数据库 PG 对标补齐设计

> 日期：2026-07-20
> 关联：docs/superpowers/plans/2026-07-19-pg-arch-refactor-plan.md（三档已完成）

**目标：** 将当前关系型实现从教学原型（45% PG 对标）补齐到生产可用（~100% PG 对标），覆盖 BTree 分裂、MVCC 版本链、CLOG 持久化、HOT、TOAST、并行执行、2PC、WAL 归档、视图/规则/继承等 22 个模块。

**总体架构：** 在现有 PG 风格分层架构上深度补齐，保持 catalog/buffer pool/WAL/executor/planner 已有接口不变，扩展实现深度。

**周期：** 23-29 周，分 6 个 Wave，按依赖关系串行执行。

---

## 1. 依赖关系与 Wave 划分

```
Wave 1（基础）
  ├── BTree 分裂 + Latch
  ├── CLOG 持久化
  └── 脏页刷盘 + 后台写进程

Wave 2（版本链核心）
  ├── MVCC 版本链 ← 依赖 CLOG
  ├── HOT ← 依赖 MVCC 版本链
  └── VACUUM ← 依赖 MVCC 版本链 + HOT

Wave 3（存储扩展）
  ├── TOAST ← 依赖 Heap 改造
  ├── 预读 ← 依赖 Buffer Pool
  ├── 统计信息 ANALYZE ← 依赖 catalog 持久化
  ├── 序列/自增 ← 独立
  ├── 外键 + CHECK ← 依赖 catalog
  └── Double Write ← 独立

Wave 4（查询加速）
  ├── 并行执行 ← 依赖 Buffer Pool
  ├── 分区表 ← 依赖 catalog
  └── MergeJoin ← 独立

Wave 5（事务深度）
  ├── 2PC ← 依赖 WAL + CLOG
  └── 子事务/保存点 ← 依赖 CLOG

Wave 6（高级特性）
  ├── WAL 归档 + PITR ← 依赖 WAL
  ├── 表达式索引 + 部分索引 ← 依赖 BTree
  ├── 视图/物化视图 ← 依赖 planner
  ├── 规则系统 ← 依赖 parser + planner
  └── 继承表 ← 依赖 catalog
```

---

## 2. Wave 1 详细设计：基础

### 2.1 BTree 页面分裂 + 并发 Latch

**文件：**
- 修改：`engineering/src/db/storage/access/btree/btreeam.c`
- 修改：`engineering/include/db/access/btree/btpage.h`
- 新增：`engineering/src/db/storage/access/btree/btree_split.c`
- 新增：`engineering/include/db/access/btree/btree_split.h`

**页面格式：** BTPageHeader 增加 `btpo_rightlink`（右兄弟页指针）和 `btpo_cycleid`（并发分裂标识），`btpo_flags` 扩展支持 `BTP_ROOT/BTP_LEAF/BTP_INTERNAL/BTP_HALF_DEAD`

**分裂算法：**
- 叶节点满时（`btpo_count >= BTREE_MAX_KEYS`），找中间键值（`btree_split_find_pivot`），右半部分搬到新页，更新 `btpo_next/btpo_rightlink`，插入父节点
- 内部节点满时，中间键值提升到父节点，左右页分裂
- 根节点满时，创建新根节点，原根拆为左右孩子
- 递归向上分裂直到根

**并发 Latch：**
- 每个页面一个 `rwlock_t`，读锁共享，写锁互斥
- 扫描时加读锁，通过 `btpo_rightlink` 跳页，解锁前一页
- 插入时加写锁，按页面号排序获取避免死锁
- 并发分裂时的"右移"策略：扫描发现 `btpo_cycleid` 标识分裂，通过 `btpo_rightlink` 跳到右页继续

**测试：**
- 插入 2000 条数据触发多层分裂，验证数据完整
- 4 线程并发插入 10000 条，无死锁

### 2.2 CLOG 持久化

**文件：**
- 新增：`engineering/src/db/storage/txn/clog.c`
- 新增：`engineering/include/db/storage/txn/clog.h`
- 修改：`engineering/src/db/storage/txn/mvcc.c`（替换内存实现为 CLOG）
- 修改：`engineering/src/db/storage/CMakeLists.txt`

**文件格式：** `pg_xact/` 下 256KB 段文件，每事务 2bit：00=IN_PROGRESS, 01=COMMITTED, 10=ABORTED, 11=PREPARED

**SLRU 缓存：** LRU 缓存最近访问的 CLOG 页面（默认 64 页），未命中从文件读取，脏页延迟写回

**API：**
```c
int clog_init(const char *data_dir);
void clog_shutdown(void);
int clog_set_status(TransactionId xid, transaction_status_t status);
transaction_status_t clog_get_status(TransactionId xid);
void clog_flush(void);
int clog_checkpoint(XLogRecPtr checkpoint_lsn);
```

**集成：** `mvcc_mark_committed/mvcc_mark_aborted/mvcc_get_status` 改为调 CLOG

### 2.3 脏页刷盘 + 后台写进程

**文件：**
- 修改：`engineering/src/db/storage/buffer/bufmgr.c`
- 新增：`engineering/src/db/storage/buffer/bgwriter.c`
- 新增：`engineering/include/db/storage/buffer/bgwriter.h`

**后台写进程：** 独立线程，周期（默认 100ms）扫描脏页，每批写 10 页，写前确保 WAL 已刷盘

**同步机制：** 写盘前检查 LSN >= WAL flush LSN，写完后清除 `BM_DIRTY`

**GUC 参数：** `bgwriter_delay`（100ms）、`bgwriter_lru_maxpages`（10）、`bgwriter_lru_multiplier`（2.0）

---

## 3. Wave 2 详细设计：版本链核心

### 3.1 MVCC 版本链

**文件：**
- 修改：`engineering/src/db/storage/access/heap/heapam.c`
- 修改：`engineering/include/db/access/heapam.h`
- 新增：`engineering/src/db/storage/access/heap/heap_update.c`
- 新增：`engineering/src/db/storage/access/heap/heap_version.c`

**元组头改造：** `HeapTupleHeaderData` 增加 `t_ctid`（`ItemPointerData`）、`t_infomask`（32bit）、`t_hoff`（元组头偏移）

**infomask 标志：** `HEAP_HASNULL`、`HEAP_HASVARWIDTH`、`HEAP_XMIN_COMMITTED`、`HEAP_XMIN_INVALID`、`HEAP_XMAX_COMMITTED`、`HEAP_XMAX_INVALID`、`HEAP_UPDATED`、`HEAP_HOT_UPDATED`、`HEAP_ONLY_TUPLE`

**heap_update 流程：**
1. 创建新版本元组，插入同页或新页
2. 旧元组 `t_ctid` → 新位置，`t_xmax` = 当前 XID，设 `HEAP_UPDATED`
3. 新元组 `t_xmin` = 当前 XID，`t_ctid` = 自己
4. 可见性判断：遍历 `t_ctid` 链找到最新版本

**heap_delete 流程：** 设 `t_xmax` = 当前 XID，`t_ctid` = 自己，不物理删除

### 3.2 HOT（Heap-Only Tuple）

**文件：**
- 修改：`engineering/src/db/storage/access/heap/heapam.c`
- 修改：`engineering/include/db/access/heapam.h`

**HOT 判定：** UPDATE 未修改索引列 + 新版本可放入同页

**HOT 流程：**
1. 同页追加新版本，旧版本 `t_ctid` → 新偏移
2. 旧 `t_infomask` |= `HEAP_HOT_UPDATED`，新 `t_infomask` |= `HEAP_ONLY_TUPLE`
3. 不修改索引项

**HOT 索引扫描：** 索引 ctid 指向根元组，通过 `t_ctid` 链追踪到最新可见版本

### 3.3 VACUUM

**文件：**
- 修改：`engineering/src/db/tools/vacuum/vacuum.c`
- 新增：`engineering/src/db/tools/vacuum/vacuum_dead.c`
- 新增：`engineering/src/db/tools/vacuum/vacuum_index.c`

**扫描阶段：** 遍历页面，检查每个元组是否对任何活跃事务不可见 → 标记死元组

**清理阶段：** 页面内死元组空间标记可重用，活元组压缩合并空闲空间，全空页面标记可回收

**索引清理：** 遍历索引，删除指向死元组的索引项；HOT 链全死时删除索引项

**冻结：** `t_xmin` 早于 `oldest_xid_limit` 时标记 FROZEN

---

## 4. Wave 3 详细设计：存储扩展

### 4.1 TOAST

**文件：**
- 新增：`engineering/src/db/storage/access/toast/toast.c`
- 新增：`engineering/include/db/access/toast.h`

**触发：** 元组 > 2KB 时自动处理，先 pglz 压缩，压缩后仍超限则外存到 `pg_toast_<oid>` 表

**测试：** 插入 10KB TEXT 验证外存 + 读取正确

### 4.2 预读

**文件：**
- 修改：`engineering/src/db/storage/buffer/bufmgr.c`

**API：** `buf_read_ahead(relfilenode, start_blkno, n_pages)` 异步预读 N 页，标记 `BM_READ_AHEAD`

**触发：** 全表扫描每访问 N 页预读 N 页，索引扫描按 `btpo_next` 链预读

### 4.3 统计信息 ANALYZE

**文件：**
- 新增：`engineering/src/db/tools/analyze/analyze.c`
- 新增：`engineering/include/db/tools/analyze.h`
- 修改：`engineering/src/db/sql/planner.c`

**pg_statistic 表：** `starelid/staattnum/stanullfrac/stadistinct/stakindN/stanumbersN/stavaluesN`

**采样：** 扫描 `default_statistics_target` 行，计算 MCV、等深直方图、NULL 比例

**集成：** `planner_get_table_stats` 读 pg_statistic，`estimate_selectivity` 用直方图

### 4.4 序列/自增

**文件：**
- 新增：`engineering/src/db/core/sequence.c`
- 新增：`engineering/include/db/sequence.h`

**文件：** `pg_sequence/` 目录，每序列一个文件

**API：** `sequence_create/nextval/currval/setval`

**SERIAL：** `CREATE TABLE t (id SERIAL)` → 自动 `CREATE SEQUENCE` + `DEFAULT nextval`

### 4.5 外键 + CHECK

**文件：**
- 修改：`engineering/src/db/storage/catalog/catalog.c`
- 修改：`engineering/src/db/sql/nodeModifytable.c`

**外键：** INSERT/UPDATE 子表时检查父表主键存在，DELETE 父表时检查子表无引用

**CHECK：** INSERT/UPDATE 时求值约束表达式

**pg_constraint 表：** 约束名称/类型/相关表 OID/表达式

### 4.6 Double Write

**文件：**
- 新增：`engineering/src/db/storage/buffer/double_write.c`
- 新增：`engineering/include/db/storage/buffer/double_write.h`

**Buffer：** 128 页内存缓冲区，刷脏页时先批量写 Double Write 文件（顺序 I/O），再写实际位置

**文件：** `pg_dw/` 目录，默认 16MB

---

## 5. Wave 4 详细设计：查询加速

### 5.1 并行执行

**文件：**
- 修改：`engineering/src/db/sql/nodeGather.c`
- 修改：`engineering/src/db/sql/nodeSeqscan.c`
- 新增：`engineering/include/db/sql/parallel.h`

**ParallelScanState：** 共享 `next_blockno/nblocks/nworkers/lock`

**Gather 节点：** N 个 worker 各扫一部分页面，结果通过 TupleQueue 汇聚

### 5.2 分区表

**文件：**
- 修改：`engineering/src/db/storage/catalog/catalog.c`
- 新增：`engineering/src/db/sql/nodeAppend.c`

**pg_partition 表：** `partrelid/partkind/partexpr/partbound`

**分区裁剪：** planner 检查 WHERE 条件中的分区键，只扫描匹配分区

### 5.3 MergeJoin

**文件：**
- 新增：`engineering/src/db/sql/nodeMergejoin.c`
- 新增：`engineering/include/db/sql/nodeMergejoin.h`

**算法：** 同时扫描内外两个有序列表，比较连接键，相等输出匹配对

---

## 6. Wave 5 详细设计：事务深度

### 6.1 2PC

**文件：**
- 修改：`engineering/src/db/storage/txn/txn_xact.c`
- 修改：`engineering/src/db/storage/wal/wal.c`

**PREPARE：** 写 PREPARE WAL 日志（含所有修改），CLOG 标记 PREPARED，释放锁不释放 XID

**COMMIT/ROLLBACK PREPARED：** 写对应日志，CLOG 标记，崩溃恢复时自动回滚未完成 PREPARED

### 6.2 子事务/保存点

**文件：**
- 修改：`engineering/src/db/storage/txn/txn_xact.c`

**SubXID：** 从事务 ID 池分配，CLOG 支持子事务状态

**事务栈：** 父事务栈底，子事务栈顶，ROLLBACK TO SAVEPOINT 弹出栈顶子事务并 Undo

---

## 7. Wave 6 详细设计：高级特性

### 7.1 WAL 归档 + PITR

**文件：**
- 修改：`engineering/src/db/storage/wal/wal.c`
- 新增：`engineering/src/db/storage/wal/wal_archive.c`

**段文件：** 16MB/段，文件名 `<timeline>_<segno>`，满时自动切换

**归档：** `archive_command` 配置项，段切换时执行

**PITR：** 从全量备份恢复，回放归档日志到目标时间点

### 7.2 表达式索引 + 部分索引

**文件：**
- 修改：`engineering/src/db/storage/access/btree/btreeam.c`
- 修改：`engineering/include/db/access/amapi.h`

**表达式索引：** `pg_index.indexprs` 存储表达式树，插入/更新时求值

**部分索引：** `pg_index.indpred` 存储谓词，插入/更新时检查

### 7.3 视图 + 物化视图

**文件：**
- 修改：`engineering/src/db/storage/catalog/catalog.c`
- 修改：`engineering/src/db/sql/planner.c`

**视图：** `pg_views` 表存储查询定义，查询时展开为子查询

**物化视图：** 存储结果到物理表，`REFRESH` 时截断重执行

### 7.4 规则系统

**文件：**
- 新增：`engineering/src/db/sql/rewriter.c`
- 新增：`engineering/include/db/sql/rewriter.h`

**规则类型：** ON SELECT（视图底层）、ON INSERT/UPDATE/DELETE（DO INSTEAD/DO ALSO）

**流程：** 解析 → 重写器匹配 pg_rules → 生成新查询树 → planner

### 7.5 继承表

**文件：**
- 修改：`engineering/src/db/storage/catalog/catalog.c`

**pg_inherits 表：** `inhrelid/inhparent/inhseqno`

**查询时：** 父表查询自动包含子表，`ONLY` 排除子表

---

## 8. 当前 PG 对标与补齐目标

| 模块 | 当前 | Wave | 补齐后 | 关键缺失 |
|------|------|------|--------|---------|
| BTree AM | 25% | W1 | 100% | 分裂 + Latch |
| CLOG | 0% | W1 | 100% | 持久化 |
| Buffer Pool | 60% | W1/W3 | 100% | 刷盘 + 预读 + Double Write |
| MVCC 版本链 | 35% | W2 | 100% | t_ctid + infomask |
| HOT | 0% | W2 | 100% | 同页 HOT 更新 |
| VACUUM | 20% | W2 | 100% | 死元组清理 + freeze |
| TOAST | 0% | W3 | 100% | 大字段外存 |
| 统计信息 | 0% | W3 | 100% | pg_statistic |
| 序列 | 0% | W3 | 100% | SERIAL + nextval |
| 约束 | 30% | W3 | 100% | FK + CHECK 强制 |
| 并行执行 | 20% | W4 | 100% | Parallel SeqScan |
| 分区表 | 30% | W4 | 100% | 分区裁剪 |
| MergeJoin | 0% | W4 | 100% | 归并连接 |
| 2PC | 10% | W5 | 100% | PREPARE + COMMIT PREPARED |
| 子事务 | 0% | W5 | 100% | SAVEPOINT |
| WAL 归档 | 0% | W6 | 100% | 段文件 + PITR |
| 表达式索引 | 0% | W6 | 100% | 函数索引 |
| 视图 | 30% | W6 | 100% | CREATE VIEW |
| 规则系统 | 0% | W6 | 100% | 查询重写 |
| 继承表 | 0% | W6 | 100% | INHERITS |

**总体：45% → ~100%**

---

## 9. 测试策略

每个 Wave 完成后，通过以下测试验证：
1. **单元测试**：每个新增模块独立测试
2. **集成测试**：Wave 内模块联调
3. **端到端测试**：CREATE TABLE → INSERT → SELECT → UPDATE → DELETE 完整链路
4. **并发测试**：多线程并发读写，验证无死锁
5. **持久化测试**：写入后重启，验证数据完整
6. **压力测试**：大数据量验证正确性和性能

---

## 10. 文件变更总览

| Wave | 新增文件 | 修改文件 | 新增代码（预估） |
|------|---------|---------|----------------|
| W1 | 5 | 6 | ~3000 |
| W2 | 4 | 5 | ~4000 |
| W3 | 8 | 8 | ~5000 |
| W4 | 3 | 4 | ~3000 |
| W5 | 0 | 3 | ~4000 |
| W6 | 3 | 5 | ~4000 |
| **合计** | **23** | **31** | **~23000** |