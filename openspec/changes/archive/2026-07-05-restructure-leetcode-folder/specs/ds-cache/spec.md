# ds-cache 规格

## 概述

缓存数据结构实现 LRU（最近最少使用）和 LFU（最不经常使用）算法。

## 目录结构

```
src/ds/cache/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   ├── lru_cache.c    # LRU 缓存实现
│   └── lfu_cache.c    # LFU 缓存实现
└── problems/          # LeetCode 题目
```

## 实现功能

### LRU Cache

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `lru_create()` | O(1) |
| 获取 | `lru_get()` | O(1) |
| 设置 | `lru_put()` | O(1) |

### LFU Cache

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `lfu_create()` | O(1) |
| 获取 | `lfu_get()` | O(1) |
| 设置 | `lfu_put()` | O(1) |

## README.md 文档要求

1. LRU 原理可视化（双向链表 + 哈希表）
2. LFU 原理可视化（频率桶）
3. get/put 操作的具体示例

## 验收标准

- [ ] LRU Cache 实现完成
- [ ] LFU Cache 实现完成
- [ ] README.md 包含缓存淘汰过程可视化
- [ ] CMakeLists.txt 正确配置
