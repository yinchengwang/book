# 组合模式 学习笔记

## 核心概念

- **组合模式 (Composite)**: 将对象组合成树形结构以表示部分-整体层次
- **叶子节点 (Leaf)**: 没有子节点的元素
- **组合节点 (Composite)**: 包含子节点的容器
- **统一接口**: Leaf 和 Composite 实现相同接口

## 关键要点

| 角色 | 行为 | 子节点 |
|------|------|--------|
| Component | 声明统一接口 | 可选 (默认实现返回 0/空) |
| Leaf | 实现具体行为 | 无 |
| Composite | 组合子节点, 委派操作 | 有, 负责增删管理 |

## 工程对照

树形层次结构在 `engineering/` 轨中最直接的体现是 BTree 索引。`engineering/src/db/btreeam/btreeam.c` 实现了 B+ 树索引结构，其中内部节点 (Composite) 和叶子节点 (Leaf) 都通过统一的页面结构进行操作。B+ 树的内节点只存储键值和子节点指针，叶子节点存储实际数据。`engineering/include/db/btreeam.h` 中定义的搜索/插入/删除操作对内部节点和叶子节点使用相同的抽象接口。数据库的 Catalog 系统 (`engineering/include/db/catalog.h`) 维护的系统表也构成了层次结构——pg_database -> pg_class -> pg_attribute。

## 面试要点

1. 组合模式使客户代码可以一致地处理简单和复杂元素
2. 安全 vs 透明: 安全方式在 Composite 定义子节点管理方法, 透明方式在 Component 定义
3. 递归操作是组合模式的核心遍历方式
