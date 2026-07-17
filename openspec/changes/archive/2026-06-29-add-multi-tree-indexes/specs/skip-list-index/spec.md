# Skip List 索引

## Purpose

实现 Skip List 索引数据结构，用于学习概率平衡数据结构。Skip List 通过随机层级实现平衡，无需复杂旋转操作，代码简洁且并发友好。

## Requirements

### Requirement: Skip List 核心操作

Skip List 实现 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 描述 |
|------|----------|------|
| 创建 | `skip_list_t* skip_list_create(skip_list_compare_fn compare)` | 创建索引 |
| 销毁 | `void skip_list_drop(skip_list_t *list)` | 释放所有内存 |
| 插入 | `int skip_list_insert(...)` | 插入 key-value |
| 删除 | `int skip_list_delete(...)` | 删除指定 key |
| 查找 | `void* skip_list_search(...)` | 查找 key 对应的 value |
| 范围查询 | `int skip_list_range(...)` | 范围查找 |

### Requirement: Skip List 数据结构

Skip List 节点 SHALL 包含：
- `key/value`：键值对
- `forward[]`：层级前进指针数组
- `level`：当前节点层级（1 ~ MAX_LEVEL）

默认配置：
- `MAX_LEVEL = 16`
- 晋升概率 `P = 0.5`

### Requirement: Skip List 操作语义

- **插入**：计算随机层级，从高层向低层插入
- **删除**：各层级同步删除
- **查找**：从最高层向下搜索
- **范围查询**：找到起点后顺序遍历

### Scenario: 基本操作

- **WHEN** 用户调用 `skip_list_create(NULL)`
- **THEN** 创建 Skip List，使用默认字节序比较

- **WHEN** 用户插入 key "A"、"B"、"C"
- **THEN** 按排序顺序存储，层级随机

- **WHEN** 用户调用 `skip_list_range("A", "C")`
- **THEN** 返回 [A, B, C] 范围内的所有元素