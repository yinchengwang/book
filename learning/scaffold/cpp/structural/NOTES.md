# 结构型模式工程对照

## 桥接模式 (Bridge)

数据库存储引擎使用桥接模式分离访问方法与缓存：

```cpp
// engineering/include/db/rel.h
// 抽象层：Relation 接口
class Relation {
public:
    virtual ~Relation() = default;
    virtual void open() = 0;
    virtual void close() = 0;
    virtual ScanResult scan(...) = 0;
protected:
    Relation(BufferPool& buf) : bufferPool_(buf) {}  // 桥接到实现
    BufferPool& bufferPool_;
};

// engineering/include/db/heapam.h
// 实现层：HeapAccessMethod
class HeapAccessMethod : public Relation {
    // 具体实现
};
```

## 装饰器模式 (Decorator)

RAG 模块中向量索引可使用装饰器增强：

```cpp
// engineering/rag/index/vector_index.h
class VectorIndex {
public:
    virtual SearchResult search(...) = 0;
    virtual void add(...) = 0;
};

// 缓存装饰器
template<typename Index>
class CachedVectorIndex : public VectorIndex {
    // 添加 LRU 缓存层
};
```

## 组合模式 (Composite)

文件系统或配置树使用组合模式：

```cpp
// 类似目录结构的树形组合
class ConfigNode {
    vector<unique_ptr<ConfigNode>> children_;
    void traverse() {
        for (auto& child : children_) child->traverse();
    }
};
```

## 适配器模式 (Adapter)

日志系统适配不同输出：

```cpp
// engineering/include/db/logger.h
class Logger {
    virtual void log(Level, const char*) = 0;
};

class FileLoggerAdapter : public Logger {
    // 适配 C 文件 API
};
```

## 代理模式 (Proxy)

Buffer Pool 本身就是一种代理模式：

```cpp
// engineering/src/db/bufmgr.c
// BufferPool 代理底层磁盘 I/O
// 提供缓存、脏页标记、置换策略
```

## 外观模式 (Facade)

数据库初始化使用外观封装复杂流程：

```cpp
// engineering/src/db/core/initdb.c
// initdb 封装了：
// - 目录创建
// - 系统表初始化
// - 配置文件生成
// - WAL 初始化
```

## 模式选择指南

| 场景 | 推荐模式 |
|------|----------|
| 接口不兼容 | 适配器 |
| 多维度变化 | 桥接 |
| 树形结构 | 组合 |
| 动态增强 | 装饰器 |
| 简化复杂系统 | 外观 |
| 延迟加载/访问控制 | 代理 |
