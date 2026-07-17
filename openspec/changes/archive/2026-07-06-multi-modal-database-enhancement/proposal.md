# 变更：多模态数据库功能增强

## 概要

增强多模态数据库各模型的完成度，修复已知缺陷，实现核心查询功能。

## 背景

当前多模态数据库实现了8个数据模型，但存在以下核心问题：

1. **SPATIAL 模型**：snprintf UB（源目标buffer重叠）、缺少空间查询 API
2. **VECTOR 模型**：search 是空函数，无法进行 KNN 搜索
3. **TIMESERIES 模型**：query 是空函数，无法进行聚合查询
4. **RELATIONAL 模型**：heap_delete/update 实现不完整
5. **KV 模型**：tuple_update 未实现
6. **GRAPH 模型**：缺失头文件，CRUD 未实现
7. **DOCUMENT 模型**：drop 是空操作，缺少 JSON 查询

## 变更范围

### 文件修改
- `src/db/storage/spatial/spatial_engine.c` - 修复 snprintf UB，添加查询 API
- `include/db/spatial_engine.h` - 添加查询 API 声明
- `src/db/storage/vector/vector_engine.c` - 实现暴力 KNN 搜索
- `src/db/storage/ts/ts_engine.c` - 实现聚合查询
- `src/db/storage/heap/heapam.c` - 修复 delete/update
- `src/db/storage/kv/kv_engine.c` - 实现 tuple_update
- `src/db/storage/graph/graph_engine.c` - 实现 CRUD
- `src/db/storage/doc/doc_engine.c` - 修复 drop，实现 JSON 查询

### 新增文件
- `include/db/graph/graph.h` - 图数据库头文件
- `test/db/storage/multi_modal_enhancement_test.cpp` - 增强功能测试

## 影响分析

- 修复 snprintf UB 将消除潜在崩溃风险
- 实现的查询功能将使各模型真正可用
- 不影响现有已通过测试的功能

## 回滚计划

如出现问题，回退修改的文件即可，不影响其他模块。
