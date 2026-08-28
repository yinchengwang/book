# C0-3 统一错误码与资源管理 设计文档

## 设计目标

消除三套并存的错误码（KV enum / Vector -1 / SQL 自行）、执行器手工 free 链易漏、`mm_storage` 假成功（drop 空转、scan 恒 NULL）、mm_insert 手工字节偏移无版本。

## 方案

### 1. 统一错误码空间 `DBERR_*`

`include/db/errors.h` 新增 `dberr_t` enum：

```c
#define DBERR_OK                  0
#define DBERR_INVALID            -1   /* 参数非法 */
#define DBERR_IO                 -2   /* IO 错误 */
#define DBERR_NOMEM              -3
#define DBERR_FULL               -4   /* page/full */
#define DBERR_CONFLICT           -5   /* CAS 失败 */
#define DBERR_NOT_IMPLEMENTED    -6   /* 未实装接口 */
#define DBERR_WAL_FAILED         -7   /* WAL 写失败 */
#define DBERR_NOT_FOUND          -8
#define DBERR_EXISTS             -9
#define DBERR_CORRUPT            -10
/* 模态前缀 100+：VECTOR/KV/REL/GRAPH/TS/DOC/SPATIAL/TREE/RDF/SPARSE */
#define DBERR_MOD_BASE           100
#define DBERR_MOD_VECTOR         (DBERR_MOD_BASE + 3)
#define DBERR_MOD_KV             (DBERR_MOD_BASE + 1)
/* ... */
```

提供 `dberr_str(int code)` 返回可读字符串。

### 2. 向后兼容映射

| 旧码 | 新码 |
|------|------|
| KV_OK (0) | DBERR_OK (0) |
| KV_INVALID (6) | DBERR_INVALID (-1) |
| KV_FULL（实际误用为 KV_ERROR） | DBERR_FULL (-4) |
| Vector 返回 -1 | DBERR_INVALID 或 DBERR_NOT_IMPLEMENTED |

各模态 API 函数保持旧 enum 返回值（不破坏 ABI），内部判定时通过 `kv_to_dberr()` 等适配宏转换。

### 3. EState MemoryContext 化

- `ExecutorStart` 创建 per-query `MemoryContext`
- 各算子初始化挂此 context
- `ExecutorEnd` 一次 `MemoryContextReset()` 替代手工 free

### 4. vector drop/scan 实装

- `vector_engine_table_drop`：实装为"删除 meta + 删除数据文件 + WAL 记录"或返回 `DBERR_NOT_IMPLEMENTED`
- `vector_engine_scan_begin`：返回明确错误而非 NULL

### 5. mm_record_header_t 序列化契约

```c
typedef struct mm_record_header_s {
    uint32_t magic;      /* 'MMDB' = 0x4D4D4442 */
    uint32_t version;    /* 当前 1 */
    uint32_t model;      /* DataModel 枚举值 */
    uint32_t payload_len;/* 紧随其后的负载长度 */
} mm_record_header_t;
```

新写入路径：mm_insert 加头部；旧数据 magic 不匹配时按历史 schema 解析。

### 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 旧 enum 破坏 ABI | 内部转换宏，新代码用 DBERR_*，旧代码继续工作 |
| MemoryContext 接入扩散面 | nodeSeqscan 首批示范，其余算子后续跟进 |
| 向后解析旧 schema | magic 不匹配时回退旧解析路径（待 TDD 验证） |
