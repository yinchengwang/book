# 观察者模式

## 描述

本模块演示了观察者模式 (Observer Pattern) 及其发布-订阅变体的实现方式。观察者模式定义了对象间的一对多依赖关系, 当一个对象 (Subject) 的状态发生变化时, 所有依赖它的对象 (Observer) 都会收到通知并自动更新。

本演示涵盖:
- **经典 Observer 模式**: 通过 `Subject` / `Observer` 抽象类实现观察者注册、注销和事件广播
- **具体观察者**: `EmailNotifier` / `SMSNotifier` / `PushNotifier` 分别模拟不同渠道的通知
- **订单系统场景**: `OrderService` 作为 Subject, 在订单状态变更时通知所有注册的观察者
- **EventEmitter 变体**: 基于事件名的发布-订阅模式, 支持 `on` / `off` / `emit` 操作

## 目录结构

```
pattern_observer/
├── main.py    # 主演示文件
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
