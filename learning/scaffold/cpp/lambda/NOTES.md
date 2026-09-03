# Lambda 表达式工程对照笔记

## 概述

本文档对比学习中的 Lambda 表达式与 `engineering/src/db/storage/` 中的回调函数模式，分析两者在设计理念上的共通之处与差异。

## 回调函数模式（C 语言风格）

在 `engineering/src/db/storage/` 中，大量使用回调函数模式来处理存储引擎的各类操作。以 `bufmgr.c`（Buffer Pool 管理器）为例：

```c
// engineering/src/db/storage/bufmgr.c 中的典型回调模式

// 1. 回调函数类型定义
typedef bool (*page_iterator_callback)(PageHeader page, void *arg);

// 2. 接受回调的函数签名
void buffer_pool_scan(BufferPool *pool,
                      page_iterator_callback callback,
                      void *arg);

// 3. 使用回调的场景
bool check_dirty(PageHeader page, void *arg) {
    return page->is_dirty;
}

buffer_pool_scan(pool, check_dirty, NULL);
```

### 特点分析

| 特性 | C 回调模式 | Lambda 表达式 |
|------|-----------|---------------|
| **状态传递** | 通过 `void* arg` 参数 | 通过捕获列表 |
| **类型安全** | 依赖函数指针，编译期检查弱 | 模板化，类型推导强 |
| **语法重量** | 需要单独定义函数 | 内联定义，轻量 |
| **使用场景** | 固定 API 接口（C 风格库） | 现代 C++ 代码 |

## 工程实践对比

### 1. 状态管理的对比

**C 回调方式（engineering/src/db/storage/）**：
```c
// 状态通过参数传递
void scan_with_max_count(BufferPool *pool,
                         int max_count,  // 状态参数
                         page_iterator_callback callback) {
    int count = 0;
    // 需要手动维护计数状态
    // ...
}
```

**Lambda 方式（学习示例）**：
```cpp
// 状态通过捕获隐式传递
int max_count = 100;
auto check = [max_count](PageHeader page) {
    return ++count <= max_count;  // 状态封装在 lambda 内部
};
```

### 2. 多态行为的对比

**C 回调方式**：
```c
// 使用函数指针实现多态
typedef void (*action_t)(void *data);

void execute_action(action_t action, void *data) {
    action(data);
}

// 不同的回调实现
void insert_action(void *data) { /* 插入逻辑 */ }
void delete_action(void *data) { /* 删除逻辑 */ }
```

**Lambda 方式**：
```cpp
// 使用 std::function 实现运行时多态
std::function<void()> action;

if (condition) {
    action = []() { /* 插入逻辑 */ };
} else {
    action = []() { /* 删除逻辑 */ };
}

action();
```

## 设计模式映射

| 设计模式 | C 回调实现 | Lambda 实现 |
|----------|-----------|-------------|
| **策略模式** | 函数指针 | `std::function` + Lambda |
| **访问器模式** | 遍历 + 回调 | STL 算法 + Lambda |
| **命令模式** | 函数指针 | Lambda 闭包 |

## 在 storage 模块中的应用场景

假设 `engineering/src/db/storage/` 要迁移到 C++，以下场景适合使用 Lambda：

1. **页面遍历**：
   ```cpp
   buffer_pool.for_each_page([](Page& page) {
       if (page.is_dirty()) {
           page.flush();
       }
   });
   ```

2. **条件查找**：
   ```cpp
   auto it = buffer_pool.find_if([](const Page& page) {
       return page.txn_id() > oldest_active_txn;
   });
   ```

3. **数据转换**：
   ```cpp
   std::vector<uint64_t> page_ids;
   buffer_pool.transform_pages(std::back_inserter(page_ids),
       [](const Page& page) { return page.id(); });
   ```

## 总结

Lambda 表达式是 C 回调函数模式的**现代化演进**：

- **优势**：语法简洁、类型安全、状态封装自然
- **局限**：需要 C++ 运行时，编译产物可能略大
- **共通**：核心思想一致——将行为作为一等公民（First-class citizen）传递

工程代码选择 C 回调还是 Lambda，应根据**目标平台**和**代码风格**决定。现代 C++ 项目推荐优先使用 Lambda，C 库或嵌入式场景仍适用回调模式。
