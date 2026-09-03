# C++ 结构型设计模式

> 适配器、桥接、组合、装饰器、外观、代理模式

## 编译

```bash
g++ -std=c++17 -o main main.cpp
./main
```

## 包含模式

| 模式 | 说明 |
|------|------|
| 适配器 (Adapter) | 将不兼容接口转换为兼容接口 |
| 桥接 (Bridge) | 分离抽象与实现 |
| 组合 (Composite) | 树形结构统一处理 |
| 装饰器 (Decorator) | 动态添加功能 |
| 外观 (Facade) | 简化复杂子系统 |
| 代理 (Proxy) | 控制对象访问 |

## 运行结果

```
=== C++ 结构型设计模式演示 ===

1. Adapter:
Legacy: Hello via adapter

2. Bridge:
Vector: drawing circle at (1,2) r=3

3. Composite:
Dir: root/
  File: file1.txt
  Dir: subdir/
    File: file2.txt

4. Decorator:
Simple Coffee, Milk, Sugar = $2.7

5. Facade:
CPU: Freezing...
Memory: Loading at 0
CPU: Jumping to 0
CPU: Executing...

6. Proxy:
Loading photo.jpg from disk...
Displaying photo.jpg
Displaying photo.jpg
```
