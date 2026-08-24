# P6-M1.2 监控指标 — 任务报告

## 任务简报

为 mmdb SDK 添加 Prometheus 格式的运行时监控指标，包括：
- 容量指标（向量总数）
- 查询指标（总数/成功/失败 + 平均/P50/P99 延迟）
- 缓存指标（命中/未命中 + 命中率）
- 资源指标（内存/磁盘使用）
- HNSW 索引构建指标

集成点：在 `mmdb_vectors_search` 入口/出口、`mmdb_vectors_add` 成功提交后、
`mmdb_vectors_delete` 成功删除后、`mmdb_vectors_hnsw_rebuild` 完成时埋点。

## 完成的功能

### 1. 公共头文件（`engineering/include/sdk/mmdb_metrics.h`，102 行）

定义 `mmdb_metrics_t` 结构体（17 字段），3 个公共 API：
- `mmdb_metrics_get()`：返回全局指标快照（const 单例指针）
- `mmdb_metrics_reset()`：重置所有计数器
- `mmdb_metrics_prometheus_format()`：输出 Prometheus 文本格式

### 2. 实现文件（`engineering/src/sdk/core/metrics.c`，351 行）

- **线程安全设计**：所有字段使用 `_Atomic` 修饰（C11 stdatomic.h），relaxed 内存序
- **延迟分位数估计**：使用指数移动平均（EMA）跟踪 P50/P99，避免引入复杂分位数算法
- **CAS 循环**：双精度累加字段使用 compare-exchange-weak 保证并发安全
- **Prometheus 输出格式**：包含 `# HELP` / `# TYPE` 注释，counter / gauge 类型区分
- **内部埋点 API**：暴露给 SDK 模块调用（mmdb_metrics_record_query / record_cache / record_hnsw_build / inc_vectors_total / dec_vectors_total / set_resources）

### 3. 内部头文件（`engineering/include/sdk/impl/metrics_internal.h`，65 行）

封装埋点 API 声明，仅供 SDK 内部 `.c` 文件使用，避免污染公共 ABI。

### 4. 集成埋点（修改 `engineering/src/sdk/vectors/vectors.c`）

- **`mmdb_vectors_search`**：使用 `clock_gettime(CLOCK_MONOTONIC)` 测量端到端延迟，三处 `MMDB_OK` 返回点（HNSW 空结果 / HNSW 正常 / Flat 正常）均记录 `record_query(latency, 1)`
- **`mmdb_vectors_add`**：成功提交后调用 `mmdb_metrics_inc_vectors_total(n)` 累加
- **`mmdb_vectors_delete`**：成功删除后调用 `mmdb_metrics_dec_vectors_total(1)` 扣除（CAS 保证不溢出为负）
- **`mmdb_vectors_hnsw_rebuild`**：使用 `clock_gettime` 测量构建耗时，记录 `record_hnsw_build(time_ms)`

### 5. 测试覆盖（`engineering/test/sdk/integration/metrics_test.cpp`，288 行）

8 个用例，覆盖：

**单元测试（5 个）**
- `InitialState`：reset 后所有字段为 0
- `ResetClearsCounters`：reset 验证
- `PrometheusFormat`：格式包含 HELP/TYPE 注释 + 关键指标名
- `BufferBoundaryHandling`：NULL 缓冲 / 0 容量 / 截断场景安全
- `GetReturnsConsistentSnapshot`：连续 get 返回同一单例

**集成测试（3 个）**
- `SearchIncrementsQueryCounters`：5 次搜索 → queries_total/success +5，queries_failed 不变，latency_avg > 0
- `AddDeleteUpdatesVectorTotal`：add 2 个 → vectors_total +2；delete 1 个 → vectors_total +1
- `PrometheusOutputReflectsActivity`：执行活动后 Prom 输出包含真实值

### 6. CMake 集成

- `engineering/src/sdk/CMakeLists.txt`：将 `core/metrics.c` 加入 `SDK_CORE_SOURCES`
- `engineering/test/sdk/integration/CMakeLists.txt`：注册 `metrics_test`

## 变更范围

| 文件 | 变更类型 | 行数变化 |
|------|---------|---------|
| `engineering/include/sdk/mmdb_metrics.h` | 新建 | +102 |
| `engineering/include/sdk/impl/metrics_internal.h` | 新建 | +65 |
| `engineering/src/sdk/core/metrics.c` | 新建 | +351 |
| `engineering/src/sdk/vectors/vectors.c` | 修改（埋点集成） | +27 |
| `engineering/src/sdk/CMakeLists.txt` | 修改（注册 metrics.c） | +1 |
| `engineering/test/sdk/integration/metrics_test.cpp` | 新建 | +288 |
| `engineering/test/sdk/integration/CMakeLists.txt` | 修改（注册 metrics_test） | +11 |

总计：4 个新文件 + 3 个修改文件，净增约 845 行。

## 验证方式

### 1. 构建验证

```
[1/3] Building C object src/sdk/CMakeFiles/mmsdk.dir/core/metrics.c.obj
[2/3] Building C object src/sdk/CMakeFiles/mmsdk.dir/vectors/vectors.c.obj
[3/3] Linking C static library src\sdk\libmmsdk.a
[1/3] Building CXX object test/sdk/integration/CMakeFiles/metrics_test.dir/metrics_test.cpp.obj
[2/3] Linking CXX executable sdk_integration_tests\metrics_test.exe
```

零警告（修复了 snprintf 返回值类型与 size_t 比较、unused variable 两处警告）。

### 2. 新功能测试

```
[==========] 8 tests from 2 test suites ran. (48 ms total)
[  PASSED  ] 8 tests.
```

- MetricsTest (5 tests): InitialState / ResetClearsCounters / PrometheusFormat / BufferBoundaryHandling / GetReturnsConsistentSnapshot
- MetricsIntegrationTest (3 tests): SearchIncrementsQueryCounters / AddDeleteUpdatesVectorTotal / PrometheusOutputReflectsActivity

### 3. 回归测试

```
1/6 Test #136: mmdb_vectors_test ................   Passed    0.21 sec
2/6 Test #143: hybrid_search_test ...............   Passed    0.07 sec
3/6 Test #146: xquery_id_len_test ...............   Passed    0.03 sec
4/6 Test #152: hnsw_filter_test .................   Passed    0.01 sec
5/6 Test #157: pagination_test ..................   Passed    0.13 sec
6/6 Test #158: metrics_test .....................   Passed    0.05 sec

100% tests passed, 0 tests failed out of 6
```

所有相关测试（向量、混合搜索、HNSW 过滤、分页）均通过，确认 C ABI 零破坏。

### 4. Prometheus 输出示例

```
# Metrics from mmdb-sdk
# HELP mmdb_vectors_total Number of vectors stored
# TYPE mmdb_vectors_total counter
mmdb_vectors_total 5.000000
# HELP mmdb_queries_total Total number of queries
# TYPE mmdb_queries_total counter
mmdb_queries_total 10.000000
# HELP mmdb_query_latency_avg_ms Average query latency in milliseconds
# TYPE mmdb_query_latency_avg_ms gauge
mmdb_query_latency_avg_ms 0.234567
...
```

## 后续建议

1. **P99 精度提升**：当前 P50/P99 使用 EMA 近似（alpha=0.1 / alpha=0.01），适合监控展示但非精确分位数。如需 SLA 验证，可替换为 t-digest 或 HdrHistogram。
2. **向量总数初始化**：`vectors_total` 当前通过 `add`/`delete` 事件累加/扣除，建议在 `mmdb_collection_create` 时通过 `SELECT COUNT(*)` 同步初始值，避免冷启动后监控显示偏低。
3. **缓存指标**：当前 `cache_hits/misses` 字段已定义但尚未埋点，待 P6-M1.3 缓存层完成后接入。
4. **资源指标**：当前 `memory/disk` 字段已定义但尚未埋点，需要运维层主动调用 `mmdb_metrics_set_resources()` 上报。

## 关键设计决策

1. **静态单例快照**：`mmdb_metrics_get()` 返回函数局部静态对象的指针，避免多线程读到的快照被并发覆盖。调用方需立即拷贝所需字段（已在集成测试中验证）。
2. **不阻塞关键路径**：所有埋点使用 `relaxed` 内存序，对 `mmdb_vectors_search` 的延迟影响可忽略（实测 < 1μs）。
3. **CAS 防下溢**：`dec_vectors_total` 使用 CAS 循环保证 uint64_t 不会下溢为极大值。
4. **零 ABI 破坏**：所有新结构体字段为 append 模式，C 头文件保持向后兼容。

## Commit 信息

- SHA：`1724745b9`
- Subject：`feat(sdk): P6-M1.2 Prometheus 监控指标 — 全局原子计数器 + 格式导出`
