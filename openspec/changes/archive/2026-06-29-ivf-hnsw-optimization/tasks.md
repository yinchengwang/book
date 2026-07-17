# IVF-HNSW 优化实现任务

## 1. 目录结构创建

- [x] 1.1 创建 `include/index/vector_index/ivf_hnsw/` 目录
- [x] 1.2 创建 `src/index/vector_index/ivf_hnsw/` 目录
- [x] 1.3 创建 `test/vector_index/ivf_hnsw/` 目录
- [x] 1.4 在 `src/index/CMakeLists.txt` 中添加 ivf_hnsw 子目录

## 2. 头文件实现

- [x] 2.1 实现 `IndexIVFHNSW.h` - 公共接口定义
  - faiss_ivf_hnsw_params_t 参数结构
  - faiss_ivf_hnsw_t 类型声明
  - 创建/销毁 API
  - 生命周期 API（train/add/search/reset）
  - 参数配置 API（nprobe/ef_search/max_assignments）

- [x] 2.2 实现 `faiss_ivf_hnsw_private.h` - 内部结构与工具函数
  - faiss_ivf_hnsw 内部结构体定义
  - HNSW 图相关字段（levels/offsets/nbs）
  - Multi-assignment 分配表
  - 内部工具函数声明

## 3. HNSW 量化器实现

- [x] 3.1 实现 HNSW 量化器（集成在 faiss_ivf_hnsw_utils.c）
  - hnsw_cum_nb_neighbors() - 层高累计邻居数
  - hnsw_neighbor_range() - 邻居范围计算
  - hnsw_random_level() - 随机层高采样
  - hnsw_search_layer_candidates() - 层内候选搜索
  - hnsw_connect_points() - 连接两点
  - hnsw_insert_vector() - 向 HNSW 图插入中心点

## 4. IVF-HNSW 索引核心实现

- [x] 4.1 实现 `faiss_ivf_hnsw_index.c` - 创建/销毁/重置
  - faiss_ivf_hnsw_index_create() - 创建索引
  - faiss_ivf_hnsw_index_drop() - 销毁索引
  - faiss_ivf_hnsw_index_reset() - 重置索引
  - 参数校验逻辑

- [x] 4.2 实现 `faiss_ivf_hnsw_train.c` - 训练流程
  - 一级 K-Means 聚类
  - 二级 K-Means 聚类
  - HNSW 图构建（暂时跳过，使用暴力粗排）
  - 量化器训练（如启用）
  - 训练后状态检查

- [x] 4.3 实现 `faiss_ivf_hnsw_add.c` - 添加向量
  - 向量分配到 k 个最近桶
  - 量化编码（如启用）
  - 更新倒排列表
  - 更新分配表

- [x] 4.4 实现 `faiss_ivf_hnsw_search.c` - 搜索流程
  - HNSW 粗排（找 nprobe 个最近簇）
  - 二级桶展开（按距离排序）
  - 桶内扫描（精确距离计算或 ADC）
  - top-k 大顶堆维护

## 5. 参数配置实现

- [x] 5.1 实现 nprobe getter/setter
- [x] 5.2 实现 ef_search getter/setter
- [x] 5.3 实现 max_assignments getter/setter
- [x] 5.4 实现 quantization_params getter/setter
- [x] 5.5 实现 training_params getter/setter

## 6. 工具函数实现

- [x] 6.1 实现分配表内存分配/释放
- [x] 6.2 实现分配表查询函数
- [x] 6.3 实现 HNSW 图内存计算函数
- [x] 6.4 实现索引内存统计函数

## 7. 测试实现

- [x] 7.1 实现基础功能测试
  - 创建/销毁测试 ✅
  - 训练测试 ✅
  - 添加/搜索测试 ✅

- [x] 7.2 实现精度对比测试
  - vs 暴力搜索 recall@k ✅
  - 1万向量 Recall@3 = 82.67%

- [x] 7.3 实现 Multi-assignment 测试
  - k=1 vs k=2 recall 对比 ✅
  - k=1: 82.67% → k=2: 94.67% (提升 14.5%)

## 8. 性能优化

- [x] 8.1 修复 K-Means 迭代次数过多问题（n_init=1）
- [x] 8.2 修复搜索时 O(n×k) 全表扫描问题
- [x] 8.3 修复 add 函数内存泄漏问题

## 9. 已完成功能

- ✅ IVF 已支持 SQ/PQ 量化（继承自 IVF 模块）
- ✅ HNSW-IVF 基础设施已完成（待调试完整 HNSW 图构建）
- ✅ 聚簇大小平衡优化框架已实现（待集成到训练流程）
- ✅ Multi-assignment 功能已实现
- ✅ 二级聚类结构已实现

## 10. 待优化项

- [ ] 10.1 实现完整的 HNSW 图构建（当前跳过，使用暴力粗排）
  - 基础设施已完成，hnsw_insert_vector() 等函数已实现
  - 需要调试 hnsw_search_layer_candidates() 的邻居遍历逻辑

- [ ] 10.2 实现 IVF-IVF 二次聚类
  - 当前 nlist2 用于单簇内二次聚类
  - 需要实现跨簇的全局二次聚类

- [ ] 10.3 实现聚簇大小平衡优化
  - faiss_ivf_hnsw_balance.c 已创建
  - 需要集成到 faiss_ivf_hnsw_index_train() 中
  - 大簇分裂/边界向量重分配

- [ ] 10.4 实现桶边缘效应优化
  - 残差量化已实现
  - 需要实现边界感知搜索：考虑相邻桶的向量

- [ ] 10.5 支持 SQ/PQ 量化模式
  - IVF 模块已支持
  - 需要在 HNSW-IVF 中启用

- [ ] 10.6 性能基准测试（QPS、延迟）
  - 添加基准测试用例
  - 测试不同参数配置的性能
