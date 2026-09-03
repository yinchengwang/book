# 工程对照笔记

本文档记录 clang-tidy 在 `engineering/` 项目中的实际应用场景。

## 工程集成方式

`engineering/CMakeLists.txt` 中可通过以下方式集成 clang-tidy：

```cmake
# 方式一：CMake 内置支持
cmake -B build/engineering \
      -DCMAKE_CXX_CLANG_TIDY="clang-tidy;--config-file=.clang-tidy"

# 方式二：使用 LLVM 模块
find_package(LLVM REQUIRED)
include(AddClangTidy)
```

## engineering/src/db/ 模块分析

对 `engineering/src/db/` 的核心模块运行 clang-tidy 可发现以下典型问题：

### 1. bufmgr.c - Buffer Pool 管理器

```c
// 问题：hash_search 返回 NULL 后直接解引用
// 触发规则：bugprone-nullDerive, clang-analyzer-core.NullDereference
HashEntry* entry = hash_search(hash_table, key, HASH_FIND, NULL);
entry->refcount++;  // BUG: 如果 entry 为 NULL 则崩溃
```

**对应规则**：`bugprone-*` 检测空指针解引用

### 2. heapam.c - Heap Access Method

```c
// 问题：memcpy 拷贝非 POD 结构
// 触发规则：cppcoreguidelines-pro-type-reinterpret-cast
//          performance-no-int-to-ptr
typedef struct {
    int32_t xmin;
    int32_t xmax;
    String  data;  // 非 POD 类型！
} HeapTupleData;

HeapTuple dest;
memcpy(&dest, &src, sizeof(HeapTupleData));  // 错误！
```

**对应规则**：`cppcoreguidelines-pro-type-reinterpret-cast` 禁止 memcpy 非平凡类型

### 3. btreeam.c - BTree 索引

```c
// 问题：频繁的 malloc/free 循环
// 触发规则：cppcoreguidelines-owning-memory
//          modernize-use-unique-ptr
for (int i = 0; i < n; i++) {
    BTreePage* page = malloc(sizeof(BTreePage));
    process_page(page);
    free(page);  // 内存泄漏风险：process_page 异常时不释放
}
```

**对应规则**：`cppcoreguidelines-owning-memory` 推荐使用智能指针

### 4. wal.c - Write-Ahead Log

```c
// 问题：goto 语句（遗留代码）
// 触发规则：readability-braces-around-statements
//          cert-err33-c
if (error_code != 0) {
    goto cleanup;
}

// 大量清理代码...
cleanup:
    if (buf != NULL) {
        buffer_unpin(buf);
    }
    if (lock_held) {
        LWLockRelease(lock_id);
    }
```

**对应规则**：`readability-braces-around-statements` 建议消除 goto

### 5. catalog.c - 系统目录

```c
// 问题：整数溢出风险
// 触发规则：cert-err33-c
Oid next_oid = current_oid + 1;  // 可能溢出！
if (next_oid > MaxOid) {
    // 处理溢出
}

// 问题：按值传递大结构
// 触发规则：performance-unnecessary-value-param
void catalog_scan(CatalogEntry entry) {  // 应使用 const CatalogEntry*
    // ...
}
```

**对应规则**：`cert-err33-c` 检测整数溢出；`performance-*` 检测不必要的拷贝

## 性能相关规则应用

### performance-move-const-arg

对所有传值参数有优化建议。例如：

```c
// 问题代码
void process_data(std::vector<int> data) {  // 按值传递，触发拷贝
    // ...
}

// 建议修改为
void process_data(const std::vector<int>& data) {  // const 引用
    // 或者
void process_data(std::vector<int> data) {  // 调用方使用 std::move
    // ...
}

void caller() {
    std::vector<int> v;
    process_data(std::move(v));  // 显式移动
}
```

### performance-unnecessary-value-param

检测可优化为 const 引用的按值参数：

```cpp
// NOLINTNEXTLINE(performance-unnecessary-value-param)
void foo(std::string s) {  // 应改为 const std::string&
    // 未修改 s
}
```

## 现代化改造建议

通过 `modernize-*` 规则可系统性升级代码：

| 规则 | 当前代码 | 建议修改 |
|------|----------|----------|
| `modernize-use-auto` | `for (std::vector<int>::iterator it = v.begin(); ...)` | `for (auto it = v.begin(); ...)` |
| `modernize-use-nullptr` | `if (ptr == NULL)` | `if (ptr == nullptr)` |
| `modernize-use-emplace` | `v.push_back(Foo(args))` | `v.emplace_back(args)` |
| `modernize-use-unique-ptr` | `Foo* f = new Foo()` | `auto f = std::make_unique<Foo>()` |
| `modernize-range-for` | `for (int i = 0; i < n; i++)` | `for (const auto& item : v)` |

## CI/CD 集成建议

在持续集成中运行 clang-tidy：

```yaml
# .github/workflows/static-analysis.yml
- name: Run clang-tidy
  run: |
    find engineering/src -name "*.c" -o -name "*.cpp" | \
    xargs clang-tidy \
      --config-file=engineering/.clang-tidy \
      --warnings-as-errors='*' \
      --quiet
```

建议将 clang-tidy 集成到 pre-commit hook 或 PR 检查流程中。
