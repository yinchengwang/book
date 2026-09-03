# 外观模式

外观模式 (Facade Pattern) 示例代码。

## 描述

外观模式为复杂子系统提供统一的高层接口。本示例模拟一个在线购物结算系统，演示如何使用 Facade 封装库存管理、支付处理、物流配送和通知服务等多个子系统。

## 目录结构

```
pattern_facade/
├── main.py      # 主代码：包含子系统和外观实现
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

- **Facade**: 统一的高层接口，知道哪些子系统类负责哪些请求
- **子系统**: 实现具体功能，不知道 Facade 的存在
- **客户**: 只与 Facade 交互，不直接操作子系统
