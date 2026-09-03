# P5 性能规模化 设计文档

## 架构概览

P5 在 P4 ARCHIVED 基础上进行性能规模化，目标：1M × 128 规模搜索 ≥2000 qps + Recall@10 ≥0.95。

## 模块依赖

```
T5.0（解 block）
  ├─ P5-7: embedding_test 期望值
  ├─ X1: MSVC SIMD 检测
  └─ X2: 跨平台锁 wrapper
        ↓
T5.5（SIMD 完整族）
  ├─ distance.c: L2 + 内积 + 余弦 AVX2
  ├─ faiss_hnsw_search_filtered.c: SIMD 重排
  └─ xquery.c: SIMD 候选重排
        ↓
T5.4（selector 集成）
  ├─ vector_index_selector.c: 决策模块
  └─ vectors.c: 调用 selector
        ↓
T5.7（1M Recall）+ T5.8（阶梯 + 报告）
        ↓
T5.9（性能优化）✅
  ├─ P5-1: 哈希查找 ✅ (sdk_id_hash_t + memcmp 校验)
  ├─ P5-3: 最小堆 ✅ (MinimaxHeap 替换选择排序)
  └─ P5-5: 0 候选修复 ✅ (filtered search greedy descent)
        ↓
P5-2（roaring bitmap）+ P5-6（双模同集合）
        ↓
Whole-Branch Review → 归档
```

## 关键设计决策

### 1. SIMD 完整族（用户决策 #3）

- **范围**：L2 + 内积 + 余弦，全部 AVX2 加速
- **实现**：`distance.c` 新增 `inner_product_simd()` / `cosine_simd()`
- **集成**：HNSW 路径 + xquery 路径统一调用 SIMD
- **跨平台**：MSVC `__cpuidex` / GCC `__get_cpuid_count` / Clang 同 GCC

### 2. 跨平台锁 Wrapper（X2）

```c
/* mmdb_lock.h */
#ifdef _WIN32
typedef SRWLOCK mmdb_rwlock_t;
#else
typedef pthread_rwlock_t mmdb_rwlock_t;
#endif

int mmdb_rwlock_init(mmdb_rwlock_t *lock);
int mmdb_rwlock_rdlock(mmdb_rwlock_t *lock);
int mmdb_rwlock_wrlock(mmdb_rwlock_t *lock);
int mmdb_rwlock_unlock(mmdb_rwlock_t *lock);
int mmdb_rwlock_destroy(mmdb_rwlock_t *lock);
```

### 3. Roaring Bitmap（P5-2）

- **现状**：`build_filter_ctx` 分配 `id_map->count` 字节 bitmap
- **问题**：N=1亿时 = 100MB 内存
- **方案**：sorted array 实现的轻量级 CRoaring 兼容层（`third_part/croaring/roaring_bitmap.h`）
- **ABI**：`hnsw_filter_ctx_t` 结构体末尾 append roaring 指针（void* roaring）
- **阈值**：`ROARING_THRESHOLD = 100000`，bitmap_size 超过此值时切换到 roaring 路径

### 4. 双模同集合（P5-6）

- **现状**：VECTOR 集合无 FTS5，TEXT 集合无向量索引
- **问题**：T4.2 hybrid 次通道必然返回 0 候选
- **方案**：collection 通过 capability 标志位支持多索引类型并存
- **架构**：`mmdb_collection_t` 末尾 append `has_text` + `has_vector` capability 标志（int）
  - `MMDB_MODEL_TEXT` 集合默认 `has_text=1, has_vector=0`
  - `MMDB_MODEL_VECTOR` 集合默认 `has_text=0, has_vector=1`
  - 用户可通过 `mmdb_text_enable()` / `mmdb_vectors_enable()` 动态开启另一能力（幂等）
- **API**：`mmdb_text_enable(c)` / `mmdb_vectors_enable(c)`，置位 capability 后自动创建对应数据表
- **次通道路由**：text.c / vectors.c 中所有 `c->model != X` 检查替换为 `!c->has_X`，使 hybrid 次通道真正激活

## 验收标准

| 指标 | 目标 | 测试 |
|------|------|------|
| HNSW+filter qps | ≥2000 | T5.7/T5.8 |
| Recall@10 (1M) | ≥0.85 | T5.7 |
| Recall@10 (100K) | ≥0.95 | T5.8 |
| 跨平台 build | MSVC/GCC/Clang | T5.0/T5.5 |
| 亿级 bitmap | 内存 -80% | P5-2 |
| hybrid 次通道 | 真正激活 | P5-6 |

## 风险

| 风险 | 缓解 |
|------|------|
| Windows SRWLOCK 行为差异 | 单元测试覆盖 rdlock/wrlock 语义 |
| 10M 内存超限 | GTEST_SKIP 优雅跳过 |
| HNSW 1M Recall 不达标 | 调整 ef_search 参数 |
| roaring 集成复杂度 | CRoaring 是 C 库，API 简单 |
| 双模同集合 ABI 破坏 | 结构体末尾 append，零破坏 |
