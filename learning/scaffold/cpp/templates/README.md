# C++ 模板元编程 (Template Metaprogramming)

## 概述

模板元编程是 C++ 中的一种高级编程技术，它利用 C++ 模板系统在**编译时**进行计算和代码生成。相比于运行时计算，模板元编程具有以下优势：

- **零运行时开销**：计算在编译时完成，运行时无额外成本
- **类型安全**：编译器在编译时进行严格类型检查
- **代码复用**：一套代码适用于多种类型

## 核心概念

### 1. 函数模板 (Function Template)

函数模板允许定义与类型无关的函数：

```cpp
template <typename T>
T max_value(T a, T b) {
    return (a > b) ? a : b;
}

// 编译器自动生成:
// int max_value(int a, int b);
// double max_value(double a, double b);
```

### 2. 类模板 (Class Template)

类模板用于创建泛型数据结构：

```cpp
template <typename T, size_t N = 100>
class Stack {
    T data_[N];
    size_t top_;
public:
    void push(const T& value);
    T pop();
};
```

### 3. 模板特化 (Template Specialization)

为特定类型提供定制实现：

```cpp
// 通用版本
template <typename T>
T max_value(T a, T b) { return (a > b) ? a : b; }

// char* 特化版本
template <>
const char* max_value<const char*>(const char* a, const char* b) {
    return (strcmp(a, b) > 0) ? a : b;
}
```

### 4. 可变参数模板 (Variadic Template)

处理任意数量的参数：

```cpp
template <typename... Args>
void print_all(Args... args) {
    (std::cout << ... << args);  // C++17 折叠表达式
}
```

### 5. SFINAE

"Substitution Failure Is Not An Error" — 替换失败不是错误。编译器在尝试模板重载时，如果某个重载的替换导致错误，会静默跳过该重载：

```cpp
// 仅当 T 是算术类型时才启用
template <typename T>
auto add(T a, T b) -> std::enable_if_t<std::is_arithmetic_v<T>, T> {
    return a + b;
}
```

### 6. constexpr if (C++17)

编译时条件分支，简化 SFINAE 的使用：

```cpp
template <typename T>
std::string inspect_type(T) {
    if constexpr (std::is_integral_v<T>) {
        return "整数";
    } else if constexpr (std::is_floating_point_v<T>) {
        return "浮点";
    } else {
        return "其他";
    }
}
```

## 应用场景

| 场景 | 示例 |
|------|------|
| 泛型数据结构 | vector, list, map 等容器 |
| 类型 traits | 检测类型能力（是否可迭代、是否有某成员） |
| 编译时计算 | 阶乘、斐波那契等数值计算 |
| 静态多态 | CRTP 模式实现零成本虚函数 |
| 代码生成 | 根据类型自动生成序列化/反序列化代码 |

## 学习路径

1. 掌握函数模板和类模板的基础用法
2. 理解模板参数推导规则
3. 学习模板特化（偏特化、全特化）
4. 理解 SFINAE 原理
5. 掌握可变参数模板
6. 学习 type_traits 库
7. 实践 constexpr if 和编译时计算

## 参考资源

- C++ 标准库 `<type_traits>`
- 《C++ Templates》第二版
- cppreference.com 模板文档
