# 类型擦除工程对照

## std::function 的应用

### SQL 执行器回调

```cpp
// engineering/include/db/sql_exec.h
// 回调函数使用 std::function 统一接口

using RowCallback = std::function<void(const Row&)>;

class SQLExecutor {
public:
    void execute(const char* sql, RowCallback callback) {
        // 执行查询，回调每一行
        while (fetchRow()) {
            callback(currentRow_);
        }
    }
};
```

### 存储引擎钩子

```cpp
// WAL 提交完成回调
using CommitCallback = std::function<void(TxnId)>;

class WALManager {
public:
    void onCommit(TxnId id, CommitCallback cb) {
        commitCallbacks_.push_back(std::move(cb));
    }
};
```

## std::any 的应用

### 配置系统

```cpp
// engineering/include/db/guc.h
// GUC 参数使用 std::any 存储不同类型

class GUCParameter {
public:
    template<typename T>
    void set(T value) {
        value_ = std::any(value);
    }

    template<typename T>
    T get() const {
        return std::any_cast<T>(value_);
    }

private:
    std::any value_;
};

// 使用
GUCParameter param;
param.set<int>(128);        // shared_buffers
param.set<std::string>("replica");  // wal_level
```

### 事件系统

```cpp
// 通用事件携带任意数据
class Event {
public:
    template<typename T>
    void setData(T&& data) {
        data_ = std::forward<T>(data);
    }

    template<typename T>
    T getData() const {
        return std::any_cast<T>(data_);
    }

private:
    std::any data_;
};
```

## 类型擦除的实现模式

### Concept + Model 模式

```cpp
// 概念：定义接口
class ISerializer {
public:
    virtual ~ISerializer() = default;
    virtual void serialize(std::ostream&) const = 0;
    virtual std::string name() const = 0;
};

// 模型：实现具体类型
template<typename T>
class TypedSerializer : public ISerializer {
public:
    explicit TypedSerializer(const T& value) : value_(value) {}
    void serialize(std::ostream& os) const override {
        os << value_;
    }
    std::string name() const override { return typeid(T).name(); }

private:
    T value_;
};

// 类型擦除的包装器
class AnySerializer {
public:
    template<typename T>
    AnySerializer(T value) : impl_(std::make_unique<TypedSerializer<T>>(value)) {}

    void serialize(std::ostream& os) const { impl_->serialize(os); }

private:
    std::unique_ptr<ISerializer> impl_;
};
```

## 性能考虑

| 操作 | 开销 |
|------|------|
| std::function 调用 | 虚函数调用 + 可能的对象拷贝 |
| std::any_cast | 类型检查 + 值拷贝 |
| 小型函数优化 | 编译器可能消除开销 |

## 何时使用类型擦除

| 场景 | 推荐 |
|------|------|
| 需要存储不同类型 | ✓ std::any |
| 需要统一调用接口 | ✓ std::function |
| 需要类型安全联合 | ✓ std::variant |
| 性能关键路径 | ✗ 考虑模板或虚函数 |
| 编译期多态足够 | ✗ 考虑 CRTP |
