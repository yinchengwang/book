# Lambda 表达式（C++）

## 概述

Lambda 表达式是 C++11 引入的一种匿名函数机制，允许在需要函数对象的任何地方内联定义函数。Lambda 提供了一种简洁的方式来创建函数对象，特别适合与 STL 算法配合使用。

## 基本语法

```cpp
[捕获列表] (参数列表) -> 返回类型 { 函数体 }
```

### 各部分说明

| 部分 | 必选 | 说明 |
|------|------|------|
| `[捕获列表]` | 否 | 指定如何捕获外部变量 |
| `(参数列表)` | 否 | 函数参数，与普通函数相同 |
| `-> 返回类型` | 否 | 显式指定返回类型 |
| `{函数体}` | 是 | 函数实现 |

## 捕获列表详解

| 捕获方式 | 语法 | 说明 |
|----------|------|------|
| 不捕获 | `[]` | 无法访问外部变量 |
| 按值捕获 | `[=]` | 捕获所有外部变量的副本 |
| 按引用捕获 | `[&]` | 捕获所有外部变量的引用 |
| 显式捕获 | `[x, &y]` | 按值捕获 x，按引用捕获 y |
| 初始化捕获 | `[z = expr]` | C++14+，捕获表达式结果 |

## 常用场景

### 1. STL 算法配合

```cpp
std::vector<int> nums = {1, 2, 3, 4, 5};

// for_each 遍历
std::for_each(nums.begin(), nums.end(), [](int n) {
    std::cout << n << " ";
});

// transform 转换
std::transform(nums.begin(), nums.end(), nums.begin(),
               [](int n) { return n * n; });

// find_if 查找
auto it = std::find_if(nums.begin(), nums.end(), [](int n) {
    return n > 3;
});
```

### 2. 即时回调

```cpp
auto callback = [](int x) { return x * 2; };
process(callback);
```

### 3. std::function 存储

```cpp
std::function<int(int)> func = [](int x) { return x + 1; };
```

## 泛型 Lambda（C++14）

```cpp
// 参数类型自动推导
auto print = [](auto value) {
    std::cout << value << std::endl;
};

print(42);      // int
print(3.14);    // double
```

## 注意事项

1. **捕获生命周期**：按引用捕获时，确保 lambda 执行时引用的对象仍然有效
2. **mutable**：按值捕获的 lambda 默认 const，需要 mutable 才能修改副本
3. **性能**：Lambda 通常比函数指针有更好的内联优化机会

## 编译

```bash
g++ -std=c++17 -Wall -Wextra -g -o test main.cpp
```

## 运行

```bash
./test
```
