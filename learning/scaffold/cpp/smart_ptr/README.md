# smart_ptr scaffold

C++ 智能指针演示——unique_ptr/shared_ptr/weak_ptr + 自定义删除器 + make_unique/make_shared。

## 复现命令

```bash
cd learning/scaffold/smart_ptr

g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
```

## 关键点

- **`unique_ptr<T>`**：独占所有权，不可拷贝只可移动，零开销
- **`shared_ptr<T>`**：共享所有权，引用计数，**循环引用会泄漏**
- **`weak_ptr<T>`**：不增加引用计数，必须 `lock()` 转 shared_ptr 才能访问
- **`make_unique` / `make_shared`**（C++14）：优先使用——异常安全 + 单次内存分配
- **自定义删除器**：`unique_ptr<T, Deleter>` ——Deleter 可以是函数指针/lambda/functor
- **`auto_ptr` 已废弃**：C++98 的设计缺陷，C++11 起 `unique_ptr` 取代

详见 NOTES.md 工程对照段。