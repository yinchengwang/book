# ART 自适应基数树

## Purpose

实现 ART（Adaptive Radix Tree）索引，用于学习现代内存索引设计。ART 根据 key 前缀长度自适应选择节点类型，在空间和性能间取得平衡，被用于 HyPer 数据库和多种内存数据库。

## Requirements

### Requirement: ART 核心操作

ART 实现 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 描述 |
|------|----------|------|
| 创建 | `art_tree_t* art_create(void)` | 创建空树 |
| 销毁 | `void art_destroy(art_tree_t *tree)` | 释放所有内存 |
| 插入 | `int art_insert(...)` | 插入 key-value |
| 删除 | `int art_delete(...)` | 删除指定 key |
| 查找 | `void* art_search(...)` | 查找 key 对应的 value |
| 范围查询 | `int art_range(...)` | 范围查询 |

### Requirement: ART 节点类型

ART SHALL 支持 4 种节点类型：

| 类型 | 子节点数 | 描述 |
|------|----------|------|
| N4   | 4        | 前 4 位唯一 |
| N16  | 16       | 前 8 位唯一 |
| N48  | 48       | 前 16 位唯一 |
| N256 | 256      | 前 32 位唯一 |

节点公共字段：
- `type`：节点类型
- `count`：子节点数量
- `prefix[]`：压缩前缀（可选）

### Requirement: ART 操作语义

- **插入**：沿 key 逐字节导航，匹配前缀后选择子节点
- **删除**：定位叶子，移除并更新父节点
- **查找**：逐字节匹配，无匹配时返回 NULL
- **节点切换**：当子节点数超过阈值时切换到更大节点类型

### Scenario: 基本操作

- **WHEN** 用户创建 `art_create()`
- **THEN** 创建空 ART 树

- **WHEN** 用户插入 key=[0x01, 0x02, 0x03] 和 key=[0x01, 0x02, 0x04]
- **THEN** 前两个字节共享前缀，第三个字节分裂为两个子节点

- **WHEN** 用户调用 `art_search([0x01, 0x02, 0x03])`
- **THEN** 沿字节路径查找，返回对应 value