# Radix Tree 前缀索引

## Purpose

实现 Radix Tree（压缩前缀树）索引，用于学习前缀压缩和最长前缀匹配。Radix Tree 在 Redis SDS 中大量使用，适合字符串键的高效存储和查找。

## Requirements

### Requirement: Radix Tree 核心操作

Radix Tree 实现 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 描述 |
|------|----------|------|
| 创建 | `radix_tree_t* radix_tree_create(void)` | 创建空树 |
| 销毁 | `void radix_tree_drop(radix_tree_t *tree)` | 释放所有内存 |
| 插入 | `int radix_tree_insert(...)` | 插入 key-value |
| 删除 | `int radix_tree_delete(...)` | 删除指定 key |
| 查找 | `void* radix_tree_search(...)` | 精确查找 |
| 前缀搜索 | `int radix_tree_prefix(...)` | 前缀匹配查找 |
| 最长前缀 | `void* radix_tree_longest_prefix(...)` | 最长前缀匹配 |

### Requirement: Radix Tree 数据结构

Radix Tree 节点 SHALL 包含：
- `prefix`：压缩的前缀字符串
- `prefix_len`：前缀长度
- `is_end`：是否为单词结尾
- `value`：当 is_end=true 时存储 value
- `children`：子节点映射（哈希表或数组）

### Requirement: Radix Tree 操作语义

- **插入**：沿路径匹配前缀，不匹配时分裂节点
- **删除**：移除叶子节点，向上合并可合并的节点
- **前缀搜索**：沿前缀深度遍历，返回所有匹配叶子
- **最长前缀**：贪婪匹配，记录最长成功匹配

### Scenario: 基本操作

- **WHEN** 用户插入 "foo"、"foobar"、"foobaz"
- **THEN** 树结构为：root -> "foob" -> "ar" / "az"，压缩存储

- **WHEN** 用户调用 `radix_tree_prefix("foo")`
- **THEN** 返回 ["foo", "foobar", "foobaz"]

- **WHEN** 用户调用 `radix_tree_longest_prefix("fooba")`
- **THEN** 返回 "foob"，因为 "fooba" 不是完整 key