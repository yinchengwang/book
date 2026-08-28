# P3-4 Column Family 设计

> 日期：2026-08-28
> 状态：与提案一致，实现完成
> 目标：实现类 Cassandra/HBase 的列族存储引擎

## 一、架构

```
cf_db_t (顶层 DB 句柄)
  ├─ kv_t (底层 KV 实例)
  ├─ 列族元数据：family 列表 + schema
  └─ 锁：mmdb_rwlock_t (跨平台)
        ↓
cf_family (列族)
  ├─ name (字符串)
  ├─ 列定义
  └─ Row 数据（存于 KV，使用前缀编码）
```

## 二、核心数据结构

### cf_db_t
```c
typedef struct cf_db_s cf_db_t;
```
列族数据库句柄，包含 KV 实例、列族列表、读写锁。

### cf_row_t
```c
typedef struct cf_row_s cf_row_t;
```
行结构：行键 + 多列值（动态列）。

### cf_column_t
```c
typedef struct cf_column_s cf_column_t;
```
列定义：列名 + 列值（二进制）+ 列类型（Static/Dynamic）。

## 三、API 表面

| 函数 | 作用 |
|------|------|
| `cf_open(data_dir)` | 打开/创建列族数据库 |
| `cf_close(db)` | 关闭 |
| `cf_flush(db)` | 刷盘 |
| `cf_create_family(db, name)` | 创建列族 |
| `cf_drop_family(db, name)` | 删除列族 |
| `cf_list_families(db)` | 列出所有列族 |
| `cf_put(db, cf, key, col, val)` | 写入单列 |
| `cf_get(db, cf, key, col)` | 读取单列 |
| `cf_delete_column(db, cf, key, col)` | 删除单列 |
| `cf_get_row(db, cf, key)` | 读取整行 |
| `cf_delete_row(db, cf, key)` | 删除整行 |
| `cf_iter_*` | 迭代器 API |
| `cf_batch_execute(db, ops[], n)` | 批量操作 |
| `cf_family_stats(db, cf)` | 列族统计 |

## 四、KV 编码

行键编码：`<cf_name>:<row_key>`
列名编码：`<cf_name>:<row_key>:<column_name>`
所有数据存于单个 KV 实例，使用前缀扫描实现列族隔离。

## 五、文件清单

创建：
- `engineering/include/db/cf/cf_engine.h` (407 行)
- `engineering/include/db/cf/cf_row.h` (128 行)
- `engineering/include/db/cf/cf_column.h` (123 行)
- `engineering/src/db/cf/cf_engine.c` (987 行)
- `engineering/src/db/cf/cf_row.c` (277 行)
- `engineering/src/db/cf/cf_column.c` (211 行)
- `engineering/test/db/cf/cf_engine_test.cpp` (667 行，22 个测试)

构建集成：
- `engineering/src/db/cf/CMakeLists.txt`
- `engineering/test/db/cf/CMakeLists.txt`
- `engineering/src/db/CMakeLists.txt` add_subdirectory(cf)
- `engineering/test/db/CMakeLists.txt` add_subdirectory(cf)