# DS 统一 API 与错误码日志系统重构

## 1. 背景与目标

当前项目存在以下问题需要解决：

### 1.1 API 不兼容问题

部分 ds 测试（`str_test`、`tree_test`、`queue_test`）与 ds 库 API 不兼容，已暂时排除：

| 测试文件 | 问题 |
|---------|------|
| `str_test.cpp` | 引用旧版 `include/self_made/` 头文件 |
| `tree_test.cpp` | `tree_node_t` vs `ds_bitree_node_t` 类型不兼容 |
| `queue_test.cpp` | `dequeue()` 签名不一致（返回数据 vs 不返回） |

### 1.2 错误码与日志分散

当前项目中错误码和日志处理分散在多个地方，缺乏统一规范：

```
现有分散情况：
├── include/db/errors.h     → 待废弃
├── include/db/log.h        → 待废弃
├── rag/include/rag/error.h  → 待废弃
├── rag/include/rag/logger.h → 待废弃
└── 分散在各模块内部
```

## 2. 解决方案

### 2.1 文件结构设计

```
include/
├── common/
│   ├── base_err.h    ← SYS_* 错误码 + 基础错误结构
│   └── base_log.h   ← 统一日志宏
│
├── ds/
│   ├── ds_err.h     ← DS_* 数据结构错误码
│   └── (现有队列/树/字符串等头文件)
│
├── db/
│   ├── db_err.h     ← DB_* + SQLSTATE 兼容码
│   └── (存储/索引/查询等头文件)
│
└── rag/
    └── rag_err.h    ← RAG_* 错误码（C++ 风格保留）
```

### 2.2 错误码命名规范

```
格式: <系统>_<级别>_<描述>

级别: WARN / ERROR / FATAL（INFO/DEBUG/TRACE 级别不需要错误码）
系统: SYS / DB / RAG / DS

示例:
├── SYS_ERROR_OUT_OF_MEMORY
├── SYS_WARN_DEPRECATED_API
├── SYS_FATAL_PANIC
├── DB_ERROR_TABLE_NOT_FOUND
├── RAG_ERROR_MODEL_LOAD_FAILED
└── DS_ERROR_QUEUE_FULL
```

### 2.3 SQLSTATE 兼容性

对于 DB 层错误码，同时支持 PostgreSQL 风格的 5 字符 SQLSTATE：

```c
// 两种格式共存
#define DB_ERROR_TABLE_NOT_FOUND  "DB_ERROR_TABLE_NOT_FOUND"
#define DB_ERR_TABLE_NOT_FOUND    "42P01"  // PostgreSQL 标准码
```

## 3. 变更范围

### 3.1 本次实施范围（仅错误码）

- [x] 创建 `include/common/base_err.h`
- [x] 创建 `include/common/base_log.h`
- [x] 创建 `include/ds/ds_err.h`
- [x] 创建/改造 `include/db/db_err.h`
- [x] 创建 `include/rag/rag_err.h`
- [ ] 废弃 `include/db/errors.h`
- [ ] 废弃 `include/db/log.h`
- [ ] 废弃 `rag/include/rag/error.h`
- [ ] 废弃 `rag/include/rag/logger.h`

### 3.2 后续实施范围（DS API 修复）

- [ ] Queue API：`dequeue()` 统一为 `dequeue(queue, &data)` 出参风格
- [ ] Tree API：新增 `ds_bitree_create_from_array()` 等便利函数
- [ ] String API：补充缺失函数到 `algo/ds/string.h`
- [ ] 测试迁移：更新 `str_test.cpp`、`tree_test.cpp`、`queue_test.cpp`
- [ ] 移除 CMakeLists.txt 中的排除列表

## 4. 错误码定义详情

### 4.1 公共层错误码 (SYS_*)

```c
// 错误级别枚举
typedef enum {
    SYS_ERR_LEVEL_SUCCESS = 0,
    SYS_ERR_LEVEL_INFO    = 1,
    SYS_ERR_LEVEL_WARN    = 2,
    SYS_ERR_LEVEL_ERROR   = 3,
    SYS_ERR_LEVEL_FATAL   = 4,
} sys_err_level_t;

// 错误码定义
#define SYS_ERROR_OUT_OF_MEMORY     "SYS_ERROR_OUT_OF_MEMORY"
#define SYS_ERROR_INVALID_PARAM     "SYS_ERROR_INVALID_PARAM"
#define SYS_ERROR_NULL_POINTER      "SYS_ERROR_NULL_POINTER"
#define SYS_ERROR_FILE_NOT_FOUND    "SYS_ERROR_FILE_NOT_FOUND"
#define SYS_ERROR_IO_FAILED         "SYS_ERROR_IO_FAILED"
#define SYS_FATAL_PANIC             "SYS_FATAL_PANIC"
```

### 4.2 数据结构层错误码 (DS_*)

```c
// Queue 错误码
#define DS_ERROR_QUEUE_EMPTY        "DS_ERROR_QUEUE_EMPTY"
#define DS_ERROR_QUEUE_FULL         "DS_ERROR_QUEUE_FULL"

// Stack 错误码
#define DS_ERROR_STACK_EMPTY        "DS_ERROR_STACK_EMPTY"
#define DS_ERROR_STACK_FULL         "DS_ERROR_STACK_FULL"

// 通用错误码
#define DS_ERROR_INVALID_CAPACITY   "DS_ERROR_INVALID_CAPACITY"
#define DS_ERROR_INVALID_INDEX      "DS_ERROR_INVALID_INDEX"
```

### 4.3 数据库层错误码 (DB_*)

```c
// SQLSTATE 兼容码（PostgreSQL 标准）
#define DB_ERR_NOT_NULL_VIOLATION   "23502"
#define DB_ERR_UNIQUE_VIOLATION     "23505"
#define DB_ERR_FOREIGN_KEY_VIOLATION "23503"

// DB_* 格式错误码
#define DB_ERROR_TABLE_NOT_FOUND    "DB_ERROR_TABLE_NOT_FOUND"
#define DB_ERROR_COLUMN_NOT_FOUND   "DB_ERROR_COLUMN_NOT_FOUND"
#define DB_ERROR_DEADLOCK_DETECTED  "DB_ERROR_DEADLOCK_DETECTED"

// 扩展能力错误码
#define DB_ERROR_VERTEX_NOT_FOUND   "DB_ERROR_VERTEX_NOT_FOUND"
#define DB_ERROR_VECTOR_DIM_MISMATCH "DB_ERROR_VECTOR_DIM_MISMATCH"
```

### 4.4 RAG 层错误码 (RAG_*)

```c
// 模型错误码
#define RAG_ERROR_MODEL_LOAD_FAILED     "RAG_ERROR_MODEL_LOAD_FAILED"
#define RAG_ERROR_MODEL_UNSUPPORTED     "RAG_ERROR_MODEL_UNSUPPORTED"

// 索引错误码
#define RAG_ERROR_INDEX_BUILD_FAILED    "RAG_ERROR_INDEX_BUILD_FAILED"
#define RAG_ERROR_INDEX_NOT_FOUND       "RAG_ERROR_INDEX_NOT_FOUND"

// LLM 错误码
#define RAG_ERROR_LLM_TIMEOUT           "RAG_ERROR_LLM_TIMEOUT"
#define RAG_ERROR_LLM_GENERATION_FAILED "RAG_ERROR_LLM_GENERATION_FAILED"

// 检索错误码
#define RAG_WARN_QUERY_TOO_SHORT        "RAG_WARN_QUERY_TOO_SHORT"
```

## 5. 日志宏设计

### 5.1 日志级别

```c
typedef enum {
    LOG_LEVEL_TRACE = 0,  // 跟踪（无错误码）
    LOG_LEVEL_DEBUG = 1,  // 调试（无错误码）
    LOG_LEVEL_INFO  = 2,  // 信息（无错误码）
    LOG_LEVEL_WARN  = 3,  // 警告（有错误码）
    LOG_LEVEL_ERROR = 4,  // 错误（有错误码）
    LOG_LEVEL_FATAL = 5,  // 致命（有错误码）
} log_level_t;
```

### 5.2 标准日志宏

```c
// 标准日志宏（全局）
#define LOG_TRACE(fmt, ...)  log_write(LOG_LEVEL_TRACE, ...)
#define LOG_DEBUG(fmt, ...)  log_write(LOG_LEVEL_DEBUG, ...)
#define LOG_INFO(fmt, ...)   log_write(LOG_LEVEL_INFO, ...)
#define LOG_WARN(fmt, ...)   log_write(LOG_LEVEL_WARN, ...)
#define LOG_ERROR(fmt, ...)  log_write(LOG_LEVEL_ERROR, ...)
#define LOG_FATAL(fmt, ...)  log_write(LOG_LEVEL_FATAL, ...)

// 带标签的日志宏
#define DB_LOG_DEBUG(tag, fmt, ...)   log_write(LOG_LEVEL_DEBUG, ..., "[DB:%s] " fmt, tag, ...)
#define RAG_LOG_ERROR(tag, fmt, ...)  log_write(LOG_LEVEL_ERROR, ..., "[RAG:%s] " fmt, tag, ...)
```

## 6. 待废弃文件清单

| 文件路径 | 替代方案 | 废弃方式 |
|---------|---------|---------|
| `include/db/errors.h` | `include/common/base_err.h` | 删除 |
| `include/db/log.h` | `include/common/base_log.h` | 删除 |
| `rag/include/rag/error.h` | `include/rag/rag_err.h` | 删除 |
| `rag/include/rag/logger.h` | `include/common/base_log.h` | 删除 |

> 注：废弃操作需在并发变更完成后执行，避免编译失败。
