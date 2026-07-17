# A4: 端到端集成测试

## 概述

建立完整的端到端集成测试体系，验证全链路正确性。

## 问题背景

当前测试状态：
- 各模块有单元测试 (103 个测试文件)
- 缺乏模块间集成测试
- 缺乏端到端功能测试
- 缺乏故障恢复测试

## 目标

- Collection CRUD 集成测试
- 向量插入→持久化→查询链路测试
- 分布式功能测试
- 故障恢复测试
- 性能回归测试

## 任务清单

### 测试框架

- [ ] A4.1 设计集成测试框架
- [ ] A4.2 定义测试 fixture
- [ ] A4.3 实现测试环境搭建/清理
- [ ] A4.4 实现测试数据生成器

### Collection CRUD 测试

- [ ] A4.5 测试 create_collection
- [ ] A4.6 测试 drop_collection
- [ ] A4.7 测试 list_collections
- [ ] A4.8 测试 describe_collection
- [ ] A4.9 测试 collection 不存在/已存在边界

### 向量操作测试

- [ ] A4.10 测试向量插入
- [ ] A4.11 测试批量插入
- [ ] A4.12 测试向量查询
- [ ] A4.13 测试向量删除
- [ ] A4.14 测试向量更新
- [ ] A4.15 测试批量操作原子性

### 持久化测试

- [ ] A4.16 测试插入后持久化
- [ ] A4.17 测试重启后数据恢复
- [ ] A4.18 测试部分持久化 (模拟崩溃)
- [ ] A4.19 测试 WAL 回放
- [ ] A4.20 测试检查点恢复

### 搜索质量测试

- [ ] A4.21 测试 TopK 准确性
- [ ] A4.22 测试 Recall@K
- [ ] A4.23 测试不同度量类型
- [ ] A4.24 测试边界条件 (空集合/全匹配)

### 分布式测试

- [ ] A4.25 测试单节点分片
- [ ] A4.26 测试多副本一致性
- [ ] A4.27 测试故障转移
- [ ] A4.28 测试数据迁移

### 性能回归测试

- [ ] A4.29 定义性能基准
- [ ] A4.30 实现插入性能测试
- [ ] A4.31 实现搜索性能测试
- [ ] A4.32 实现内存使用测试
- [ ] A4.33 配置 CI 性能回归检查

## 关键文件

### 新建

```
test/db/integration/CMakeLists.txt
test/db/integration/collection_test.cpp      # Collection CRUD
test/db/integration/vector_ops_test.cpp     # 向量操作
test/db/integration/persistence_test.cpp    # 持久化测试
test/db/integration/search_quality_test.cpp  # 搜索质量
test/db/integration/distributed_test.cpp     # 分布式测试
test/db/integration/perf_regression_test.cpp # 性能回归
test/db/integration/test_fixture.h           # 测试固件
test/db/integration/test_util.h              # 测试工具
```

### 测试结构

```cpp
// test_fixture.h
class VecDBTestFixture : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    // 测试辅助
    void CreateTestCollection(const char* name);
    void InsertTestVectors(int count);
    void VerifyVectorsExist(int count);
};

// collection_test.cpp
TEST_F(VecDBTestFixture, CreateCollection) { ... }
TEST_F(VecDBTestFixture, DropCollection) { ... }

// persistence_test.cpp
TEST_F(VecDBTestFixture, PersistAndRecover) { ... }
TEST_F(VecDBTestFixture, CrashRecovery) { ... }
```

## 依赖关系

- 前置: A1, A2, A3 (全部完成)
- 后置: Phase A 完成

## 评估标准

- [ ] 所有集成测试通过
- [ ] 崩溃恢复测试通过
- [ ] 性能回归测试通过
- [ ] CI 配置完成
