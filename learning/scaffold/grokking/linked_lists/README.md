# 链表技巧

## 简介

Grokking 算法高频题型——链表操作。

链表是面试中出现频率极高的数据结构，掌握指针操作和常用技巧是解决链表问题的关键。

## 目录结构

```
linked_lists/
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
gcc -std=c11 -Wall -Wextra -o linked_lists main.c
./linked_lists
```

## 涵盖内容

- 反转链表: 迭代法 O(n) + 递归法 O(n)
- 环检测: Floyd 判环算法 O(n)
- 合并有序链表: 双指针归并 O(n+m)
- 找中点: 快慢指针 O(n)
