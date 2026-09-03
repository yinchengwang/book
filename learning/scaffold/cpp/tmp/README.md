# 模板元编程（Template Metaprogramming）

## 概述

模板元编程（TMP）是一种利用 C++ 模板系统在编译期进行计算和类型操作的编程技术。

## 核心概念

### 类型计算
- `TypeList<Ts...>` — 类型列表
- `TypeAt<N, T...>` — 获取第 N 个类型
- 编译期递归展开

### 值计算
- `Fibonacci<N>` — 编译期斐波那契
- `GCD<a, b>` — 编译期最大公约数
- `Power<base, exp>` — 编译期幂运算

### constexpr if (C++17)
- 编译期条件分支
- 消除未使用的分支

### 可变参数模板
- `template<typename... Args>`
- 递归展开参数包

## 编译要求

```bash
g++ -std=c++17 -O2 -o test main.cpp
```

## 相关资源

- cppreference: https://en.cppreference.com/w/cpp/language/templates
- 《C++ Templates》第二版
