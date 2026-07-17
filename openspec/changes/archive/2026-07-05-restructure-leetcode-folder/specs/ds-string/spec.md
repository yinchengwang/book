# ds-string 规格

## 概述

字符串处理是编程中的基础能力。本规格定义常用字符串操作的实现及可视化文档要求。

## 目录结构

```
src/ds/string/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   └── string.c       # 字符串操作实现
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 说明 |
|------|--------|------|
| 长度 | `str_len()` | 获取字符串长度 |
| 复制 | `str_copy()` | 字符串复制 |
| 连接 | `str_concat()` | 字符串拼接 |
| 比较 | `str_compare()` | 字符串比较 |
| 查找子串 | `str_find()` | 模式匹配 |
| 旋转 | `str_rotate()` | 字符串旋转 |
| 翻转 | `str_reverse()` | 字符串翻转 |

## README.md 文档要求

1. 字符串内存布局可视化
2. 每个操作的具体示例（如 `"hello"` 翻转后变成 `"olleh"`）
3. KMP 算法过程可视化（如有实现）

## 验收标准

- [ ] 基本字符串操作实现完成
- [ ] README.md 包含操作示例
- [ ] 包含 LeetCode 0206、0551 等题目
- [ ] CMakeLists.txt 正确配置
