# MVCC 多版本并发控制

## 学习目标
- 理解 MVCC 的核心思想和实现方式
- 掌握版本链的管理和可见性判断

## 核心概念

- **MVCC**：多版本并发控制，读不阻塞写，写不阻塞读
- **版本链**：同一行的多个版本通过指针链接
- **可见性**：根据事务 ID 判断哪个版本对当前事务可见

## 版本链结构

```mermaid
graph LR
    V3[版本3 最新] -->|指向旧版本| V2[版本2]
    V2 -->|指向旧版本| V1[版本1 最旧]
```

## Tuple 头部信息

```mermaid
classDiagram
    class TupleHeader {
        +xmin: 创建事务ID
        +xmax: 删除/更新事务ID
        +cid: 命令ID
        +infomask: 状态标志
    }
```

## 可见性判断

```mermaid
flowchart TD
    A[读取 Tuple] --> B{xmin 已提交?}
    B -->|否| C[不可见]
    B -->|是| D{xmin 是当前事务?}
    D -->|是| E[可见]
    D -->|否| F{xmax 已提交?}
    F -->|否| G[可见]
    F -->|是| H{xmax 是当前事务?}
    H -->|是| I[不可见]
    H -->|否| J[不可见]
```

## 要点总结

- MVCC 通过版本链实现读写并发
- 可见性判断基于事务 ID 和提交状态

## 思考题

1. MVCC 如何处理 Update 操作？
2. 版本链过长如何优化？