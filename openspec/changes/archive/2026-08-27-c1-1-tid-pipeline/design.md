# C1-1 关系模态 TID 管道修复 设计文档

## 设计目标

修复 02 卷识别的本系统最严重缺陷：`nodeModifyTable.c` 在 UPDATE/DELETE 时硬编码 TID `(block=0, offset=24)`，无论目标行在哪都改块 0 偏移 24。

## 方案

### 1. 复现测试先行（T1）

测试场景：建表 → 插入 3 行 → UPDATE 第二行 → 验证只有第二行被改、第一/三行不变。修复前会 FAIL（第二行没被改，块 0 偏移 24 的位置出现意外值或不变）。

### 2. TupleTableSlot 增加 tts_tid（T2）

`TupleTableSlot` 增加 `tts_tid` 字段（ItemPointerData：block + offset，6 字节）。

扫描路径（`ExecSeqScan` / `table_getnext` / `heap_getnext`）填充 tts_tid。

### 3. heap 层回填 tid（T3）

- `heap_insert` 在新元组写入页面后，回填 `(blocknum, offset)` 给调用方（通过 out 参数或额外接口）
- `heap_update/delete` 接受真实 tid（已有 tid 参数，正确解析即可）

### 4. nodeModifyTable 使用真实 tid（T4）

删除 `:70-75`（UPDATE）与 `:96-99`（DELETE）的硬编码 tid，改为 `slot->tts_tid`。

### 5. DML 错误传播（T5）

`heap_insert/update/delete` 失败 → ModifyTable 中止执行并返回错误（与 C0-3 DBERR_* 对齐）；`mt_processed` 只在成功时自增。

### 6. 回归验证（T6）

跑 SQL 集成测试，确认无回退。

## 不变项

- Relation/heap 公开 API 签名变更最小化（仅加 out_tid 参数，保留兼容）
- ModifyTable 路径不变
- WAL 接入（C0-2 T3）已先行，tid 是写入路径独立维度
