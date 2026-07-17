# Pimpl 惯用法工程对照

## 数据库存储引擎中的 Pimpl

### Catalog 实现

```cpp
// engineering/include/db/catalog.h
// 使用 Pimpl 隐藏系统表实现
class Catalog {
public:
    Catalog(StorageEngine* storage);
    ~Catalog();

    Oid getTableOid(const char* name) const;
    TableDesc* getTableDesc(Oid oid) const;

private:
    struct CatalogData;  // 前向声明
    std::unique_ptr<CatalogData> data_;  // Pimpl
};
```

```cpp
// engineering/src/db/catalog.c
// 实现细节完全隐藏
struct Catalog::CatalogData {
    StorageEngine* storage_;
    std::unordered_map<std::string, Oid> nameToOid_;
    std::unordered_map<Oid, TableDesc*> oidToDesc_;
    // 所有私有成员和依赖都在这里
};
```

### Buffer Pool 实现

```cpp
// engineering/include/db/buf.h
class BufferPool {
public:
    BufferPool(size_t size);
    ~BufferPool();

    Page* getPage(PageId id);
    void unpinPage(PageId id);

private:
    struct BufferPoolImpl;
    std::unique_ptr<BufferPoolImpl> impl_;
};
```

## Pimpl 的优势

### 1. 编译防火墙

```
不使用 Pimpl:
┌──────────────┐
│ widget.h     │
│ - vector<T>  │
│ - map<K,V>   │
└──────┬───────┘
       │ 依赖
       ▼
┌──────────────┐
│ user1.cpp    │ ── 重编译
├──────────────┤
│ user2.cpp    │ ── 重编译
├──────────────┤
│ user3.cpp    │ ── 重编译
└──────────────┘

使用 Pimpl:
┌──────────────┐
│ widget.h     │
│ - Impl*      │ ← 只需要前向声明
└──────┬───────┘
       │ 无依赖
       ▼
┌──────────────┐
│ user1.cpp    │ ← 不重编译
├──────────────┤
│ user2.cpp    │ ← 不重编译
├──────────────┤
│ user3.cpp    │ ← 不重编译
└──────────────┘
```

### 2. ABI 稳定

库的实现可以自由改变，不影响用户：

```cpp
// v1
struct Widget::Impl {
    int counter_;
};

// v2（可以完全重写）
struct Widget::Impl {
    std::vector<int> history_;
    size_t current_;
    // 新的数据结构
};

// 用户代码不需要改变
```

### 3. 减少头文件依赖

```cpp
// 之前：widget.h 暴露所有依赖
#include <vector>
#include <map>
#include <string>
class Widget {
    std::vector<int> data_;  // 用户需要包含 vector
    std::map<int, std::string> lookup_;
};

// 之后：widget.h 简洁
class Widget {
    struct Impl;  // 只前向声明
    std::unique_ptr<Impl> pImpl_;
};
```

## Pimpl 注意事项

1. **必须定义析构函数**：因为 unique_ptr 需要完整类型
2. **需要处理移动语义**：Rule of Five
3. **有轻微性能开销**：额外的指针间接访问
4. **内存分配**：impl 对象需要额外分配

## Pimpl vs 私有嵌套类

| 方式 | 优点 | 缺点 |
|------|------|------|
| Pimpl | 完全编译隔离、ABI 稳定 | 额外堆分配 |
| 嵌套类 | 无额外分配 | 编译依赖仍存在 |

## 何时使用 Pimpl

| 场景 | 推荐 |
|------|------|
| 公共 API / 库 | ✓ 必须 |
| 频繁修改的实现 | ✓ 减少重编译 |
| 依赖复杂第三方库 | ✓ 隐藏依赖 |
| 内部实现 | 可选 |
| 简单类 | ✗ 过度设计 |
