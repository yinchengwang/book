# C++ 表达式模板

> 惰性求值、矩阵运算、表达式树

## 编译

```bash
g++ -std=c++17 -o main main.cpp
./main
```

## 什么是表达式模板

表达式模板是一种模板元编程技术，将表达式建模为类型：

```cpp
// 传统方式：创建临时对象
Vec v3 = v1 + v2;  // v1+v2 创建临时，拷贝到 v3

// 表达式模板：惰性求值
auto result = v1 + v2;  // 只存储表达式
v3 = result;  // 直接计算，无临时
```

## 表达式模板模式

```
VecAdd<Vec<int,3>, Vec<int,3>>
        │
        ├── lhs_: Vec<int,3>
        ├── rhs_: Vec<int,3>
        └── operator[](i): return lhs_[i] + rhs_[i]
```

## 运行结果

```
=== 表达式模板演示 ===

1. Lazy Evaluation:
Scalar[0] = 5
Scalar[100] = 5

2. Vector Expressions:
v1 + v2 = 5 7 9
v1 + v2 * 2 = 9 12 15
v3 = v1 + v2 + {1,1,1} = 6 8 10

3. Matrix Expressions:
m1 + m2 =
  6 8
  10 12

4. Performance:
=== 表达式模板性能优势 ===
...
```
