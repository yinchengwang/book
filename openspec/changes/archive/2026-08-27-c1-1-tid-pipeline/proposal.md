# C1-1 关系模态 TID 管道修复 Proposal

## Why

差距分析 02 卷发现本系统最严重缺陷：`nodeModifyTable.c:70-75`（UPDATE）与 `:96-99`（DELETE）构造 TID 时硬编码"块 0、偏移 24"——无论目标行在哪，更新删除都作用于错误位置，多行表上 **UPDATE/DELETE 会改错行**。根源是扫描算子没有把物理行位置放进 slot（`nodeSeqscan.c:242` `ExecCopyTupleToSlot(slot, tuple, NULL)` 第三参数传 NULL）。同时 DML 失败静默吞没（`:62-64,117` 照常 `mt_processed++`）。

## What Changes

- TupleTableSlot 增加 `tts_tid`（ItemPointerData：block + offset）字段
- heap 层返回真实物理位置：`heap_insert` 回填 tid，扫描路径 `table_getnext` 填充 slot 的 tid
- `nodeModifyTable.c` UPDATE/DELETE 使用真实 tid（删除硬编码）
- DML 错误传播：heap_insert/update/delete 失败 → 中止执行并返回 DBERR（计数不再虚增）
- 复现测试先行：多行表 UPDATE/DELETE 目标行验证（`tid_pipeline_bug_02rel` 命名）

## Capabilities

| 能力 | 交付 |
|------|------|
| TID 管道 | 扫描→slot→ModifyTable 全链路真实物理位置 |
| DML 正确性 | 多行表 UPDATE/DELETE 改对行的集成测试通过 |
| 错误传播 | DML 失败返回错误，mt_processed 真实 |

## Impact

- 修改：nodeModifyTable.c、nodeSeqscan.c、heapam.c、executor.c（slot 结构）
- 新增：tid 集成测试
- 预计 3-4 个 commit
