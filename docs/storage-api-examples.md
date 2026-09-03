# 数据库存储引擎 API 使用示例

本文档提供 PostgreSQL 风格存储引擎的完整使用示例。

## 1. 基础使用

### 1.1 数据库打开/关闭

```c
#include "db/kv.h"

// 打开数据库（不存在则创建）
kv_t *db = kv_open("mydb.db");
if (!db) {
    // 处理错误
    return -1;
}

// 使用数据库...

// 关闭数据库
kv_close(db);
```

### 1.2 基本键值操作

```c
// 插入键值对
const char *key = "user:1";
const char *value = "{\"name\":\"Alice\",\"age\":30}";

kv_result_t result = kv_put(db, key, strlen(key), value, strlen(value));
if (result != KV_OK) {
    // 处理错误
}

// 读取键值对
void *out_value = NULL;
size_t out_len = 0;
result = kv_get(db, key, strlen(key), &out_value, &out_len);
if (result == KV_OK && out_value) {
    printf("Value: %.*s\n", (int)out_len, (char*)out_value);
    free(out_value);  // 记得释放内存
}

// 删除键值对
kv_delete(db, key, strlen(key));
```

### 1.3 键值扫描

```c
// 扫描以 "user:" 开头的所有键
kv_iter_t *iter = kv_scan(db, "user:", 5, NULL, 0);
while (kv_iter_next(iter) == KV_OK) {
    const void *key = kv_iter_key(iter);
    size_t key_len = kv_iter_key_len(iter);
    const void *value = kv_iter_value(iter);
    size_t value_len = kv_iter_value_len(iter);
    
    printf("Key: %.*s, Value: %.*s\n",
           (int)key_len, (char*)key,
           (int)value_len, (char*)value);
}
kv_iter_free(iter);
```

## 2. 存储统计

```c
#include "db/kv_engine.h"

// 获取存储统计信息
kv_stats_t stats;
kv_result_t result = kv_stats(db, &stats);
if (result == KV_OK) {
    printf("Keys: %zu\n", stats.num_keys);
    printf("Size: %zu bytes\n", stats.total_size);
}
```

## 3. 多模态存储

### 3.1 初始化多模态存储管理器

```c
#include "db/mm_storage.h"
#include "db/kv_engine.h"
#include "db/vector_engine.h"

// 初始化（注册引擎）
mm_storage_init("./data");

// 注册引擎
storage_register_engine(MODEL_KV, kv_engine_get_ops());
storage_register_engine(MODEL_VECTOR, vector_engine_get_ops());

// 获取上下文
mm_context_t *ctx = mm_get_context();
```

### 3.2 KV 引擎操作

```c
#include "db/kv_engine.h"

// 创建 KV 数据库
kv_engine_create("test_db", NULL);

// 打开数据库
void *db = kv_engine_open("test_db", ACCESS_MODE_READ_WRITE);
kv_engine_db_t *kv_db = (kv_engine_db_t *)db;

// 使用 KV API
kv_put(kv_db->kv, "key1", 5, "value1", 6);

void *out_value = NULL;
size_t out_len = 0;
kv_get(kv_db->kv, "key1", 5, &out_value, &out_len);

// 关闭数据库
kv_engine_close(db);

// 删除数据库
kv_engine_drop("test_db");
```

### 3.3 向量引擎操作

```c
#include "db/vector_engine.h"

// 初始化向量引擎
vector_engine_init("./data/vector");

// 创建向量集合
vector_engine_create("embeddings", NULL);

// 打开集合
void *vec_db = vector_engine_open("embeddings", ACCESS_MODE_READ_WRITE);

// 构造向量数据: id(8) + dimension(4) + float[dimension]
uint8_t data[1024];
uint64_t id = 1;
int32_t dim = 128;

uint8_t *ptr = data;
memcpy(ptr, &id, sizeof(uint64_t));
ptr += sizeof(uint64_t);
memcpy(ptr, &dim, sizeof(int32_t));
ptr += sizeof(int32_t);
for (int i = 0; i < dim; i++) {
    float v = (float)i / dim;
    memcpy(ptr, &v, sizeof(float));
    ptr += sizeof(float);
}

// 插入向量
vector_engine_insert(vec_db, data, sizeof(data));

// KNN 搜索
vector_search_params_t params = {
    .top_k = 10,
    .metric = METRIC_L2,
    .query_vector = query_data,
    .dimension = 128
};
vector_search_result_t *results = NULL;
size_t result_count = 0;
vector_engine_search(vec_db, &params, &results, &result_count);

// 释放结果
vector_engine_free_results(results, result_count);

// 关闭集合
vector_engine_close(vec_db);
```

### 3.4 时序引擎操作

```c
#include "db/ts_engine.h"

// 初始化时序引擎
ts_engine_init("./data/timeseries");

// 创建时序指标
ts_engine_create("cpu_metrics", NULL);

// 打开指标
void *ts_db = ts_engine_open("cpu_metrics", ACCESS_MODE_READ_WRITE);

// 构造数据点: timestamp(8) + value(8)
uint8_t data[32];
int64_t timestamp = 1700000000000LL;
double value = 42.5;

memcpy(data, &timestamp, sizeof(int64_t));
memcpy(data + sizeof(int64_t), &value, sizeof(double));

// 插入数据点
ts_engine_insert(ts_db, data, sizeof(data));

// 聚合查询
ts_query_params_t params = {
    .start_time = 1700000000000LL,
    .end_time = 1700003600000LL,
    .aggregation = TS_AGG_AVG,  // AVG, SUM, MIN, MAX, COUNT
    .granularity = GRANULARITY_MINUTE
};

ts_query_result_t *query_result = NULL;
ts_engine_query(ts_db, &params, &query_result);

// 处理结果...

// 关闭
ts_engine_close(ts_db);
```

### 3.5 空间引擎操作

```c
#include "db/spatial_engine.h"

// 初始化空间引擎
spatial_engine_init("./data/spatial");

// 创建空间数据集
spatial_engine_create("locations", NULL);

// 打开数据集
void *spatial_db = spatial_engine_open("locations", ACCESS_MODE_READ_WRITE);

// 构造几何数据: id_len(4) + id + geom_type(4) + wkt_len(4) + wkt
const char *geo_id = "point_001";
const char *wkt = "POINT(1.0 2.0)";
uint8_t data[256];

uint32_t id_len = strlen(geo_id);
uint32_t geom_type = GEOM_POINT;
uint32_t wkt_len = strlen(wkt);

uint8_t *ptr = data;
memcpy(ptr, &id_len, sizeof(uint32_t));
ptr += sizeof(uint32_t);
memcpy(ptr, geo_id, id_len);
ptr += id_len;
memcpy(ptr, &geom_type, sizeof(uint32_t));
ptr += sizeof(uint32_t);
memcpy(ptr, &wkt_len, sizeof(uint32_t));
ptr += sizeof(uint32_t);
memcpy(ptr, wkt, wkt_len);

// 插入几何对象
spatial_engine_insert(spatial_db, data, sizeof(data));

// 范围查询
bbox_t bbox = bbox_create(-10.0, -10.0, 10.0, 10.0);
spatial_query_params_t params = {
    .bbox = &bbox,
    .geom_type = GEOM_POINT
};

spatial_query_result_t *results = NULL;
size_t result_count = 0;
spatial_engine_query(spatial_db, &params, &results, &result_count);

// 关闭
spatial_engine_close(spatial_db);
```

## 4. 错误处理

```c
#include "db/errors.h"

// 初始化错误系统
db_error_init();

// 检查错误
if (db_has_error()) {
    db_error_code_t code = db_get_error_code();
    const char *msg = db_get_error_message();
    printf("Error %d: %s\n", code, msg);
}

// 清除错误
db_clear_error();

// 关闭错误系统
db_error_shutdown();
```

## 5. 日志系统

```c
#include "db/log.h"

// 初始化日志
log_config_t config = {
    .level = LOG_LEVEL_DEBUG,  // DEBUG, INFO, WARN, ERROR
    .target = LOG_TARGET_CONSOLE,  // CONSOLE, FILE
    .enable_colors = true,
    .file_path = "app.log"
};
log_init(&config);

// 使用日志
LOG_DEBUG("Debug message: %s", "details");
LOG_INFO("Info message: %d", 42);
LOG_WARN("Warning message");
LOG_ERROR("Error: %s", "something went wrong");

// 关闭日志
log_shutdown();
```

## 6. 清理资源

```c
// 按初始化顺序反向关闭
mm_storage_shutdown();  // 如果使用了多模态存储
kv_close(db);          // 关闭数据库
log_shutdown();        // 最后关闭日志
db_error_shutdown();
```
