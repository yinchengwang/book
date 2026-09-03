# C++ SOLID 原则

> SRP/OCP/LSP/ISP/DIP 五大面向对象设计原则

## 编译

```bash
g++ -std=c++17 -o main main.cpp
./main
```

## 五大原则

| 原则 | 全称 | 核心思想 |
|------|------|----------|
| SRP | 单一职责原则 | 一个类只有一个变化原因 |
| OCP | 开闭原则 | 对扩展开放，对修改封闭 |
| LSP | 里氏替换原则 | 子类必须可替换基类 |
| ISP | 接口隔离原则 | 多个专用接口优于一个胖接口 |
| DIP | 依赖倒置原则 | 依赖抽象而非具体 |

## 运行结果

```
=== SOLID 原则演示 ===

1. SRP (Single Responsibility):
Saving user: Alice
Email to Alice: Welcome!
Generating report for: Alice

2. OCP (Open/Closed):
Drawing Circle
Drawing Rectangle
Drawing Triangle

3. LSP (Liskov Substitution):
Area: 20
Area: 25

4. ISP (Interface Segregation):
Printing
Printing
Scanning
Faxing

5. DIP (Dependency Inversion):
MySQL: INSERT INTO orders...
PostgreSQL: INSERT INTO orders...
```
