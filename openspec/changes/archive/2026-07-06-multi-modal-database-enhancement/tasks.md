# 多模态数据库功能增强任务

## 任务状态

- [x] 待完成
- [x] 已完成

## 任务清单

### Phase 1: SPATIAL 空间模型增强 [P0]

- [x] **T1.1** 修复 `spatial_engine_table_open` 中 snprintf UB (L71)
  - 使用独立的 dir_path 缓冲区
- [x] **T1.2** 修复 `spatial_engine_get_stats` 中 snprintf UB (L170)
  - 使用独立的 dir_path 缓冲区
- [x] **T1.3** 添加 Windows 编译兼容性
  - 添加 `#include <direct.h>`
  - 添加 `#include <errno.h>`
  - 定义 `mkdir` 为 `_mkdir`
- [x] **T1.4** 实现 `spatial_engine_query_bbox` 空间查询
  - 声明在 `include/db/spatial_engine.h`
  - 暴力扫描 geometries.bin
  - 使用 `bbox_intersects` 判断相交
- [x] **T1.5** 实现 `spatial_engine_nearest` 最近邻查询
  - 声明在 `include/db/spatial_engine.h`
  - 暴力计算欧氏距离
  - 使用选择排序维护 top-k 结果

### Phase 2: VECTOR 向量模型增强 [P0]

- [x] **T2.1** 实现 `vector_engine_search` 暴力 KNN 搜索
  - 从 schema/dimension 读取维度
  - 顺序读取 vectors.bin
  - 调用 `vector_l2_distance` / `cosine_similarity`
  - 使用选择排序维护 top-k 结果
- [x] **T2.2** 修复 `vector_engine_free_results` 内存泄漏

### Phase 3: TIMESERIES 时序模型增强 [P0]

- [x] **T3.1** 实现 `ts_engine_query` 聚合查询
  - 支持 SUM/AVG/MIN/MAX/COUNT
  - 顺序读取 .tsd 文件
  - 使用 `ts_align_timestamp` 落桶
- [x] **T3.2** 消除重复工具函数

### Phase 4: RELATIONAL 关系模型增强 [P1]

- [x] **T4.1** 修复 `heap_delete` 实现
  - 读取页面、设置 t_xmax 标记
  - 标记页面为脏
  - 更新统计
- [x] **T4.2** 修复 `heap_update` 实现
  - 先标记旧元组为删除
  - 尝试在同一页面插入新元组
  - 页面空间不足时创建新页
- [x] **T4.3** 修复 `heap_getnext` 跳过已删除元组

### Phase 5: KV 键值模型增强 [P1]

- [x] **T5.1** 实现 `kv_engine_tuple_update`
  - 修改 storage_ops_t 中 tuple_update 指向
  - 直接调用 kv_put 覆盖已有值

### Phase 6: GRAPH 图模型增强 [P1]

- [x] **T6.1** 实现 `graph_engine_tuple_insert`
  - 支持顶点/边插入
  - 修改 storage_ops_t 指针
- [x] **T6.2** 实现 `graph_engine_tuple_delete`
  - 支持顶点/边删除
  - 修改 storage_ops_t 指针

### Phase 7: DOCUMENT 文档模型增强 [P1]

- [x] **T7.1** 实现 `doc_engine_table_drop`
  - 递归删除目录及文件
- [x] **T7.2** 修复 `doc_engine_table_open` snprintf UB
- [x] **T7.3** 修复 `doc_engine_get_stats` snprintf UB
- [x] **T7.4** 实现 JSONPath 查询 `doc_engine_query`
  - 支持 `$.field` 路径语法
  - 简单的字段匹配提取

## 验收标准

- [x] SPATIAL 模型 snprintf UB 修复，编译通过
- [x] SPATIAL 支持 bbox 范围查询和最近邻查询
- [x] VECTOR 支持 KNN 搜索，返回 top-k 结果
- [x] TIMESERIES 支持聚合查询 (SUM/AVG/MIN/MAX/COUNT)
- [x] RELATIONAL delete/update 正确标记元组
- [x] KV 支持 update 操作
- [x] GRAPH CRUD 可用
- [x] DOCUMENT 支持 drop 和 JSONPath 查询
- [x] 主目标 AlgorithmPractice 编译通过
