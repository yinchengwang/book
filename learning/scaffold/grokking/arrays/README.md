# 数组

## 简介

Grokking 算法高频题型——数组操作。

数组是最基础的数据结构，掌握其核心操作对于算法面试至关重要。

## 目录结构

```
arrays/
├── main.c    # 算法题演示代码
├── Makefile  # 构建配置
├── README.md # 本文件
└── NOTES.md  # 学习笔记
```

## 运行方法

```bash
# 编译
make

# 运行
make run

# 清理
make clean

# 或手动编译运行
gcc -std=c11 -Wall -Wextra -o arrays main.c
./arrays
```

## 涵盖内容

- 两数之和: 哈希表法 O(n)
- 旋转数组: 二分查找 O(log n)
- 最大子数组和: 滑动窗口 O(n)
