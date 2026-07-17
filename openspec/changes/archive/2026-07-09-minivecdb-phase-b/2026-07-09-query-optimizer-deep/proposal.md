# B2: 查询优化器深度

## 概述

完善查询优化器，实现代价模型、索引选择、Join 重排等高级优化能力。

## 问题背景

当前优化器状态：
- ✅ SQL Parser 完备
- ✅ Rewrite 规则已实现
- ❌ 代价模型缺失
- ❌ 索引选择未实现
- ❌ Join 重排未实现
- ❌ 向量查询计划未优化

## 目标

- 代价模型实现
- 索引选择策略
- Join 重排算法
- 向量查询计划优化

## 任务清单

### 代价模型

- [ ] B2.1 设计代价模型框架
- [ ] B2.2 实现页面代价估算
- [ ] B2.3 实现 IO 代价估算
- [ ] B2.4 实现 CPU 代价估算
- [ ] B2.5 实现统计信息收集
- [ ] B2.6 实现直方图统计
- [ ] B2.7 实现基数估算
- [ ] B2.8 实现代价计算器

### 索引选择

- [ ] B2.9 设计索引选择器
- [ ] B2.10 实现单列索引选择
- [ ] B2.11 实现多列索引选择
- [ ] B2.12 实现复合索引评估
- [ ] B2.13 实现部分索引考虑
- [ ] B2.14 实现覆盖索引识别
- [ ] B2.15 实现索引成本对比

### Join 重排

- [ ] B2.16 设计 Join 顺序规划器
- [ ] B2.17 实现 Join 图构建
- [ ] B2.18 实现 Selinger 算法
- [ ] B2.19 实现贪心 Join 重排
- [ ] B2.20 实现 Buschke 算法
- [ ] B2.21 实现 Join 类型选择 (Hash/NL/Merge)

### 向量查询优化

- [ ] B2.22 设计向量查询计划节点
- [ ] B2.23 实现 ANN Search 代价估算
- [ ] B2.24 实现索引选择 (HNSW/DiskANN/IVF)
- [ ] B2.25 实现混合检索计划
- [ ] B2.26 实现参数调优 (ef_search/M)

### 计划缓存

- [ ] B2.27 实现计划缓存
- [ ] B2.28 实现缓存失效策略
- [ ] B2.29 实现自适应查询

### 测试

- [ ] B2.30 代价模型测试
- [ ] B2.31 索引选择测试
- [ ] B2.32 Join 重排测试
- [ ] B2.33 向量查询优化测试

## 关键文件

### 新建

```
src/db/optimizer/cost_model.c
src/db/optimizer/index_selector.c
src/db/optimizer/join_planner.c
src/db/optimizer/vector_plan.c
src/db/optimizer/plan_cache.c
include/db/optimizer/cost_model.h
include/db/optimizer/index_selector.h
include/db/optimizer/join_planner.h
include/db/optimizer/vector_plan.h
test/db/optimizer/cost_model_test.cpp
test/db/optimizer/join_planner_test.cpp
```

### 修改

```
src/db/optimizer/
src/db/optimizer/CMakeLists.txt
```

## 技术方案

### 代价模型

```c
// 总代价 = IO 代价 + CPU 代价
TotalCost = (#sequential_pages * seq_page_cost) +
            (#random_pages * rand_page_cost) +
            (#tuples * cpu_tuple_cost) +
            (#comparisons * cpu_operator_cost)

// 统计信息
typedef struct {
    int64_t num_rows;           // 行数
    int64_t num_pages;          // 页数
    double avg_width;            // 平均行宽
    Histogram *hist;            // 直方图
    double n_distinct;          // 不同值数量
} TableStats;
```

### 索引选择决策树

```
选择最佳索引:
├── 检查 WHERE 子句
│   ├── 单列条件 → 评估单列索引
│   ├── 多列条件 → 评估复合索引
│   └── 范围条件 → 评估索引可用性
├── 计算索引代价
│   ├── 索引扫描代价
│   └── 回表代价
├── 选择覆盖索引
└── 返回最优索引组合
```

## 依赖关系

- 前置: A4 (Phase A 完成)
- 可并行: B1, B3

## 评估标准

- [ ] 代价模型可估算查询成本
- [ ] 索引选择选择正确索引
- [ ] Join 重排产生最优计划
- [ ] 向量查询计划可优化
