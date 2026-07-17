# 组合模式 (Composite Pattern)

## 简介

组合模式将对象组合成树形结构以表示部分-整体层次, 使客户代码
可以一致地处理简单和复杂元素。本示例展示: 文件系统树、组织架构树、
表达式计算树, 以及透明/安全两种实现风格。

## 目录结构

```
pattern_composite/
├── main.py      # 组合模式演示代码
├── Makefile     # 构建和运行配置
├── README.md    # 本文件
└── NOTES.md     # 学习笔记
```

## 运行方式

```bash
make run
# 或直接运行:
python3 main.py
```

## 示例要点

1. 文件系统: 文件 (Leaf) + 目录 (Composite) 树
2. 组织架构: 员工 (Leaf) + 部门 (Composite) 树
3. 表达式: 数字 (Leaf) + 运算符 (Composite) 树
4. 透明 vs 安全两种实现风格对比
