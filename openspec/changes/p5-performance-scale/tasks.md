# P5 性能规模化 任务清单

## 任务列表

### Phase 0：解 Block
- [x] **T5.0** 解 block 现有 bug（合并 commit）
  - P5-7: embedding_test 期望值修复（`-2/-3` → `MMDB_ERR_NOT_IMPLEMENTED`）
  - X1: MSVC `__cpuidex` 兼容（`__get_cpuid_count` 跨平台分发）
  - X2: pthread_rwlock → 跨平台 wrapper（Windows SRWLOCK / POSIX pthread）

### Phase 1：SIMD + 跨平台
- [ ] **T5.5** SIMD AVX2 完整族 + HNSW/xquery 路径集成
  - L2 + 内积 + 余弦 全部 AVX2 加速
  - HNSW 搜索路径 + xquery 候选重排调用 SIMD
  - 跨平台编译验证（MSVC/GCC/Clang）

### Phase 2：Selector 集成
- [x] **T5.4** vector_index_selector 集成
  - vectors.c 调用 selector 替换硬编码阈值
  - selector 决策：HNSW/IVF-PQ/IVF/flat
  - fallback 策略：索引创建失败 → flat

### Phase 3：Recall + 阶梯
- [ ] **T5.7** 1M×128 Recall@10 验证
  - 1K 子集采样 GT，Recall@10 ≥ 0.85
  - 20 queries 全 GT 计算
- [ ] **T5.8** 100K/1M/10M 阶梯基准 + 性能报告
  - staircase_benchmark 脚本
  - `docs/performance-scale-report.md`

### Phase 4：性能优化
- [x] **T5.9** P5-1 哈希优化 + P5-3 最小堆 + P5-5 0 候选排查（已完成）
  - build_filter_ctx O(N) → O(1) 哈希查找
  - 选择排序 → 最小堆
  - 小数据集 0 候选根因修复（filtered search greedy descent）
  - Review 修复：hash collision safety（memcmp 校验）+ 未使用函数清理

### Phase 5：亿级 + 双模
- [ ] **P5-2** roaring bitmap 亿级优化
  - CRoaring 集成，bitmap 内存 -80%
- [ ] **P5-6** 双模同集合 SDK 架构
  - collection 支持多索引类型并存
  - VECTOR+TEXT 并存，hybrid 次通道真正激活

### Phase 6：Review + 归档
- [ ] **Whole-Branch Review**
- [ ] **归档**

## 完成记录

（待填充）
