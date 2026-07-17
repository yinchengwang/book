# Big O 时间空间复杂度

## 简介

Grokking 算法第一课——复杂度分析。

算法复杂度分析是评估算法效率的核心技能，通过 Big O  notation 描述算法在输入规模增长时的性能变化趋势。

## 目录结构

```
big_o/
├── main.c    # 复杂度分析演示代码
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
gcc -std=c11 -Wall -Wextra -o big_o main.c
./big_o
```

## 涵盖内容

- 时间复杂度: O(1), O(log n), O(n), O(n log n), O(n²), O(2^n)
- 空间复杂度: O(1), O(n), O(n²)
- 复杂度对比表格
- 常见算法对照
