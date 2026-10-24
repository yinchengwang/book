# 统一错误码与资源管理规范（新增）

## 目的

消除三套错误码并存（KV/Vector/SQL 各有体系）、执行器手工 free 易漏、`mm_storage` 假成功（drop/scan）、mm_insert 无版本等缺陷。

## 要求

### REQ-1：统一错误码空间

`include/db/errors.h` 必须定义 `DBERR_*` 错误码空间：

- 通用类：OK / INVALID / IO / NOMEM / FULL / CONFLICT / NOT_IMPLEMENTED / WAL_FAILED / NOT_FOUND / EXISTS / CORRUPT
- 模态前缀（≥100）：VECTOR / KV / REL / GRAPH / TS / DOC / SPATIAL / TREE / RDF / SPARSE

提供 `dberr_str(int code)` 返回可读字符串。

### REQ-2：向后兼容

既有模态错误码（KV `kv_result_t` 等）保留 enum 定义，内部判定通过适配宏映射到 `DBERR_*`；新代码优先使用 `DBERR_*`，旧代码继续工作（零 ABI 破坏）。

### REQ-3：per-query MemoryContext

执行器 `ExecutorStart` 创建 per-query `MemoryContext`，所有算子初始化（`ExecInit*`）资源挂此 context；`ExecutorEnd` 一次 `MemoryContextReset()` 替代手工 free 链。

### REQ-4：mm_storage 契约完整

`mm_storage` 全接口必须实装或返回 `DBERR_NOT_IMPLEMENTED`，**禁止**假成功（drop 空操作、scan 恒 NULL）。

### REQ-5：序列化版本

`mm_record_header_t`（magic + version + model + payload_len）作为所有模态 mm_insert 的统一头部；旧数据 magic 不匹配时按历史 schema 兼容读取。

## 实现文件

- `engineering/include/db/errors.h`（DBERR_* 新增）
- `engineering/src/db/core/errors.c`（dberr_str）
- `engineering/src/db/storage/access/heap/heapam.c`（K2 模态前缀宏）
- `engineering/include/db/mm_record.h`（mm_record_header_t）
- `engineering/src/db/storage/vector/vector_engine.c`（drop/scan 实装）
- `engineering/src/db/sql/executor.c`（EState MemoryContext 化）
- `engineering/src/db/sql/nodeSeqscan.c`（手工 free 链删除示范）
