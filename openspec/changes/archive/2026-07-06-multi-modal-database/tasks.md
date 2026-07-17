# 任务：多模态数据库架构重构

## 任务状态标记

- [ ] 待完成
- [x] 已完成

## 实现任务

### Phase 1: 目录结构重构

- [x] **T1.1** 创建 `data/` 顶层目录结构
  - 创建 `data/base/` 关系数据库目录
  - 创建 `data/kv/` KV 存储目录
  - 创建 `data/graph/` 图数据库目录
  - 创建 `data/vector/` 向量数据库目录
  - 创建 `data/timeseries/` 时序数据库目录
  - 创建 `data/document/` 文档数据库目录
  - 创建 `data/spatial/` 空间数据库目录
  - 创建 `data/yang/` Yang 数据库目录

- [x] **T1.2** 创建 `src/db/storage/` 子目录结构
  - 重构现有存储模块到子目录
  - 创建新的存储引擎目录

### Phase 2: 存储抽象层

- [x] **T2.1** 创建 `include/db/storage_engine.h`
  - 定义 `storage_ops_t` 接口结构
  - 定义 `DataModel` 枚举
  - 定义 `AccessMode` 枚举

- [x] **T2.2** 创建 `include/db/mm_storage.h`
  - 定义 `mm_context_t` 上下文结构
  - 声明上下文管理 API
  - 声明模型操作 API

- [x] **T2.3** 创建 `src/db/mm_storage.c`
  - 实现上下文初始化/关闭
  - 实现引擎注册机制
  - 实现模型路由

### Phase 3: KV 存储引擎

- [x] **T3.1** 创建 `include/db/kv_engine.h`
  - 定义 KV 上下文结构
  - 声明 KV API

- [x] **T3.2** 创建 `src/db/storage/kv_engine.c`
  - 实现 KV 存储接口
  - 复用现有 KV 实现

### Phase 4: 向量存储引擎

- [x] **T4.1** 创建 `include/db/vector_engine.h`
  - 定义向量上下文结构
  - 定义向量搜索结构
  - 声明向量 API

- [x] **T4.2** 创建 `src/db/storage/vector_engine.c`
  - 实现向量存储接口
  - 实现暴力搜索和距离计算

### Phase 5: 时序存储引擎

- [x] **T5.1** 创建 `include/db/ts_engine.h`
  - 定义时序上下文结构
  - 定义数据点结构
  - 声明时序 API

- [x] **T5.2** 创建 `src/db/storage/ts_engine.c`
  - 实现时序存储接口
  - 实现聚合查询

### Phase 6: 文档存储引擎

- [x] **T6.1** 创建 `include/db/doc_engine.h`
  - 定义文档上下文结构
  - 声明文档 API

- [x] **T6.2** 创建 `src/db/storage/doc_engine.c`
  - 实现文档存储接口

### Phase 7: 空间存储引擎

- [x] **T7.1** 创建 `include/db/spatial_engine.h`
  - 定义空间上下文结构
  - 定义几何类型
  - 声明空间 API

- [x] **T7.2** 创建 `src/db/storage/spatial_engine.c`
  - 实现空间存储接口
  - 实现空间索引

### Phase 8: Yang 存储引擎

- [x] **T8.1** 创建 `include/db/yang_engine.h`
  - 定义 Yang 上下文结构
  - 定义节点结构
  - 声明 Yang API

- [x] **T8.2** 创建 `src/db/storage/yang_engine.c`
  - 实现 Yang 存储接口
  - 实现层次遍历

### Phase 9: 图存储引擎 (扩展)

- [x] **T9.1** 扩展现有图存储
  - 创建 `include/db/graph_engine.h`
  - 创建 `src/db/storage/graph_engine.c`
  - 集成到 mm_storage 框架
  - 实现 storage_ops_t 接口

### Phase 10: CMake 构建集成

- [x] **T10.1** 更新 `src/db/CMakeLists.txt`
  - 添加新存储引擎到构建

- [x] **T10.2** 更新 `src/db/storage/CMakeLists.txt`
  - 重组存储模块构建（使用 GLOB_RECURSE 自动收集）

### Phase 11: 测试

- [x] **T11.1** 创建单元测试
  - 创建 `test/db/storage/mm_storage_test.cpp`
  - 测试多模态存储管理器
  - 测试各引擎功能

## 依赖关系

```
T1.1, T1.2 (目录结构)
         │
         ▼
T2.1, T2.2 (存储抽象层) ──→ T2.3 (mm_storage 实现)
         │
         ▼
T3.1 ─→ T3.2
T4.1 ─→ T4.2
T5.1 ─→ T5.2
T6.1 ─→ T6.2
T7.1 ─→ T7.2
T8.1 ─→ T8.2 (Yang 引擎)
T9.1 (Graph 扩展)
         │
         ▼
T10.1 ─→ T10.2
         │
         ▼
T11.1 (测试)
```

## 验收标准

1. 目录结构符合设计规范
2. 存储抽象层正常工作
3. 各引擎可独立初始化
4. 现有 SQL/图功能保持兼容

## 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/db/storage_engine.h` | 新建 | 存储引擎接口 |
| `include/db/mm_storage.h` | 新建 | 多模态管理器 |
| `include/db/kv_engine.h` | 新建 | KV 引擎头文件 |
| `include/db/vector_engine.h` | 新建 | 向量引擎头文件 |
| `include/db/ts_engine.h` | 新建 | 时序引擎头文件 |
| `include/db/doc_engine.h` | 新建 | 文档引擎头文件 |
| `include/db/spatial_engine.h` | 新建 | 空间引擎头文件 |
| `include/db/yang_engine.h` | 新建 | Yang 引擎头文件 |
| `include/db/graph_engine.h` | 新建 | 图引擎头文件 |
| `src/db/mm_storage.c` | 新建 | 多模态管理器实现 |
| `src/db/storage/kv_engine.c` | 新建 | KV 引擎实现 |
| `src/db/storage/vector_engine.c` | 新建 | 向量引擎实现 |
| `src/db/storage/ts_engine.c` | 新建 | 时序引擎实现 |
| `src/db/storage/doc_engine.c` | 新建 | 文档引擎实现 |
| `src/db/storage/spatial_engine.c` | 新建 | 空间引擎实现 |
| `src/db/storage/yang_engine.c` | 新建 | Yang 引擎实现 |
| `src/db/storage/graph_engine.c` | 新建 | 图引擎实现 |
| `test/db/storage/mm_storage_test.cpp` | 新建 | 单元测试 |

## 注意事项

1. 保持向后兼容，不破坏现有 API
2. 遵循项目中文注释规范
3. 使用 errors.h 和 log.h 进行错误处理和日志记录
