# 设计模式全景 学习笔记

## 核心概念

- **GoF 23**: Gang of Four 提出的 23 种经典面向对象设计模式
- **创建型 (5)**: Singleton, Factory Method, Abstract Factory, Builder, Prototype
- **结构型 (7)**: Adapter, Bridge, Composite, Decorator, Facade, Flyweight, Proxy
- **行为型 (11)**: Template, Strategy, Observer, Command, Iterator, State, Visitor, Mediator, Memento, Chain, Interpreter

## 六大设计原则 (SOLID +)

| 缩写 | 原则 | 核心思想 |
|------|------|----------|
| SRP | 单一职责 | 一个类只做一件事 |
| OCP | 开闭原则 | 对扩展开放, 对修改关闭 |
| LSP | 里氏替换 | 子类可替代父类 |
| ISP | 接口隔离 | 接口尽量小且专一 |
| DIP | 依赖倒置 | 依赖抽象而非具体 |
| LoD | 迪米特法则 | 最少知识原则 |

## 工程对照

设计原则在 `engineering/` 轨的存储引擎中有直接体现。`engineering/include/db/storage_engine.h` 定义了多模态存储引擎的统一接口 (开闭原则), 各类引擎 (KV/Vector/Graph/Document) 通过实现该接口来注册到系统中。`engineering/src/db/` 中各模块通过 `db/catalog.h` 定义的元数据接口交互 (依赖倒置), 而不是直接访问系统表。`engineering/src/db/txn/` 中的事务管理器只负责事务的 ACID 保证 (单一职责), 不混入存储引擎的页面管理逻辑。

## 面试要点

1. 设计模式是"工具箱", 不要为了用而用
2. 优先用语言特性解决问题 (如 Python 装饰器替代 Decorator 模式)
3. 模式之间的对比 (Strategy vs State, Adapter vs Bridge) 是高频面试题
