# GoF 23 种设计模式全景

## 描述

本模块演示了 GoF (Gang of Four) 23 种经典面向对象设计模式中的 6 种代表模式, 覆盖三大类别:

- **创建型**: 单例模式 (Singleton), 工厂方法 (Factory Method)
- **结构型**: 适配器模式 (Adapter), 装饰器模式 (Decorator)
- **行为型**: 策略模式 (Strategy), 观察者模式 (Observer)

每种模式都有独立的测试用例和清晰的输出边界标记, 适合作为设计模式入门的参考样例。

## 目录结构

```
design_patterns/
├── main.py    # 主演示文件 (6 种模式)
├── Makefile   # 构建/运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 如何运行

```bash
# 运行演示
make run

# 或者直接使用 python3
python3 main.py

# 检查环境并运行
make check

# 清理 (无产物需要清理)
make clean
```

## 输出示例

```
============================================================
GoF 23 种设计模式全景演示
============================================================

[创建型] 单例模式 Singleton
  c1: DatabaseConfig(host=192.168.1.100, port=5432, db=testdb)
  c2: DatabaseConfig(host=192.168.1.100, port=5432, db=testdb)
  同一实例: True
...
```
