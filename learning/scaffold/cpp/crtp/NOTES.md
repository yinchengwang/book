# CRTP 模式工程对照

## 在高性能库中的应用

### std::enable_shared_from_this

C++ 标准库的 CRTP 应用：

```cpp
// C++ 标准库实现
template<class T>
class enable_shared_from_this {
    std::weak_ptr<T> weak_this;
public:
    shared_ptr<T> shared_from_this() {
        return shared_ptr<T>(weak_this);
    }
};
```

### Boost.Iterator

Boost.Iterator 库使用 CRTP 实现自定义迭代器：

```cpp
// 类似 boost::iterator_adaptor
template<class Derived, class Base>
class iterator_adaptor : public Base {
    Derived& derived() { return static_cast<Derived&>(*this); }
};
```

## 在项目中的应用场景

### 1. 对象池引用计数

```cpp
// engineering/src/db/bufmgr.c
// 可以用 CRTP 实现对象池的通用计数逻辑

template<typename T>
class RefCounted {
    static int refCount;
public:
    void addRef() { ++refCount; }
    void release() { if (--refCount == 0) delete static_cast<T*>(this); }
};
```

### 2. 日志记录器

```cpp
template<typename Derived>
class Logger {
    void log(Level level, const char* msg) {
        // 统一格式化
        // 调用派生类的实际输出
    }
};

class FileLogger : public Logger<FileLogger> {
    void write(const std::string& msg) { /* 写入文件 */ }
};
```

### 3. 性能关键代码

对于数据库内部的热路径，CRTP 可以减少虚函数调用开销：

```cpp
// Buffer Pool 页面操作
template<typename PageType>
class PageAccessor {
    void read(PageId id, char* buf) {
        static_cast<PageType*>(this)->readImpl(id, buf);
    }
};
```

## CRTP 使用场景

| 场景 | 是否使用 CRTP |
|------|---------------|
| 性能关键路径 | ✓ |
| 类型集合固定 | ✓ |
| 需要编译期多态 | ✓ |
| 插件/运行时加载 | ✗ |
| 接口需要运行时切换 | ✗ |
| 需要跨 ABI 边界 | ✗ |

## 注意事项

1. **派生类必须在基类之前完整定义**
2. **不支持虚析构函数的多态删除**
3. **增加编译时间**
4. **错误信息可能复杂**
