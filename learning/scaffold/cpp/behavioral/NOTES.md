# 行为型模式工程对照

## 策略模式 (Strategy)

数据库排序策略使用策略模式：

```cpp
// engineering/src/algo/sort.c
// 不同排序算法作为策略可选
enum SortType { QUICK_SORT, MERGE_SORT, HEAP_SORT };

void sort_strategy(vector<int>& data, SortType type) {
    switch (type) {
        case QUICK_SORT: quick_sort(data); break;
        case MERGE_SORT: merge_sort(data); break;
        case HEAP_SORT: heap_sort(data); break;
    }
}
```

## 观察者模式 (Observer)

事件驱动系统使用观察者模式：

```cpp
// 类似 PostgreSQL 的 NOTIFY/LISTEN
class EventBus {
    vector<EventListener*> listeners_;
    void publish(const Event& e) {
        for (auto* l : listeners_) l->onEvent(e);
    }
};
```

## 命令模式 (Command)

WAL 日志系统是命令模式的天然应用：

```cpp
// engineering/src/db/wal.c
// 每个日志记录是一个命令
class WALCommand {
    virtual void redo() = 0;
    virtual void undo() = 0;
};

class InsertCommand : public WALCommand {
    // 记录插入操作
};
```

## 迭代器模式 (Iterator)

STL 容器的迭代器本身就是迭代器模式：

```cpp
// C++ 标准库
std::vector<int> v = {1, 2, 3};
for (auto it = v.begin(); it != v.end(); ++it) {
    // 使用 *it
}
```

## 模板方法模式 (Template Method)

存储引擎扫描接口使用模板方法：

```cpp
// engineering/include/db/rel.h
class TableScan {
public:
    void scan() {
        open();
        while (next()) {
            processRow();
        }
        close();
    }

protected:
    virtual void open() = 0;
    virtual bool next() = 0;
    virtual void processRow() = 0;
    virtual void close() = 0;
};
```

## 访问者模式 (Visitor)

AST 遍历使用访问者模式：

```cpp
// 类似编译器前端的 AST 节点遍历
class ASTVisitor {
    virtual void visit(NumberExpr*) = 0;
    virtual void visit(BinaryExpr*) = 0;
    virtual void visit(CallExpr*) = 0;
};
```

## 模式选择指南

| 场景 | 推荐模式 |
|------|----------|
| 算法可切换 | 策略 |
| 事件通知 | 观察者 |
| 操作记录/撤销 | 命令 |
| 集合遍历 | 迭代器 |
| 算法骨架固定 | 模板方法 |
| 数据结构与操作分离 | 访问者 |
