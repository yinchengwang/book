# 创建型模式工程对照

## 单例模式 (Singleton)

项目中数据库连接管理使用单例模式确保全局只有一个连接实例：

```cpp
// engineering/src/db/core/db_server.c
// C 语言版本使用 pthread_once 实现线程安全单例
static pthread_once_t db_once = PTHREAD_ONCE_INIT;
static kv_t *g_db_instance = NULL;

kv_t* db_get_instance() {
    pthread_once(&db_once, db_init);
    return g_db_instance;
}
```

C++ 重构版本可使用 Meyer's Singleton 更简洁：

```cpp
class Database {
public:
    static Database& getInstance() {
        static Database instance;
        return instance;
    }
};
```

## 工厂模式 (Factory Method)

存储引擎使用工厂模式创建不同的访问方法：

```cpp
// engineering/include/db/storage_engine.h
enum class AccessMethodType { Heap, BTree, Hash, Vector };

class StorageEngineFactory {
public:
    static std::unique_ptr<AccessMethod> create(AccessMethodType type);
};
```

## 抽象工厂模式 (Abstract Factory)

RAG 模块使用抽象工厂创建不同的向量索引：

```cpp
// engineering/rag/index/vector_index_factory.h
class VectorIndexFactory {
public:
    static std::unique_ptr<VectorIndex> create(IndexType type);
};
```

## 建造者模式 (Builder)

SQL 解析器可用建造者模式构建 AST：

```cpp
// 类似 pg_query 的 QueryBuilder
auto query = QueryBuilder()
    .select({"id", "name"})
    .from("users")
    .where("age > 18")
    .orderBy("name")
    .build();
```

## 原型模式 (Prototype)

Buffer Pool 中页面复用可使用原型模式：

```cpp
// 页面预分配时克隆原型
class PagePrototype {
    Page clone() const { return Page(*this); }
};
```

## 模式选择指南

| 场景 | 推荐模式 |
|------|----------|
| 全局唯一资源 | 单例 |
| 对象创建逻辑复杂 | 工厂 |
| 产品族一致性 | 抽象工厂 |
| 对象构建步骤多 | 建造者 |
| 对象创建成本高 | 原型 |
