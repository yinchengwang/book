# C++ 创建型设计模式

> 单例、工厂、抽象工厂、建造者、原型模式

## 编译

```bash
g++ -std=c++17 -o main main.cpp
./main
```

## 包含模式

| 模式 | 说明 |
|------|------|
| 单例 (Singleton) | 全局唯一实例，线程安全 |
| 工厂方法 (Factory Method) | 子类决定创建哪个对象 |
| 抽象工厂 (Abstract Factory) | 创建一系列相关对象 |
| 建造者 (Builder) | 分步构建复杂对象 |
| 原型 (Prototype) | 通过拷贝创建对象 |

## 运行结果

```
=== C++ 创建型设计模式演示 ===

1. Singleton:
Singleton::operation()

2. Factory Method:
Using ConcreteProductA

3. Abstract Factory:
Rendering Windows Button

4. Builder:
Pizza with thin dough, tomato sauce, cheese topping

5. Prototype:
ConcretePrototype value: 42
```
