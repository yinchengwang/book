# C++ Pimpl 惯用法

> Pointer to Implementation（指向实现的指针）

## 编译

```bash
g++ -std=c++17 -o main main.cpp
./main
```

## 什么是 Pimpl

Pimpl 是一种编译防火墙技术，将实现细节藏在指针后面：

```cpp
class Widget {
public:
    Widget();
    ~Widget();

private:
    struct Impl;              // 前向声明
    std::unique_ptr<Impl> pImpl_;  // 指向实现的指针
};
```

## Pimpl 模式

```
┌─────────────────────────────────┐
│         Widget (头文件)          │
│  - 公开接口                      │
│  - struct Impl; // 前向声明      │
│  - unique_ptr<Impl> pImpl_;     │
└───────────────┬─────────────────┘
                │
                ▼
┌─────────────────────────────────┐
│       Widget::Impl (源文件)      │
│  - 所有私有成员                  │
│  - 所有依赖（vector/map/...）   │
│  - 实现代码                      │
└─────────────────────────────────┘
```

## 运行结果

```
=== Pimpl 惯用法演示 ===

1. Basic Pimpl:
Widget data: 42

2. Implementation Hiding:
Impl details are hidden from header file
Changing Impl doesn't require recompiling users

3. ABI Stability:
LibraryClass::publicMethod, state=1
LibraryClass::publicMethod, state=2

4. Factory + Pimpl:
MySQL: SELECT * FROM users
PostgreSQL: SELECT * FROM users

5. Pimpl Benefits:
  - 编译防火墙：减少编译依赖
  - ABI 稳定：实现可自由变化
  - 头文件简洁：只暴露必要接口
  - 加速编译：改变实现不影响其他文件
```
