# TupleTableSlot TID 管道规范（新增）

## 目的

修复 `nodeModifyTable.c` UPDATE/DELETE 时硬编码 TID `(block=0, offset=24)` 导致的"改错行"炸弹。

## 要求

### REQ-1：TupleTableSlot 携带 TID

`TupleTableSlot` 必须包含 `tts_tid`（ItemPointerData：block 4 字节 + offset 2 字节）。扫描路径填写真实物理位置。

### REQ-2：堆层回填 TID

`heap_insert` 成功时回填 tid 给调用方（out 参数或返回值结构体），便于 DML 算子记录。

### REQ-3：DML 算子使用真实 TID

`nodeModifyTable` UPDATE/DELETE 路径**禁止**使用任何硬编码 TID；必须从 `slot->tts_tid` 读取。

### REQ-4：错误传播

DML 失败 → 中止执行 + 返回 DBERR；`mt_processed` 只在成功时自增。
