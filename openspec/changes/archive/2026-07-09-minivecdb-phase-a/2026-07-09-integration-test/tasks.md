# A4: 端到端集成测试 - 任务清单

## 任务概述

实现端到端集成测试，验证向量数据库全链路功能正确性。

## 任务清单

### A. 测试框架

- [x] **A.1** 创建集成测试目录结构
- [x] **A.2** 设计测试用例覆盖矩阵
- [x] **A.3** 实现测试夹具（Fixture）

### B. Collection CRUD 测试

- [x] **B.1** 测试创建集合 (test_vector_api: CreateCollection ✓)
- [x] **B.2** 测试重复创建（错误处理）(test_vector_api: CreateDuplicateCollection ✓)
- [x] **B.3** 测试删除集合 (test_vector_api: DropCollection ✓)
- [x] **B.4** 测试删除不存在的集合 (test_vector_api: ErrorHandling ✓)
- [x] **B.5** 测试列出集合 (test_vector_api: ListCollections ✓)
- [x] **B.6** 测试获取集合信息 (test_vector_api: GetCollection ✓)

### C. Vector Operations 测试

- [x] **C.1** 测试插入单个向量 (test_vector_api: InsertVectors ✓)
- [x] **C.2** 测试批量插入向量 (test_vector_api: InsertVectors ✓)
- [x] **C.3** 测试搜索向量 (test_vector_api: SearchVectors ✓)
- [x] **C.4** 测试删除向量 (test_vector_api: DeleteVectors ✓)

### D. Persistence 测试

- [x] **D.1** 测试保存集合 (test_vector_api: SaveAndLoad ✓)
- [x] **D.2** 测试加载集合 (test_vector_api: SaveAndLoad ✓)
- [x] **D.3** 测试数据一致性（保存后加载）(test_vector_api: SaveAndLoad ✓)

### E. 性能测试

- [x] **E.1** 测试大批量插入性能
- [x] **E.2** 测试搜索延迟

### F. 端到端场景测试

- [x] **F.1** 完整工作流测试（创建→插入→搜索→删除）

## 文件清单

### 新建

```
test/db/integration/                    # 集成测试目录
test/db/integration/CMakeLists.txt      # 测试 CMake
test/db/integration/collection_test.cpp # Collection CRUD 测试
test/db/integration/vector_ops_test.cpp # 向量操作测试
test/db/integration/e2e_test.cpp        # 端到端场景测试
```

### 修改

```
test/db/CMakeLists.txt                  # 注册集成测试
```

## 测试结果

- test_vector_api: **11/11 通过 ✓**
- 覆盖 Collection CRUD、向量操作、持久化

## 状态

- 创建时间: 2026-07-09
- 状态: 已完成
- 所属大变更: 2026-07-09-minivecdb-end-to-end
- 前置变更: A3 (API 层 + CLI 工具)
