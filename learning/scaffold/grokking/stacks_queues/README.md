# 栈与队列

## 简介

Grokking 算法高频题型——栈与队列操作。

栈（后进先出）和队列（先进先出）是最基本的线性数据结构抽象，广泛应用在表达式求值、任务调度、图遍历等场景。

## 目录结构

```
stacks_queues/
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
gcc -std=c11 -Wall -Wextra -o stacks_queues main.c
./stacks_queues
```

## 涵盖内容

- 括号匹配: 栈 O(n)
- 最小栈: 辅助栈 O(1) min
- 约瑟夫环: 队列模拟 O(n*k)
