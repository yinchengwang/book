# 反模式 (Anti-Patterns)

## 简介

**反模式** 是看似解决问题但实际上带来更多问题的常见编程模式。识别和消除反模式是提升代码质量的关键技能。

本例展示三种常见反模式及其重构方案:

1. **GodClass (上帝类)** — 一个类承担过多职责, 违反单一职责原则 (SRP)
2. **HardCoded (硬编码)** — 魔法数字/字符串散落代码中, 降低可维护性
3. **Spaghetti (面条式代码)** — 深层嵌套循环 + 全局状态, 难以理解和测试

## 目录结构

```
anti_patterns/
├── main.py      # 反模式演示代码 (GodClass + HardCoded + Spaghetti)
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

1. GodClass 重构为 UserManager / OrderManager / InventoryManager 三个独立类
2. HardCoded 的魔法数字 0.15 拆分为有语义的 TAX_RATE 和 COMMISSION_RATE 常量
3. Spaghetti 的三层嵌套循环重构为纯函数 pipeline: filter -> double -> cap -> dedup
