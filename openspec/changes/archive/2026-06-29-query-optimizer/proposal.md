# 查询优化器

## Why

当前查询执行器直接根据 AST 生成执行计划，没有经过任何优化。对于简单查询这没问题，但随着数据量增长，未优化的执行计划会导致严重性能问题。查询优化器通过规则和代价分析选择最优执行计划，显著提升查询性能。

## What Changes

### 规则优化 (RBO)
- 常量折叠：编译时计算常量表达式
- 谓词下推：将过滤条件下推到表扫描
- 列裁剪：只读取需要的列
- 子查询展开：简单子查询转连接

### 代价优化 (CBO)
- 代价模型：基于页面IO、CPU成本估算
- 统计信息：表行数、列基数、直方图
- 代价估算：为每个执行计划计算总代价
- 计划选择：选择代价最低的计划

### 索引选择
- 索引可用性检测：检查列是否有可用索引
- 索引匹配度评估：评估索引对查询的适用性
- 多索引选择：选择最优单个索引或索引组合

### 执行计划 (EXPLAIN)
- 计划树可视化：树形结构展示执行计划
- 节点信息：每步的类型、代价、估计行数
- 格式化输出：可读性强的文本格式

## Capabilities

### New Capabilities

- `rule-optimizer`: 基于规则的查询优化，常量折叠、谓词下推、子查询展开
- `cost-estimator`: 代价估算模型，计算执行计划的总代价
- `index-selector`: 索引选择器，从可用索引中选择最优
- `explain`: EXPLAIN 执行计划输出，显示计划结构和代价

### Modified Capabilities

- `query-executor`: 执行器需要支持优化后的计划树结构

## Impact

### 新增文件

```
src/db/optimizer/
├── optimizer.c/h          # 优化器主入口
├── rule_optimizer.c/h     # 规则优化
├── cost_estimator.c/h     # 代价估算
├── index_selector.c/h     # 索引选择
├── plan_node.c/h          # 计划节点定义
└── explain.c/h            # EXPLAIN 输出

include/db/optimizer/
└── optimizer.h            # 公共接口

test/db/
└── test_optimizer.cpp     # 优化器测试
```

### 修改文件

- `src/db/sql/sql_exec.c` - 集成优化器
- `src/db/sql/sql_parser.c` - 增强 AST 支持优化提示