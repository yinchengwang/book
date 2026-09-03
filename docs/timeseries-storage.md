# 时序存储使用指南

## 概述

时序存储引擎（Timeseries Engine）专为时序数据的存储和聚合查询设计，支持分段索引、Gorilla 压缩和多种聚合函数。

## 核心特性

| 特性 | 说明 |
|------|------|
| 分段索引 | 时间范围分段存储，二分查找定位 |
| Gorilla 压缩 | 5-10x 压缩率的无损压缩 |
| 聚合查询 | SUM、AVG、MIN、MAX、COUNT |
| 时间粒度 | 秒、分钟、小时、天级聚合 |
| TTL 支持 | 自动过期数据清理 |

## 快速开始

### 1. 初始化引擎

```c
#include "db/storage/ts/ts_engine.h"

// 初始化时序引擎
ts_engine_init("/data/timeseries");

// 创建时序表
ts_engine_create("metrics", "cpu,mem,disk");

// 打开时序表
void *ts_db = ts_engine_open("metrics", ACCESS_MODE_READ_WRITE);
```

### 2. 插入数据点

```c
// 插入单个数据点
ts_record_t record;
record.timestamp = (int64_t)time(NULL) * 1000;  // 毫秒时间戳
record.value = 75.5;
record.tags.count = 1;
record.tags.tags[0].key = "host";
record.tags.tags[0].value = "server1";

ts_engine_insert(ts_db, &record, sizeof(record));

// 批量插入
ts_record_t records[100];
int64_t base_time = (int64_t)time(NULL) * 1000;
for (int i = 0; i < 100; i++) {
    records[i].timestamp = base_time + i * 60000;  // 每分钟一个点
    records[i].value = 50.0 + (double)(rand() % 100) / 10.0;
}

ts_engine_batch_insert(ts_db, records, 100);
```

### 3. 基本查询

```c
// 时间范围查询
int64_t start_time = base_time;
int64_t end_time = base_time + 3600000;  // 1 小时

ts_query_results_t results;
ts_engine_query(ts_db, start_time, end_time, GRANULARITY_MINUTE, 
                AGG_AVG, &results);

printf("查询到 %u 个数据点:\n", results.count);
for (uint32_t i = 0; i < results.count && i < 10; i++) {
    printf("  [%lu] %.2f\n", results.points[i].timestamp, results.points[i].value);
}
```

## 分段索引

分段索引将时序数据按时间范围分段存储，提高查询效率。

### 启用分段索引

```c
// 分段索引默认启用，可配置段大小和粒度
// 段大小：每个段的采样点数
// 粒度：段的聚合粒度
ts_engine_enable_segment_index(ts_db, 4096, GRANULARITY_HOUR);
```

### 获取分段统计

```c
// 获取分段统计信息
uint32_t seg_count;
uint64_t total_points;
ts_engine_segment_stats(ts_db, &seg_count, &total_points);

printf("分段统计:\n");
printf("  段数: %u\n", seg_count);
printf("  总点数: %lu\n", total_points);
```

### 分段查询

```c
// 使用分段索引进行高效查询
ts_segment_query_t query;
query.start_time = start_time;
query.end_time = end_time;
query.granularity = GRANULARITY_MINUTE;

ts_segment_result_t seg_results[100];
uint32_t result_count = ts_engine_segment_query(ts_db, &query, seg_results, 100);

printf("分段查询返回 %u 个结果:\n", result_count);
for (uint32_t i = 0; i < result_count; i++) {
    printf("  段 [%lu - %lu]: count=%u, min=%.2f, max=%.2f, avg=%.2f\n",
           seg_results[i].start_time,
           seg_results[i].end_time,
           seg_results[i].count,
           seg_results[i].min,
           seg_results[i].max,
           seg_results[i].avg);
}
```

## Gorilla 压缩

Gorilla 压缩是 Facebook 开源的高效时序压缩算法，压缩率可达 5-10x。

### 启用压缩

```c
// 启用 Gorilla 压缩
ts_engine_enable_compression(ts_db);

// 检查压缩状态
ts_engine_db_t *db = (ts_engine_db_t *)ts_db;
if (db->use_compression) {
    printf("Gorilla 压缩已启用\n");
}
```

### 获取压缩统计

```c
// 获取压缩统计
ts_compression_stats_t stats;
ts_engine_compression_stats(ts_db, &stats);

printf("压缩统计:\n");
printf("  原始大小: %lu 字节\n", stats.original_size);
printf("  压缩大小: %lu 字节\n", stats.compressed_size);
printf("  压缩率: %.2fx\n", (double)stats.original_size / stats.compressed_size);
```

## 聚合查询

支持多种聚合函数和时间粒度。

### 聚合函数

| 函数 | 说明 |
|------|------|
| AGG_SUM | 求和 |
| AGG_AVG | 平均值 |
| AGG_MIN | 最小值 |
| AGG_MAX | 最大值 |
| AGG_COUNT | 计数 |
| AGG_STDDEV | 标准差 |

### 时间粒度

| 粒度 | 说明 |
|------|------|
| GRANULARITY_SECOND | 秒级 |
| GRANULARITY_MINUTE | 分钟级 |
| GRANULARITY_HOUR | 小时级 |
| GRANULARITY_DAY | 天级 |

### 聚合查询示例

```c
int64_t start = base_time;
int64_t end = base_time + 86400000;  // 1 天

// 按小时聚合的 AVG
ts_query_results_t hourly_avg;
ts_engine_query(ts_db, start, end, GRANULARITY_HOUR, AGG_AVG, &hourly_avg);

printf("小时级 AVG 聚合:\n");
for (uint32_t i = 0; i < hourly_avg.count; i++) {
    printf("  [%lu] avg=%.2f\n", 
           hourly_avg.points[i].timestamp,
           hourly_avg.points[i].value);
}

// 按分钟聚合的 MIN/MAX
ts_query_results_t min_results, max_results;
ts_engine_query(ts_db, start, end, GRANULARITY_MINUTE, AGG_MIN, &min_results);
ts_engine_query(ts_db, start, end, GRANULARITY_MINUTE, AGG_MAX, &max_results);

// 按天聚合的 COUNT
ts_query_results_t daily_count;
ts_engine_query(ts_db, start, end, GRANULARITY_DAY, AGG_COUNT, &daily_count);
```

## TTL 和数据过期

```c
// 设置数据保留时间（毫秒）
int64_t retention_ms = 30 * 24 * 3600 * 1000LL;  // 30 天
ts_engine_set_retention(ts_db, retention_ms);

// 执行过期数据清理
uint64_t deleted_count = ts_engine_expire(ts_db, 0);  // 清理所有过期数据
printf("删除了 %lu 个过期数据点\n", deleted_count);

// 清理特定时间之前的数据
uint64_t deleted_before = ts_engine_expire(ts_db, base_time - 7 * 86400000LL);
printf("删除了 %lu 个 7 天前的数据点\n", deleted_before);
```

## 物化视图

物化视图预先计算聚合结果，加速频繁查询。

### 创建物化视图

```c
// 创建小时级 AVG 物化视图
ts_mview_t *mview = ts_mview_create(ts_db, "hourly_avg", 
                                      GRANULARITY_HOUR, AGG_AVG);
if (mview) {
    printf("物化视图创建成功\n");
}
```

### 刷新物化视图

```c
// 手动刷新
ts_mview_refresh(mview);

// 查询物化数据（更快）
ts_mview_result_t *mview_result;
uint32_t result_count = ts_mview_query(mview, start, end, &mview_result);

printf("物化视图查询结果:\n");
for (uint32_t i = 0; i < result_count; i++) {
    printf("  [%lu] %.2f\n", 
           mview_result[i].timestamp,
           mview_result[i].value);
}

// 释放结果
ts_mview_free_result(mview_result, result_count);
```

## 完整示例

```c
#include "db/storage/ts/ts_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    // 1. 初始化
    ts_engine_init("/tmp/timeseries");
    ts_engine_create("cpu_metrics", "host,region");
    
    void *ts_db = ts_engine_open("cpu_metrics", ACCESS_MODE_READ_WRITE);
    
    // 2. 启用优化特性
    ts_engine_enable_segment_index(ts_db, 4096, GRANULARITY_HOUR);
    ts_engine_enable_compression(ts_db);
    
    // 3. 插入模拟数据
    printf("插入时序数据...\n");
    
    int64_t base_time = (int64_t)time(NULL) * 1000;
    for (int day = 0; day < 7; day++) {
        for (int hour = 0; hour < 24; hour++) {
            for (int minute = 0; minute < 60; minute++) {
                ts_record_t record;
                record.timestamp = base_time + 
                    (day * 24 * 3600 + hour * 3600 + minute * 60) * 1000LL;
                // 模拟 CPU 使用率（带有周期性变化）
                double hour_factor = sin(hour * 3.14159 / 12) * 0.2;
                double minute_factor = sin(minute * 3.14159 / 30) * 0.1;
                record.value = 50.0 + hour_factor * 50 + minute_factor * 50 + 
                               (double)(rand() % 20);
                record.value = (record.value < 0) ? 0 : 
                               (record.value > 100) ? 100 : record.value;
                
                ts_engine_insert(ts_db, &record, sizeof(record));
            }
        }
    }
    printf("  完成 7 天数据 (每分钟一个点)\n");
    
    // 4. 分段统计
    printf("\n分段统计:\n");
    uint32_t seg_count;
    uint64_t total_points;
    ts_engine_segment_stats(ts_db, &seg_count, &total_points);
    printf("  段数: %u, 总点数: %lu\n", seg_count, total_points);
    
    // 5. 压缩统计
    printf("\n压缩统计:\n");
    ts_compression_stats_t comp_stats;
    ts_engine_compression_stats(ts_db, &comp_stats);
    printf("  原始: %lu 字节, 压缩: %lu 字节\n", 
           comp_stats.original_size, comp_stats.compressed_size);
    printf("  压缩率: %.2fx\n", 
           (double)comp_stats.original_size / comp_stats.compressed_size);
    
    // 6. 聚合查询
    printf("\n聚合查询 (最近 24 小时):\n");
    int64_t query_start = base_time + 6 * 24 * 3600 * 1000LL;  // 6 天前
    int64_t query_end = base_time + 7 * 24 * 3600 * 1000LL;    // 7 天前
    
    ts_query_results_t hourly_avg;
    ts_engine_query(ts_db, query_start, query_end, 
                    GRANULARITY_HOUR, AGG_AVG, &hourly_avg);
    printf("  小时级 AVG: %u 个数据点\n", hourly_avg.count);
    
    ts_query_results_t daily_max;
    ts_engine_query(ts_db, query_start, query_end, 
                    GRANULARITY_DAY, AGG_MAX, &daily_max);
    printf("  天级 MAX: %u 个数据点\n", daily_max.count);
    
    // 7. TTL 测试
    printf("\nTTL 测试:\n");
    uint64_t deleted = ts_engine_expire(ts_db, base_time - 8 * 24 * 3600 * 1000LL);
    printf("  删除 8 天前数据: %lu 个点\n", deleted);
    
    // 8. 清理
    ts_engine_close(ts_db);
    ts_engine_shutdown();
    
    return 0;
}
```

## 性能调优

### 分段大小选择

| 数据量 | 推荐段大小 | 推荐粒度 |
|--------|-----------|----------|
| < 1M 点 | 1024 | 分钟 |
| 1M - 100M | 4096 | 小时 |
| > 100M | 16384 | 天 |

### 压缩效果

| 数据模式 | 典型压缩率 |
|----------|-----------|
| 稳定数据 | 10-20x |
| 缓慢变化 | 5-10x |
| 剧烈变化 | 2-5x |

## 常见问题

**Q: 分段索引如何提升查询性能？**
A: 通过二分查找定位段，减少需要扫描的数据量。

**Q: Gorilla 压缩是否有精度损失？**
A: Gorilla 是无损压缩，不会损失精度。

**Q: 如何选择聚合粒度？**
A: 根据查询需求选择：实时监控用分钟，日报用小时，月报用天。
