# raii 学习笔记

## 概念地图

RAII（Resource Acquisition Is Initialization）是 C++ 区别于 C 的核心理念——**用对象生命周期管理资源**：

- **RAII 三定律**：
  1. 构造时获取资源（open file / lock mutex / malloc memory）
  2. 析构时释放资源（close file / unlock mutex / free memory）
  3. 资源由对象拥有，**生命周期与作用域绑定**
- **标准库 RAII 类**：
  - 文件：`std::ifstream`/`std::ofstream`/`std::fstream`
  - 锁：`std::lock_guard`/`std::unique_lock`/`std::scoped_lock`
  - 内存：`std::unique_ptr`/`std::shared_ptr`/`std::weak_ptr`
  - 线程：`std::thread`（析构时若 `joinable()` 则 `std::terminate`）
  - 锁存：`std::lock_guard` 包装 `std::mutex`/`std::recursive_mutex`
- **自定义 RAII 模式**：
  ```cpp
  class ResourceGuard {
  public:
      ResourceGuard() { /* 获取 */ }
      ~ResourceGuard() { /* 释放 */ }
      ResourceGuard(const ResourceGuard&) = delete;     // 禁止拷贝
      ResourceGuard& operator=(const ResourceGuard&) = delete;
  };
  ```
- **异常安全保证三级别**：
  - **Basic**：不泄漏资源，对象处于有效但未定义状态
  - **Strong**：操作失败回滚到调用前状态（copy-and-swap）
  - **No-throw**：永不抛异常（`noexcept` 或 swap）

## 踩坑记录

1. **忘记禁止拷贝**：锁/文件句柄被拷贝 → 双重释放。**`= delete` 拷贝构造 + 拷贝赋值**
2. **`unique_ptr` 隐式转换 `shared_ptr`**：构造开销大。**仅在需要共享所有权时用**
3. **析构函数抛异常**：栈展开中析构抛异常 → `std::terminate`
4. **RAII 类的赋值**：要么禁止（`= delete`），要么实现深拷贝/移动语义（Rule of Five）
5. **资源泄漏在循环**：`for (...) { Resource r; ... }` 每次循环正确释放
6. **`std::lock_guard` vs `std::unique_lock`**：前者简单，后者支持延迟锁定/手动 unlock/condition variable
7. **`auto_ptr` 已废弃**：C++98 的 `auto_ptr` 有缺陷，C++11 起用 `unique_ptr`/`shared_ptr`

## 工程对照（≥100 字硬约束）

RAII 在 `engineering/` 中体现为"手动资源管理 vs C++ 自动管理"的对比：

1. **`engineering/src/db/storage/page/disk.c` 的 fd 管理**：纯 C 风格——`int fd = open(path, O_RDONLY); if (fd < 0) return -1; ... close(fd);`。**每个错误路径必须 close**
2. **C++ 等价物**：`std::unique_ptr<int, decltype(&close)> fd_guard(open(path, O_RDONLY), close);` —— 任何路径（包括异常）都自动 close
3. **`engineering/src/db/storage/page/page.c` 的 page cache**：C 风格用 `Page* pages[N];` 数组 + 手动 `free(pages[i])`。**错误路径容易泄漏**
4. **`engineering/src/db/core/db_server.c` 的连接管理**：`int client_fd = accept(...); ... close(client_fd);` —— 多个错误返回点必须关闭
5. **`engineering/rag/src/rag/persist/hnsw_persist.c` 的 fopen/fclose**：6 个 `fopen` 站点必须对应 6 个 `fclose`，分散在函数多出口——极易遗漏
6. **`engineering/src/db/storage/bufmgr.c` Buffer Pool**：纯 C 函数表 `get_page/set_page`，C++ 改写可用 `std::lock_guard<std::mutex>` 自动释放 page lock
7. **`engineering/src/db/storage/wal/wal.c` WAL 文件管理**：长生命周期 fd + 短生命周期 page，写入失败时需回滚 + 关闭 fd。C++ RAII 让 `WALFileGuard` 析构时自动 flush + close

学完本卡能动手的事：把 `learning/scaffold/file_io/main.c` 的文件操作改写为 C++ 版本，用 `std::ofstream`/`std::ifstream` 替代 `fopen`/`fclose`，观察代码量减少。