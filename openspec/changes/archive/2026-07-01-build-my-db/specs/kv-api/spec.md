# KV API 规格

## 功能概述

提供简洁的键值接口，封装底层存储引擎细节。

## 核心概念

### 键（Key）

- **类型**：变长字节数组
- **最大长度**：8KB
- **比较**：字典序（逐字节比较）

### 值（Value）

- **类型**：变长字节数组
- **最大长度**：1MB（超出使用溢出页）
- **编码**：用户自定义

### 数据库实例

```c
// 数据库句柄（opaque）
typedef struct db_s db_t;

// 打开数据库
db_t *db_open(const char *path);

// 关闭数据库
int db_close(db_t *db);
```

## 接口设计

### 基本操作

```c
// 插入/更新
int db_put(db_t *db, const void *key, size_t key_len,
           const void *value, size_t value_len);

// 查询
int db_get(db_t *db, const void *key, size_t key_len,
           void **out_value, size_t *out_len);

// 删除
int db_delete(db_t *db, const void *key, size_t key_len);

// 是否存在
bool db_exists(db_t *db, const void *key, size_t key_len);
```

### 范围操作

```c
// 扫描迭代器
typedef struct db_iter_s db_iter_t;

// 创建迭代器（start=NULL 表示从头开始，end=NULL 表示到尾）
db_iter_t *db_scan(db_t *db, 
                   const void *start_key, size_t start_len,
                   const void *end_key, size_t end_len);

// 迭代器操作
bool db_iter_next(db_iter_t *iter);
void db_iter_key(db_iter_t *iter, void **key, size_t *len);
void db_iter_value(db_iter_t *iter, void **value, size_t *len);
void db_iter_free(db_iter_t *iter);

// 获取迭代器当前位置的 key/value（不移动）
int db_iter_peek(db_iter_t *iter, void **key, size_t *key_len,
                 void **value, size_t *value_len);
```

### 统计信息

```c
// 数据库统计
typedef struct {
    size_t num_keys;         // 键数量
    size_t total_size;       // 总大小（字节）
    size_t page_count;       // 使用页面数
    size_t cache_hit_rate;   // 缓存命中率
} db_stats_t;

int db_stats(db_t *db, db_stats_t *stats);
```

## 内部实现

```
db_put(key, value)
    │
    ▼
┌─────────────────────────────────────┐
│ 1. 查找 key 是否存在                 │
│    - 通过 ART 索引查找               │
│ 2. 如果存在，更新值                  │
│    - 分配新页面存储值                │
│    - 更新 ART 索引                   │
│ 3. 如果不存在，插入新记录            │
│    - 分配数据页存储 key+value        │
│    - 插入到 ART 索引                 │
└─────────────────────────────────────┘
```

## 错误处理

```c
// 错误码
#define DB_OK           0
#define DB_NOT_FOUND    1
#define DB_ERROR        2
#define DB_CORRUPT      3
#define DB_NOMEM        4

// 获取错误信息
const char *db_errmsg(db_t *db);
```

## 验收标准

- [ ] db_put 能插入新键值对
- [ ] db_put 能更新已存在的键
- [ ] db_get 能正确获取值
- [ ] db_delete 能删除键值对
- [ ] db_exists 正确判断键是否存在
- [ ] db_scan 能遍历范围内的键
- [ ] 迭代器支持顺序和逆序
- [ ] 错误处理正确
- [ ] 并发访问安全（可选）