# ds-array 规格

## 概述

数组是最基本的数据结构，提供连续内存的线性存储。本规格定义数组的完整实现及其可视化文档要求。

## 目录结构

```
src/ds/array/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   ├── array.c       # C 语言实现
│   └── array.hpp     # C++ 实现
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `array_create()` | O(1) |
| 尾部插入 | `array_push_back()` | O(1) 均摊 |
| 尾部删除 | `array_pop_back()` | O(1) |
| 指定位置插入 | `array_insert()` | O(n) |
| 指定位置删除 | `array_delete()` | O(n) |
| 按索引访问 | `array_get()` | O(1) |
| 按索引修改 | `array_set()` | O(1) |
| 获取长度 | `array_size()` | O(1) |
| 判空 | `array_empty()` | O(1) |
| 清空 | `array_clear()` | O(n) |
| 查找元素 | `array_find()` | O(n) |

## README.md 文档要求

每个操作必须包含：
1. 函数签名
2. 具体数字示例（如 `[1, 2, 4, 5]` 插入 `3` 后变成 `[1, 2, 3, 4, 5]`）
3. Mermaid 流程图展示元素移动过程
4. 新增/删除/修改的元素用特殊颜色标识

## 数据结构

```c
typedef struct {
    int* data;     // 数据指针
    int size;      // 当前元素数量
    int capacity;  // 容量
} Array;
```

## 验收标准

- [ ] 所有基本操作实现完整
- [ ] README.md 包含所有操作的 Mermaid 可视化
- [ ] 每个操作有具体数字示例
- [ ] 复杂度分析准确
- [ ] CMakeLists.txt 正确配置
