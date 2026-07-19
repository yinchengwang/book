# 查询规划

## 学习目标
- 理解查询优化器的作用和工作原理
- 掌握查询计划的生成和选择

## 核心概念

- **查询优化器**：选择最优执行计划
- **逻辑优化**：基于规则的变换（如谓词下推）
- **物理优化**：基于代价的选择（如索引选择）
- **执行计划**：具体的执行步骤树

## 优化流程

```mermaid
flowchart TD
    A[语法树 AST] --> B[逻辑优化]
    B --> C[等价变换]
    C --> D[候选计划]
    D --> E[代价估算]
    E --> F[选择最优计划]
    F --> G[物理执行计划]
```

## 查询计划树示例

```mermaid
graph TD
    A[Hash Join] --> B[Seq Scan: orders]
    A --> C[Index Scan: customers]
    C --> D["Index: customers_pkey"]
```

## 常见优化规则

```mermaid
graph LR
    A[谓词下推] --> B[减少中间结果]
    C[列裁剪] --> D[减少 I/O]
    E[常量折叠] --> F[提前计算]
    G[子查询展开] --> H[转为 Join]
```

## 代价估算

| 操作 | 代价公式 |
|------|----------|
| 顺序扫描 | N_pages * seq_page_cost |
| 索引扫描 | N_index_pages + N_tuples * cpu_tuple_cost |
| Hash Join | build_cost + probe_cost |

## 要点总结

- 优化器分为逻辑优化和物理优化
- 代价模型基于统计信息估算

## 思考题

1. 统计信息不准确会影响优化效果吗？
2. 如何强制使用特定索引？