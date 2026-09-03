# C++ 行为型设计模式

> 策略、观察者、命令、迭代器、模板方法、访问者模式

## 编译

```bash
g++ -std=c++17 -o main main.cpp
./main
```

## 包含模式

| 模式 | 说明 |
|------|------|
| 策略 (Strategy) | 算法家族可互换 |
| 观察者 (Observer) | 一对多依赖通知 |
| 命令 (Command) | 请求封装为对象 |
| 迭代器 (Iterator) | 顺序访问聚合对象 |
| 模板方法 (Template Method) | 算法骨架子类实现 |
| 访问者 (Visitor) | 操作与结构分离 |

## 运行结果

```
=== C++ 行为型设计模式演示 ===

1. Strategy:
QuickSort
1 2 5 8 9

2. Observer:
Observer1 received: Hello!
Observer2 received: Hello!

3. Command:
Light ON
Light OFF

4. Iterator:
10 20 30

5. Template Method:
Opening report.pdf
Extracting PDF data
Parsing PDF structure
Analyzing data...
Sending report

6. Visitor:
Circle area: 78.5397
Drawing circle r=5
Rectangle area: 24
Drawing rect 4x6
```
