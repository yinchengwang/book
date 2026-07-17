# C++20 协程

## 概述

C++20 引入了协程（Coroutine）支持，这是一种轻量级的并发原语，允许函数在执行过程中暂停和恢复。

## 核心概念

### 协程结构
- `co_await` — 暂停等待
- `co_yield` — 暂停并产出值（生成器）
- `co_return` — 暂停并返回值

### Awaitable 接口
实现三个成员函数即可自定义 Awaitable：
- `await_ready()` — 是否跳过等待
- `await_suspend(handle)` — 暂停逻辑
- `await_resume()` — 恢复后返回值

## 编译要求

```bash
g++ -std=c++20 -fcoroutines -o test main.cpp
```

注意：GCC 10+ 需要 `-fcoroutines` 标志。

## 相关资源

- cppreference: https://en.cpppreference.com/w/cpp/language/coroutines
- C++20 标准协程库
