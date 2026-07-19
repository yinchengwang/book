# 其他索引

## 学习目标
- 了解 GIN、GiST、BRIN 等索引的工作原理
- 掌握不同索引的适用场景

## 索引类型对比

```mermaid
graph TD
    A[BTree] -->|通用场景| B[等值 + 范围]
    C[Hash] -->|等值查询| D[精确匹配]
    E[GIN] -->|倒排索引| F[全文搜索/数组]
    G[GiST] -->|通用搜索树| H[空间/全文/模糊]
    I[BRIN] -->|块范围索引| J[超大表 + 有序数据]
```

## GIN 索引结构

```mermaid
graph LR
    subgraph "GIN 索引"
        A[Posting Tree 条目]
        B[条目: keyword1]
        C[条目: keyword2]
        B --> D[行 ID 列表]
        C --> E[行 ID 列表]
    end
```

## GiST 索引

GiST 是一棵平衡搜索树，支持自定义数据类型：
- **空间数据**：R-Tree 风格的 bounding box
- **全文搜索**：相似度查询
- **范围类型**：重叠检查

## BRIN 索引

适用于按物理顺序有序的大表：

```mermaid
graph TD
    subgraph "BRIN"
        B1["块范围 1<br/>min=100, max=199"]
        B2["块范围 2<br/>min=200, max=299"]
        B3["块范围 3<br/>min=300, max=399"]
    end
```

## 索引选型建议

| 场景 | 推荐索引 | 说明 |
|------|----------|------|
| 等值 + 范围 | BTree | 通用首选 |
| 全文搜索 | GIN | 倒排索引 |
| 空间查询 | GiST | R-Tree |
| 超大有序表 | BRIN | 极省空间 |

## 要点总结

- 不同索引适用于不同场景
- 索引选择直接影响查询性能

## 思考题

1. 一张表可以同时创建多个不同类型的索引吗？
2. BRIN 索引为什么不适合乱序数据？