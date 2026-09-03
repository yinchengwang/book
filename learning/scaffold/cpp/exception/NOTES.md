# exception 学习笔记

## 概念地图

C++ 异常处理是 RAII 哲学的"另一半"——通过 try/catch + 栈展开实现"错误传播 + 自动清理"：

- **异常三段式**：
  - `try { /* 可能抛异常的代码 */ }`
  - `throw exception_obj;` —— 抛出（值/指针/引用都可）
  - `catch (const T& e) { /* 处理 */ }` —— 按类型匹配
- **异常类型层次**：
  - `std::exception` ——基类，有 `virtual const char* what() const`
  - `std::runtime_error` ——运行时错误（文件打开失败、网络断开）
  - `std::logic_error` ——逻辑错误（参数越界、空指针）
  - `std::out_of_range` / `std::invalid_argument` 等子类
- **栈展开（stack unwinding）**：
  - 异常抛出后，从 throw 点向上回溯
  - 每个栈帧的局部对象按"反构造顺序"析构
  - RAII 资源（智能指针、lock_guard）在析构中自动清理
- **`noexcept` 关键字**：
  - `void foo() noexcept { ... }` ——承诺不抛，编译器可优化
  - 真抛则 `std::terminate()` 终止
  - 析构函数默认 `noexcept`
- **异常安全三级别**：
  - **No-throw 保证**：`noexcept` 或 swap
  - **Strong 保证**：失败时回滚到调用前状态（copy-and-swap）
  - **Basic 保证**：不泄漏资源，对象处于有效但未定义状态

## 踩坑记录

1. **catch 顺序错误**：子类异常必须在父类之前 catch，否则父类先匹配
2. **异常抛出指针**：指针所指对象必须在 catch 时仍存在——**抛值不用指针**
3. **析构函数抛异常**：C++11 起析构默认 `noexcept`——抛则 `std::terminate`
4. **构造函数抛异常**：对象未构造完成，不调析构——成员变量的析构已发生
5. **`catch(...)` 不知道类型**：必须重新抛出或用 `throw` 转换类型
6. **异常开销**：每个 try/catch 块让二进制大 5-10%；嵌入式/高频代码禁用异常
7. **跨语言边界**：C 函数不能抛 C++ 异常（无 RTTI）；`extern "C"` 函数不能用异常

## 工程对照（≥100 字硬约束）

C++ 异常处理在 `engineering/` 中体现为"C vs C++ 错误哲学"的对比：

1. **`engineering/src/db/core/errors.c` 错误码体系**：纯 C 风格——所有函数返回 `int` 错误码 + `errno` 设置。`elog(ERROR, ...)` 报告错误。这是"返回码 + 全局状态"哲学
2. **C++ 等价物**：抛 `std::runtime_error("page read failed: errno=" + std::to_string(errno))` ——异常对象自带消息
3. **`engineering/rag/src/rag/persist/hnsw_persist.c` 错误返回**：每个函数返回 `int`，调用方必须检查。`fopen` 失败 `return -1`。C++ 改写：`if (!fp) throw std::runtime_error("hnsw_persist: cannot open file " + path);`
4. **`engineering/src/db/api/vector_api.c` HTTP 错误处理**：把内部错误码映射为 HTTP 状态码。C++ 改写可用 `std::variant` 或异常 + catch in handler
5. **`engineering/src/db/storage/page/disk.c` 资源清理**：所有 fd 必须手动 `close(fd)`。C++ 用 RAII：`std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(path, "rb"), fclose);`
6. **`engineering/src/db/core/db_server.c` 服务器关闭**：用 `atexit` 注册清理函数（LIFO）。C++ 用 RAII 析构 + 智能指针自动清理
7. **`engineering/src/db/consensus/raft.c` 错误传播**：选举失败 / 网络断开返回错误码 + 日志。C++ 改写：`throw NetworkError("raft: peer unreachable")`，上层 catch 后回退 candidate 状态

学完本卡能动手的事：把 `learning/scaffold/error_handling/main.c` 中 `safe_divide` 改写为 C++ 函数 + 抛 `std::invalid_argument` 异常 + `noexcept` 标记，对比两种错误哲学的代码量与可读性。