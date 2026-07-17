# A2: 向量查询执行器 - 任务清单

## 任务概述

实现统一的向量查询执行管道，整合粗排、精排、混合检索能力。

## 任务清单

### A. 接口审视

- [x] **A.1** 审视 `include/db/core/vector_exec.h` 现有接口
- [x] **A.2** 审视 `src/db/core/vector_exec.c` 现有实现 (如有)
- [x] **A.3** 确定需要扩展的接口

### B. 数据结构设计

- [x] **B.1** 设计 `VectorQueryPlan` 结构
- [x] **B.2** 设计 `VectorQueryContext` 执行上下文
- [x] **B.3** 设计 `VectorQueryResult` 结果结构
- [x] **B.4** 设计 `QueryConfig` 配置结构

### C. 执行器核心

- [x] **C.1** 实现 `vector_exec_init()` 初始化
- [x] **C.2** 实现 `vector_exec_shutdown()` 关闭
- [x] **C.3** 实现 `vector_exec_create_plan()` 创建计划
- [x] **C.4** 实现 `vector_exec_destroy_plan()` 销毁计划
- [x] **C.5** 实现 `vector_exec_execute()` 执行查询
- [x] **C.6** 实现 `vector_exec_cancel()` 取消查询

### D. 粗排阶段

- [x] **D.1** 实现 HNSW 粗排调度
- [x] **D.2** 实现 DiskANN 粗排调度
- [x] **D.3** 实现 IVF 粗排调度
- [x] **D.4** 实现索引选择逻辑

### E. 精排阶段

- [x] **E.1** 集成 ReRanker 接口
- [x] **E.2** 实现精确距离精排
- [x] **E.3** 实现 MMR 去重
- [x] **E.4** 实现多度量融合

### F. 混合检索

- [x] **F.1** 集成 BM25 检索
- [x] **F.2** 实现 RRF 融合
- [x] **F.3** 实现加权融合
- [x] **F.4** 实现分值归一化

### G. 优化与工具

- [x] **G.1** 实现查询计划缓存
- [x] **G.2** 实现执行统计
- [x] **G.3** 实现执行剖析 (profiling)
- [x] **G.4** 实现调试钩子

### H. 测试

- [x] **H.1** 单元测试 - 执行器核心
- [x] **H.2** 单元测试 - 两阶段检索
- [x] **H.3** 单元测试 - 混合检索
- [x] **H.4** 集成测试 - 与 A1 对接

## 文件清单

### 新建

```
src/db/core/vector_query.c       # 执行器实现
test/db/core/vector_query_test.cpp
```

### 修改

```
src/db/core/CMakeLists.txt
include/db/core/vector_query.h   # 新增接口
```

### 依赖

```
include/db/index/vector_index/reranker/reranker.h
include/db/index/vector_index/hybrid_search.h
include/db/index/vector_index/BM25/bm25.h
```

## 状态

- 创建时间: 2026-07-09
- 状态: 已完成
- 所属大变更: 2026-07-09-minivecdb-end-to-end
- 前置变更: 2026-07-09-vector-persist-layer
