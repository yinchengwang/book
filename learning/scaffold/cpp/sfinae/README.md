# SFINAE 与 Concepts

## 概述

SFINAE（Substitution Failure Is Not An Error）和 Concepts 是 C++ 泛型编程的核心约束机制。

## 核心概念

### SFINAE 原则
- 模板参数替换失败时不报错
- 编译器尝试其他重载
- `std::enable_if_t` 是常用工具

### void_t 技巧
- 将任意类型映射为 void
- 用于检测表达式有效性
- `std::void_t<decltype(expr)>`

### Concepts（C++20）
```cpp
template<typename T>
concept Container = requires(T t) {
    t.begin();
    t.end();
    typename T::value_type;
};
```

### requires 表达式（C++20）
```cpp
template<typename T>
requires std::integral<T>
T foo(T t) { ... }
```

## 编译要求

```bash
g++ -std=c++17 -o test main.cpp
```

注意：C++20 Concepts 需要 `-std=c++20`。

## 相关资源

- cppreference: https://en.cppreference.com/w/cpp/language/sfinae
- C++20 Concepts: https://en.cppreference.com/w/cpp/concepts
