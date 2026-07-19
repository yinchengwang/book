# 执行器

## 学习目标
- 理解火山模型执行器的原理
- 掌握常见执行算子的实现

## 核心概念

- **火山模型**：迭代器模型，每个算子实现 open-next-close 接口
- **执行算子**：Scan、Join、Agg、Sort 等
- **向量化执行**：批量处理多行数据

## 火山模型

```mermaid
graph TD
    A[Root 算子] -->|调用 next| B[子算子]
    B -->|返回一行| A
    B -->|调用 next| C[孙算子]
    C -->|返回一行| B
```

## 算子接口

```mermaid
classDiagram
    class Executor {
        <<interface>>
        +open()
        +next(): Tuple
        +close()
    }
    class SeqScan {
        +open()
        +next()
        +close()
    }
    class HashJoin {
        +open()
        +next()
        +close()
    }
    Executor <|.. SeqScan
    Executor <|.. HashJoin
```

## Hash Join 执行流程

```mermaid
sequenceDiagram
    participant HJ as Hash Join
    participant Build as Build 侧
    participant Probe as Probe 侧

    HJ->>Build: 扫描 Build 表
    Build-->>HJ: 构建 Hash 表
    HJ->>Probe: 扫描 Probe 表
    loop 每一行
        Probe-->>HJ: 探测 Hash 表
        HJ-->>HJ: 返回匹配行
    end
```

## 要点总结

- 火山模型是流水线式拉取执行
- 每个算子独立实现，易于扩展

## 思考题

1. 火山模型的性能瓶颈在哪里？
2. 向量化执行如何提升性能？