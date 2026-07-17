# NOTES.md - 执行引擎总结

## 端到端执行流程

```
SQL → Parser → AST → Rewriter → Logical Plan → Optimizer → Physical Plan → Executor → Result
```

## 核心组件

| 组件 | 职责 |
|-----|------|
| Parser | 词法分析 + 语法分析 |
| Rewriter | 谓词下推、常量折叠 |
| Planner | 逻辑计划生成 |
| Optimizer | 代价优化、Join 重排 |
| Executor | 火山模型执行 |

## 火山模型

- **优点**: 接口统一、实现简单
- **缺点**: 行式处理、CPU 利用率低
- **改进**: 向量化执行、代码生成

## 面试高频题

1. 描述 SQL 执行引擎的整体架构
2. 火山模型的优缺点
3. 火山模型 vs 向量化执行
4. 解释查询优化的流程