# ds-hash 规格

## 概述

哈希表提供 O(1) 平均时间复杂度的键值对存储。本规格定义哈希表的完整实现及可视化文档要求。

## 目录结构

```
src/ds/hash/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   ├── hash_table.c  # 哈希表实现
│   └── hash_table.hpp
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `hash_create()` | O(1) |
| 插入 | `hash_insert()` | O(1) 均摊 |
| 查找 | `hash_lookup()` | O(1) 均摊 |
| 删除 | `hash_delete()` | O(1) 均摊 |
| 判空 | `hash_empty()` | O(1) |
| 获取大小 | `hash_size()` | O(1) |
| 遍历 | `hash_foreach()` | O(n) |

## README.md 文档要求

1. 哈希函数原理可视化
2. 哈希冲突处理（链地址法）
3. 插入/查找/删除的具体示例
4. 扩容过程可视化

## 数据结构

```c
typedef struct HashNode {
    int key;
    int value;
    struct HashNode* next;
} HashNode;

typedef struct {
    HashNode** buckets;  // 哈希桶
    int size;           // 元素数量
    int capacity;       // 桶数量
} HashTable;
```

## 验收标准

- [ ] 哈希表基本操作实现完成
- [ ] 链地址法冲突处理实现
- [ ] README.md 包含哈希冲突处理可视化
- [ ] 包含 LeetCode 1366、1367 题目
- [ ] CMakeLists.txt 正确配置
