# 适配器模式

适配器模式 (Adapter Pattern) 示例代码。

## 描述

适配器模式使不兼容的接口能够协同工作。本示例模拟一个媒体播放器系统，演示对象适配器和类适配器两种实现方式。

## 目录结构

```
pattern_adapter/
├── main.py      # 主代码：包含目标接口、被适配者、适配器和客户端
├── Makefile     # 构建和运行配置
├── README.md    # 本文件
└── NOTES.md     # 学习笔记
```

## 如何运行

```bash
# 运行程序
make run

# 检查环境并运行
make check

# 清理（无操作）
make clean
```

或直接使用 Python 运行：

```bash
python3 main.py
```

## 关键概念

- **对象适配器**: 通过组合实现，灵活度高
- **类适配器**: 通过多继承实现，适用场景有限
- **Target**: 客户端期望的接口
- **Adaptee**: 需要适配的现有类
