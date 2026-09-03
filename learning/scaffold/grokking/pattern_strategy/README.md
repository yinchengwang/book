# 策略模式

## 描述

策略模式（Strategy Pattern）定义一族算法，将每个算法封装起来，使它们可以互相替换。策略模式让算法的变化独立于使用它的客户端。

## 目录结构

```
pattern_strategy/
├── main.py      # 主程序：支付策略 + 排序策略演示
├── Makefile     # 构建/运行脚本
├── README.md    # 本文件
└── NOTES.md     # 工程对比与面试要点
```

## 运行

```bash
cd learning/scaffold/grokking/pattern_strategy
make run
```

## 核心角色

| 角色 | 类 | 职责 |
|------|-----|------|
| Strategy | `PaymentStrategy` / `SortStrategy` | 声明算法接口 |
| ConcreteStrategy | `CreditCardPayment` / `QuickSort` 等 | 实现具体算法 |
| Context | `PaymentContext` | 持有策略引用，委托执行 |
