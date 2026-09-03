# C++ CRTP 模式

> 奇特的递归模板模式 (Curiously Recurring Template Pattern)

## 编译

```bash
g++ -std=c++17 -o main main.cpp
./main
```

## 什么是 CRTP

CRTP 是一种编译期多态技术，通过模板继承实现静态分发：

```cpp
template<typename Derived>
class Base {
    void interface() {
        static_cast<Derived*>(this)->implementation();
    }
};

class Derived : public Base<Derived> {
    void implementation() { /* ... */ }
};
```

## 与虚函数对比

| 特性 | CRTP | 虚函数 |
|------|------|--------|
| 绑定时机 | 编译时 | 运行时 |
| 性能 | 无虚表开销，可内联 | 虚表查找 |
| 内联 | 可以 | 通常不能 |
| 灵活性 | 编译时固定 | 运行时多态 |
| 编译时间 | 较长 | 较短 |

## 运行结果

```
=== CRTP 演示 ===

1. Static Polymorphism:
Base::commonOperation called
Derived1::implementation
Base::commonOperation called
Derived2::implementation

2. Object Counting:
Object count: 3

3. Type-Safe Cloning:
Document: Hello
Spreadsheet: 10x5
Document: Hello

4. Method Chaining:
Line 1
No newline continued
Line 1
```
