# 查询或操作引擎

## 学习目标

- 理解 LevelDB 查询和操作执行流程
- 掌握 Iterator 设计和 Merge 迭代器的工作方式
- 了解 Snapshot 和 MVCC 的实现
- 对比 LevelDB 操作引擎与项目 algo/ 模块的关联

## 操作执行流程

### 写入操作 (Put)

```mermaid
sequenceDiagram
    participant C as Client
    participant DB as DBImpl
    participant WB as WriteBatch
    participant W as WAL
    participant M as MemTable
    participant BG as 后台线程
    participant SS as 存储系统

    C->>DB: Put(key, value)
    DB->>WB: 序列化操作
    WB->>DB: 获取序列号
    DB->>W: 追加 WAL
    W-->>DB: 持久化确认
    DB->>M: 插入 SkipList
    M-->>DB: 插入成功
    DB-->>C: 返回 OK

    Note over BG: 后台 Compaction 周期
    BG->>SS: 读取 SSTable
    BG->>BG: 合并排序+去重
    BG->>SS: 写入新 SSTable
    BG->>DB: 更新 Version
```

### 删除操作 (Delete)

```mermaid
sequenceDiagram
    participant C as Client
    participant DB as DBImpl
    participant M as MemTable

    C->>DB: Delete(key)
    DB->>M: 插入删除标记<br/>(kTypeDeletion)
    M-->>DB: 插入成功
    DB-->>C: 返回 OK

    Note over DB: 删除标记实际是插入一条<br/>带有 kTypeDeletion 类型的记录
    Note over DB: Compaction 时才会真正删除
```

### 读取操作 (Get)

```mermaid
flowchart TD
    G["Get(key)"] --> SN{"有 Snapshot?"}
    SN -->|否| CS["使用当前序列号"]
    SN -->|是| SS["使用快照序列号"]

    CS --> M1{"MemTable?"}
    M1 -->|命中| R1["返回 Value"]
    M1 -->|未命中| IM1{"Immutable?"}
    IM1 -->|命中| R1
    IM1 -->|未命中| L0["遍历 Level 0<br/>(从新到旧)"]

    L0 --> BF["Bloom Filter 检查"]
    BF -->|排除| NEXT["下一个文件"]
    BF -->|可能命中| INDEX["读取 Index Block"]
    INDEX --> BLOCK["读取 Data Block"]
    BLOCK --> FOUND{"Key 匹配?"}
    FOUND -->|是| R1
    FOUND -->|否| NEXT

    NEXT --> L0_END{"所有 L0 文件<br/>遍历完?"}
    L0_END -->|否| L0
    L0_END -->|是| L1["Level 1+ 查找"]

    L1 --> L1_BF["Bloom Filter 检查"]
    L1_BF --> L1_INDEX["二分查找 Index Block"]
    L1_INDEX --> L1_BLOCK["读取 Data Block"]
    L1_BLOCK --> L1_FOUND{"Key 匹配?"}
    L1_FOUND -->|是| R1
    L1_FOUND -->|否| NF["返回 NotFound"]
```

### 范围扫描 (Iterator)

```mermaid
flowchart TB
    subgraph "Iterator 创建"
        NEW["NewIterator()"] --> MERGE["创建 Merge 迭代器"]
        MERGE --> M1["MemTable 迭代器"]
        MERGE --> M2["Immutable 迭代器"]
        MERGE --> M3["Level 0 迭代器"]
        MERGE --> M4["Level 1 迭代器"]
        MERGE --> M5["Level 2-6 迭代器"]
    end

    subgraph "Iterator 操作"
        SEEK["Seek(key)"] --> POS["定位到 >= key 的位置"]
        NEXT["Next()"] --> MIN["从所有子迭代器中<br/>取最小 Key"]
        PREV["Prev()"] --> MAX["从所有子迭代器中<br/>取最大 Key"]
    end

    subgraph "Merge 迭代器"
        CHILD["子迭代器 1"] --> HEAP["最小堆<br/>按 Key 排序"]
        CHILD2["子迭代器 2"] --> HEAP
        CHILD3["子迭代器 3"] --> HEAP
        HEAP --> POP["弹出最小 Key"]
        POP --> DEDUP["去重<br/>(保留最新版本)"]
        DEDUP --> OUT["输出结果"]
    end
```

## 核心算法和数据结构

### Iterator 设计

LevelDB 的 Iterator 是一致的数据访问接口，隐藏了底层存储细节。

```cpp
// include/leveldb/iterator.h
class Iterator {
 public:
  // 定位
  virtual void SeekToFirst() = 0;   // 到第一个
  virtual void SeekToLast() = 0;    // 到最后一个
  virtual void Seek(const Slice& target) = 0;  // 定位到 >= target

  // 遍历
  virtual void Next() = 0;          // 下一个
  virtual void Prev() = 0;          // 上一个

  // 访问
  virtual bool Valid() const = 0;   // 是否有效
  virtual Slice key() const = 0;    // 当前 Key
  virtual Slice value() const = 0;  // 当前 Value

  // 错误
  virtual Status status() const = 0;
};
```

### 五种 Iterator 实现

| 实现 | 数据来源 | 说明 |
|------|---------|------|
| MemTable Iterator | SkipList | 遍历内存表 |
| Table Iterator | SSTable | 遍历磁盘文件 |
| Level Iterator | Level 层 | 遍历一层所有文件 |
| TwoLevel Iterator | 索引+数据 | 分层遍历 |
| Merging Iterator | 多路合并 | 合并多个子迭代器 |

### Merge 迭代器 (MergingIterator)

```mermaid
graph TB
    subgraph "MergingIterator"
        HEAP["最小堆<br/>按 Key 排序"]
        CHILD1["子迭代器 1<br/>MemTable"]
        CHILD2["子迭代器 2<br/>Immutable"]
        CHILD3["子迭代器 3<br/>Level 0"]
        CHILD4["子迭代器 4<br/>Level 1-6"]
    end

    subgraph "合并过程"
        R1["记录 1: a=1"] --> HEAP
        R2["记录 2: a=2"] --> HEAP
        R3["记录 3: a=1"] --> HEAP
        R4["记录 4: a=1"] --> HEAP
        HEAP --> SORTED["排序后: a=1, a=1, a=1, a=2"]
        SORTED --> DEDUP["去重后: a=1, a=2"]
    end
```

**Merge 迭代器算法**：

```cpp
// db/db_iter.cc
void MergingIterator::Next() {
    // 1. 从当前最小子迭代器前进
    current_->Next();

    // 2. 重新插入最小堆
    // 3. 从堆顶取出新的最小 Key
    // 4. 去重：跳过相同 Key 的旧版本
    while (heap_.top()->key() == current_->key()) {
        heap_.top()->Next();
        heap_.adjust();
    }
}
```

### Compaction 中的操作

```mermaid
flowchart TD
    PC["Pick Compaction<br/>选择压缩任务"] --> READ["读取输入文件"]
    READ --> MAKE_ITER["创建 Merge 迭代器"]
    MAKE_ITER --> SORT["排序合并"]

    SORT --> LOOP{"Iter Valid?"}
    LOOP -->|是| DROP{"当前 Key<br/>可以丢弃?"}
    DROP -->|是| SKIP["跳过"]
    DROP -->|否| WRITE["写入新 SSTable"]
    WRITE --> CHECK{"文件大小<br/>超限?"}
    CHECK -->|是| FINISH["完成当前文件"]
    FINISH --> NEW_FILE["创建新文件"]
    NEW_FILE --> LOOP
    CHECK -->|否| LOOP

    LOOP -->|否| INSTALL["安装 Compaction 结果"]
    INSTALL --> UPDATE_VER["更新 Version"]
    UPDATE_VER --> DELETE_OBS["删除旧文件"]
```

**丢弃策略**：

```
丢弃条件（满足任一即可）：
1. Key 已被删除（kTypeDeletion 标记）
2. 同一 Key 有更新版本，且旧版本在更低层不再需要
3. 低于用户指定的 Snapshot 序列号
```

### Snapshot 与 MVCC

```mermaid
sequenceDiagram
    participant C1 as Client 1
    participant C2 as Client 2
    participant DB as DBImpl
    participant SS as SnapshotList

    C1->>DB: Put("k", "v1")
    DB-->>C1: OK

    C2->>DB: GetSnapshot()
    DB->>SS: 记录当前序列号=10
    SS-->>DB: Snapshot 句柄
    DB-->>C2: Snapshot

    C1->>DB: Put("k", "v2")
    DB-->>C1: OK

    C2->>DB: Get(options.snapshot, "k")
    DB->>DB: 使用序列号 10 查找
    DB-->>C2: "v1"  <-- 读取的是旧版本

    C2->>DB: ReleaseSnapshot()
    DB->>SS: 移除快照
```

**Snapshot 实现**：

```cpp
// db/db_impl.cc
class SnapshotList {
    // 双向链表
    Snapshot list_;          // 哨兵节点
    int refs_;               // 引用计数

    Snapshot* New(SequenceNumber seq) {
        Snapshot* s = new Snapshot;
        s->number_ = seq;    // 记录当前序列号
        // 插入链表头部
        Insert(s);
        return s;
    }

    void Delete(const Snapshot* s) {
        // 引用计数递减，为 0 时移除
        if (--s->refs_ == 0) {
            Remove(s);
            delete s;
        }
    }
};
```

### WriteBatch 实现

```cpp
// db/write_batch.cc
// 将多个操作打包为原子操作
//
// 内部格式:
// +----------------+----------------+----------------+----------------+
// | sequence(8B)   | count(4B)      | record 1       | record 2       |
// +----------------+----------------+----------------+----------------+
//
// record = type(1B) + key_len(4B) + key(NB) + val_len(4B) + val(NB)

class WriteBatch {
 public:
  void Put(const Slice& key, const Slice& value);
  void Delete(const Slice& key);

  // 原子提交
  Status Write(const WriteOptions& options, DB* db);

  // 遍历操作记录
  class Handler {
   public:
    virtual void Put(const Slice& key, const Slice& value) = 0;
    virtual void Delete(const Slice& key) = 0;
  };

 private:
  std::string rep_;  // 序列化缓冲区
};
```

## 与项目 algo/ 模块的关联

### 算法关联

LevelDB 中使用的核心算法在项目 algo/ 模块中都有对应实现：

| 算法 | LevelDB 使用 | 项目对应模块 |
|------|-------------|-------------|
| **SkipList** | MemTable 实现 | `engineering/src/index/` |
| **Bloom Filter** | SSTable 快速过滤 | `engineering/src/algo/` |
| **LRU Cache** | Block Cache | `engineering/src/db/core/` |
| **二分查找** | Index Block 定位 | `engineering/src/algo/` |
| **排序合并** | Compaction 合并 | `engineering/src/algo/` |
| **最小堆** | Merge 迭代器优先队列 | `engineering/src/algo/` |
| **Hash 表** | LRU Cache 查找 | `engineering/src/self_made/` |

### 排序合并算法 (Compaction 核心)

```mermaid
graph TB
    subgraph "输入: 有序序列"
        A["序列 A<br/>L0 文件"] --> C["比较排序"]
        B["序列 B<br/>L1 文件"] --> C
    end

    subgraph "合并过程"
        C --> D{"a < b?"}
        D -->|是| OUT1["输出 a"]
        OUT1 --> ADV_A["a 前进"]
        ADV_A --> C
        D -->|否| OUT2["输出 b"]
        OUT2 --> ADV_B["b 前进"]
        ADV_B --> C
    end

    subgraph "输出: 有序序列"
        OUT["新 SSTable<br/>有序无重叠"]
    end
```

**Compaction 合并代码**：

```cpp
// db/version_set.cc
Status VersionSet::DoCompactionWork(CompactionState* compact) {
    // 创建合并迭代器
    Iterator* input = MakeInputIterator(compact);

    // 逐条处理
    for (; input->Valid(); input->Next()) {
        const Slice& key = input->key();
        const Slice& value = input->value();

        // 1. 解析序列号
        SequenceNumber seq = ExtractSequenceNumber(key);

        // 2. 检查是否可以丢弃
        bool drop = false;
        if (compact->smallest_snapshot > seq) {
            // 序列号小于最小快照，可以丢弃
            drop = true;
            if (input->value().type() == kTypeDeletion) {
                // 删除标记在 Compaction 中真正删除
                drop = true;
            }
        }

        // 3. 去重（相同 Key 保留最新版本）
        if (key == last_key) {
            drop = true;
        }

        // 4. 写入新 SSTable
        if (!drop) {
            builder->Add(key, value);
        }

        // 5. 文件大小限制
        if (builder->FileSize() >= kTargetFileSize) {
            FinishFile(builder, compact);
            builder = NewBuilder();
        }
    }
}
```

## 操作性能分析

### 写入性能

| 因素 | 影响 | 说明 |
|------|------|------|
| WAL fdatasync | 每次写入 1 次 fsync | 最大开销 |
| MemTable 插入 | O(log n) SkipList | 内存操作，快 |
| MemTable 满 | 冻结 + 后台 Compaction | 可能阻塞写入 |
| 批量写入 | 合并 WAL 写入 | 吞吐提升 10x+ |

### 读取性能

| 因素 | 影响 | 说明 |
|------|------|------|
| Bloom Filter | 过滤 90% 无效文件 | 减少磁盘 I/O |
| Block Cache | 缓存热点 Block | 减少磁盘读取 |
| 层级深度 | 最多 7 层 | 读放大可能 |
| Level 0 重叠 | 最多 4 个文件 | 逐文件查找 |

### 读放大问题

```
读放大 = 实际读取的磁盘数据量 / 返回的数据量

场景 1: 点查 Key 在 L1
- 1 次 Bloom Filter 检查
- 1 次 Index Block 读取
- 1 次 Data Block 读取
- 读放大: ~2x

场景 2: 点查 Key 不在 L0
- 需要检查 4 个 L0 文件的 Bloom Filter
- 可能需要读取 4 次 Index Block
- 读放大: ~8x

场景 3: 范围扫描
- 可能需要合并多个层级
- 读放大: 10x~100x
```

## 要点总结

- **操作流程**：Put/Get/Delete 都通过 WriteBatch 序列化，保证原子性
- **Iterator 系统**：一致的数据访问接口，支持 Merge 迭代器多路合并
- **Snapshot 机制**：基于序列号 MVCC，读操作不阻塞写操作
- **Compaction 操作**：后台合并排序 + 去重，是 LSM-Tree 的核心维护操作
- **性能权衡**：写入吞吐高但读放大不可避免，Bloom Filter 缓解读放大
- **与项目关联**：SkipList、Bloom Filter、排序合并算法与项目 algo/ 模块对应

## 思考题

1. Merge 迭代器如何保证多个子迭代器的 Key 排序正确？如果子迭代器有相同 Key 怎么处理？
2. LevelDB 的删除为什么是插入删除标记而不是立即删除？这有什么好处和坏处？
3. 如果项目中实现 LSM-Tree 引擎，Compaction 的合并排序算法能否复用现有 algo/ 模块的排序实现？
4. Snapshot 的序列号机制如何工作？为什么 Compaction 不能丢弃 Snapshot 引用的版本？