# raii scaffold

C++ RAII（Resource Acquisition Is Initialization）演示——ifstream/lock_guard/unique_ptr + 自定义 RAII + 异常安全。

## 复现命令

```bash
cd learning/scaffold/raii

g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
```

## 关键点

- **RAII 核心**：资源获取即初始化——构造时获取资源，析构时释放
- **三大 RAII 类型**：
  - `std::ifstream` / `std::ofstream` —— 文件自动关闭
  - `std::lock_guard` / `std::unique_lock` —— 互斥锁自动释放
  - `std::unique_ptr` / `std::shared_ptr` —— 堆内存自动 delete
- **自定义 RAII**：包装资源到 class，构造获取、析构释放
- **异常安全**：栈展开时所有栈上 RAII 对象的析构被调用 → **资源不泄漏**
- **禁止拷贝**：`MyLock(const MyLock&) = delete;` ——防止"重复释放"
- **`std::move` 转移所有权**：`unique_ptr` 不可拷贝，只能 move

详见 NOTES.md 工程对照段。