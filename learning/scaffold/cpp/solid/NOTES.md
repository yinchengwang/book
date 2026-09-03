# SOLID 原则工程对照

## SRP (单一职责原则)

数据库存储引擎按职责分离：

```cpp
// engineering/src/db/
// bufmgr.c - Buffer Pool 职责
// wal.c - WAL 日志职责
// disk.c - 磁盘 I/O 职责
// catalog.c - 目录管理职责

// 每个模块职责单一，便于维护和测试
```

## OCP (开闭原则)

访问方法使用抽象基类：

```cpp
// engineering/include/db/rel.h
class Relation {
public:
    virtual void open() = 0;
    virtual void close() = 0;
    virtual ScanResult scan(...) = 0;
    // 添加新功能只需扩展，不需要修改现有代码
};
```

## LSP (里氏替换原则)

Buffer 页面替换策略：

```cpp
// engineering/src/db/bufmgr.c
// Clock-Sweep 和 LRU 可以互换
// 因为它们实现了相同的抽象接口

typedef struct BufferReplacementStrategy {
    void (*evict)(BufferPool*);
    bool (*can_evict)(BufferPool*, PageId);
} BufferReplacementStrategy;
```

## ISP (接口隔离原则)

文件 I/O 接口分离：

```cpp
// engineering/include/db/disk.h
// 分离读写接口
class IReader {
    virtual size_t read(PageId, char* buf) = 0;
};

class IWriter {
    virtual size_t write(PageId, const char* buf) = 0;
};

// 具体实现可以只实现需要的接口
```

## DIP (依赖倒置原则)

SQL 执行器依赖抽象接口：

```cpp
// engineering/include/db/sql_exec.h
// 执行器不直接依赖具体存储引擎
// 而是依赖抽象的 Access Method 接口

class SQLExecutor {
public:
    SQLExecutor(AccessMethod& am) : am_(am) {}

private:
    AccessMethod& am_;  // 依赖抽象
};
```

## 违反 SOLID 的代价

| 违反原则 | 问题 | 后果 |
|----------|------|------|
| SRP | 胖类 | 修改影响范围大，难测试 |
| OCP | 硬编码 | 添加功能必须改旧代码 |
| LSP | 子类行为不一致 | 运行时错误 |
| ISP | 胖接口 | 实现不需要的功能 |
| DIP | 直接依赖 | 无法替换实现 |

## 应用建议

1. **代码审查**：检查每个类的职责是否单一
2. **接口设计**：优先使用小接口
3. **依赖注入**：通过构造函数注入依赖
4. **测试驱动**：先写测试再写实现
