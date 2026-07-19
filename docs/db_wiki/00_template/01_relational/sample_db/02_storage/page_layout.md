# 页面结构

## 学习目标
- 理解数据页的物理布局
- 掌握页面组织方式和空闲空间管理

## 核心概念

- **Page**：磁盘 I/O 的最小单位，通常 8KB
- **Page Header**：页面元数据
- **Item Pointer**：指向 Tuple 的指针

## 页面整体布局

```mermaid
graph TD
    subgraph "Page 结构 (8KB)"
        A[Page Header 24B]
        B[Item Pointer 数组]
        C[空闲空间]
        D[Special Space]
        E[Tuple 数据]
        A --> B --> C --> E
        D --> E
    end
```

## Page Header 结构

```mermaid
classDiagram
    class PageHeader {
        +pd_lsn: WAL 位置
        +pd_checksum: 校验和
        +pd_lower: 空闲起始
        +pd_upper: 空闲结束
        +pd_special: 特殊空间
        +pd_pagesize: 页面大小
    }
```

## 空闲空间管理

```mermaid
flowchart LR
    A[插入 Tuple] --> B{空闲空间足够?}
    B -->|是| C[分配空间]
    B -->|否| D[触发页面分裂或压缩]
    C --> E[更新 pd_lower/pd_upper]
    D --> E
```

## 要点总结

- 页面采用头尾双端增长布局
- Item Pointer 从前往后，Tuple 从后往前

## 思考题

1. 为什么 Item Pointer 和 Tuple 从两端增长？
2. 如何判断一个页面是否有足够空闲空间？