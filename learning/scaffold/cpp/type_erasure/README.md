# C++ 类型擦除

> std::function + std::any + 类型擦除原理

## 编译

```bash
g++ -std=c++17 -o main main.cpp
./main
```

## 类型擦除原理

类型擦除通过**概念（Concept）**和**实现（Model）**分离类型：

```
std::function<void()>
        │
        ├── Concept: 可调用对象接口
        │            ├── void call()
        │
        └── Models:
             ├── LambdaModel<...>
             ├── FunctionPointerModel
             └── FunctorModel<...>
```

## 核心类型

| 类型 | 说明 |
|------|------|
| std::function | 函数对象的类型擦除 |
| std::any | 任意类型的容器 |
| std::variant | 类型安全的联合体 |

## 运行结果

```
=== 类型擦除演示 ===

1. std::function:
Lambda called
Function called
Functor called
Inline lambda

2. Custom Type Erasure:
Af1 called
Functor called

3. std::any:
int: 42
string: hello
double: 3.14

4. Heterogeneous Container:
int: 10
string: world
float: 2.5

5. Event System:
Event 1
Event 2

6. Tradeoffs:
=== 类型擦除的权衡 ===
...
```
