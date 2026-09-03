# R27 DS 基础 — 能力规格

## 能力概述

R27 DS 基础栈，包含 12 张数据结构与 C 语言基础卡片，涵盖数组、字符串、哈希表、栈、队列、链表、基础排序、位运算、指针、函数指针、控制流、动态内存。

## 12 张卡片规格

| ID | 卡片名 | 核心内容 | 行数 | 编译要求 |
|----|--------|----------|------|----------|
| array | 数组 | 静态/动态数组、索引访问、边界检查、2x扩容 | ~100 | gcc -std=c11 |
| string | 字符串 | 字符数组、strlen/strcpy/strcat/strcmp、UTF-8 编码 | ~100 | gcc -std=c11 |
| hash_table | 哈希表 | djb2 哈希函数、开放定址线性探测、再散列 | ~120 | gcc -std=c11 |
| stack | 栈 | 顺序栈/链式栈、Push/Pop、中缀转后缀表达式求值 | ~110 | gcc -std=c11 |
| queue | 队列 | 循环队列、链式队列、阻塞队列概念 | ~110 | gcc -std=c11 |
| linked_list | 链表 | 单链表/双链表、插入/删除/反转、哨兵节点 | ~120 | gcc -std=c11 |
| sort_basic | 基础排序 | 冒泡/选择/插入排序、时间复杂度、稳定性分析 | ~130 | gcc -std=c11 |
| bitwise | 位运算 | AND/OR/XOR/NOT/shift、位图、布隆过滤器原理 | ~110 | gcc -std=c11 |
| pointer | 指针 | 指针运算、函数指针、指针与数组、const 指针 | ~100 | gcc -std=c11 |
| function_pointer | 函数指针 | 回调函数、策略模式、qsort 比较器 | ~110 | gcc -std=c11 |
| control_flow | 控制流 | 条件/循环语句、状态机、goto 合理使用 | ~90 | gcc -std=c11 |
| dynamic_memory | 动态内存 | malloc/calloc/realloc/free、内存布局、常见错误 | ~120 | gcc -std=c11 |

## 共同规格

### 目录结构
```
learning/scaffold/ds/<card_id>/
├── main.c      # 中文注释，约 90-130 行
├── Makefile    # CC=gcc, CFLAGS=-std=c11 -Wall
├── README.md   # 学习目标、核心概念、编译说明
└── NOTES.md    # 工程对照（≥100字，含 engineering/ 源码引用）
```

### 编译验证
- 所有卡片必须 `gcc -std=c11 -Wall -o test main.c && ./test` 通过
- 输出包含算法/数据结构名称、执行演示、`=== PASS ===`

### 工程对照要求
每张卡的 NOTES.md 必须包含：
1. 核心原理（3-5 行）
2. 工程应用场景（≥100 字）
3. 引用 `engineering/` 具体文件路径和代码片段

## 学习路径

```
C 语言基础（R5-R10）
    ↓
R27 DS 基础 ← 当前
    ├── array / string / pointer / dynamic_memory → C 语言核心深化
    ├── linked_list / stack / queue / hash_table → 基础数据结构
    ├── sort_basic / bitwise / function_pointer / control_flow → 算法基础
    ↓
R28 DS 树结构
    ↓
R29 DS 图论与高级算法
```

## 验收标准

- [x] 12 张卡片全部创建完成
- [x] 所有 main.c 中文注释完整
- [x] 所有 Makefile 格式统一（gcc -std=c11 -Wall）
- [x] 所有 README.md 包含学习目标与编译说明
- [x] 所有 NOTES.md 包含工程对照（≥100 字）
- [x] 12 张卡编译运行验证全部通过（`=== PASS ===`）
- [x] r27-progress.md 创建（12 行）
- [x] learning 轨构建验证通过
