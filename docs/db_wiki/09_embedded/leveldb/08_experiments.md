# LevelDB 动手实验

## 学习目标

- 掌握 LevelDB 的 C++ 编译和使用
- 动手实现 CRUD 操作和批量写入
- 观察 Compaction 行为

## 环境准备

### 安装依赖

```bash
# Ubuntu
sudo apt-get install -y build-essential cmake snappy

# macOS
brew install cmake snappy
```

### 编译 LevelDB

```bash
# 下载源码
git clone https://github.com/google/leveldb.git
cd leveldb

# 编译
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# 安装（可选）
sudo make install
```

### 项目集成

```cmake
# CMakeLists.txt
add_subdirectory(leveldb EXCLUDE_FROM_ALL)

target_link_libraries(your_target
    leveldb
    snappy
)
```

## 基础 CRUD

### 创建和打开数据库

```cpp
#include <leveldb/db.h>
#include <iostream>

int main() {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    
    leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
    if (!status.ok()) {
        std::cerr << "Open failed: " << status.ToString() << std::endl;
        return 1;
    }
    
    std::cout << "Database opened" << std::endl;
    
    // 使用数据库
    // ...
    
    delete db;
    return 0;
}
```

### Put / Get / Delete

```cpp
// 写入
leveldb::Status s = db->Put(leveldb::WriteOptions(), "key1", "value1");
if (!s.ok()) {
    std::cerr << "Put failed: " << s.ToString() << std::endl;
}

// 读取
std::string value;
s = db->Get(leveldb::ReadOptions(), "key1", &value);
if (s.ok()) {
    std::cout << "Read: key1 = " << value << std::endl;
} else {
    std::cout << "Key not found" << std::endl;
}

// 删除
s = db->Delete(leveldb::WriteOptions(), "key1");
if (!s.ok()) {
    std::cerr << "Delete failed: " << s.ToString() << std::endl;
}
```

### 完整 CRUD 示例

```cpp
#include <leveldb/db.h>
#include <iostream>

int main() {
    // 打开数据库
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    options.compression = leveldb::kSnappyCompression;
    
    leveldb::Status status = leveldb::DB::Open(options, "/tmp/leveldb_demo", &db);
    if (!status.ok()) {
        std::cerr << "Open failed: " << status.ToString() << std::endl;
        return 1;
    }
    
    // 批量写入
    for (int i = 0; i < 100; i++) {
        std::string key = "key-" + std::to_string(i);
        std::string value = "value-" + std::to_string(i);
        db->Put(leveldb::WriteOptions(), key, value);
    }
    std::cout << "Inserted 100 key-value pairs" << std::endl;
    
    // 遍历所有 key
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    int count = 0;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::cout << "  " << it->key().ToString()
                  << " => " << it->value().ToString() << std::endl;
        count++;
    }
    delete it;
    std::cout << "Total keys: " << count << std::endl;
    
    // 范围查询
    std::cout << "\nRange query [key-50, key-60):" << std::endl;
    it = db->NewIterator(leveldb::ReadOptions());
    for (it->Seek("key-50"); it->Valid() && it->key().ToString() < "key-60"; it->Next()) {
        std::cout << "  " << it->key().ToString()
                  << " => " << it->value().ToString() << std::endl;
    }
    delete it;
    
    // 删除
    db->Delete(leveldb::WriteOptions(), "key-50");
    std::cout << "\nDeleted key-50" << std::endl;
    
    delete db;
    return 0;
}
```

## 批量写入

### WriteBatch 使用

```cpp
#include <leveldb/write_batch.h>

// 批量写入
void BatchWrite(leveldb::DB* db, int n) {
    leveldb::WriteBatch batch;
    
    for (int i = 0; i < n; i++) {
        std::string key = "batch-key-" + std::to_string(i);
        std::string value = "value-" + std::to_string(i);
        batch.Put(key, value);
    }
    
    leveldb::Status s = db->Write(leveldb::WriteOptions(), &batch);
    if (!s.ok()) {
        std::cerr << "BatchWrite failed: " << s.ToString() << std::endl;
    }
}
```

### 批量更新

```cpp
// 原子更新多个 key
void AtomicUpdate(leveldb::DB* db,
                   const std::string& from_key,
                   const std::string& to_key) {
    leveldb::WriteBatch batch;
    
    // 读取旧值
    std::string value;
    db->Get(leveldb::ReadOptions(), from_key, &value);
    
    // 删除旧 key，写入新 key
    batch.Delete(from_key);
    batch.Put(to_key, value);
    
    db->Write(leveldb::WriteOptions(), &batch);
}
```

## Snapshot 使用

### 创建和使用快照

```cpp
#include <leveldb/snapshot.h>

void SnapshotDemo(leveldb::DB* db) {
    // 写入初始数据
    db->Put(leveldb::WriteOptions(), "key", "value1");
    
    // 创建快照
    const leveldb::Snapshot* snapshot = db->GetSnapshot();
    
    // 更新数据
    db->Put(leveldb::WriteOptions(), "key", "value2");
    
    // 读取最新数据
    std::string current_value;
    db->Get(leveldb::ReadOptions(), "key", &current_value);
    std::cout << "Current: " << current_value << std::endl;  // value2
    
    // 读取快照数据
    leveldb::ReadOptions snapshot_options;
    snapshot_options.snapshot = snapshot;
    std::string snapshot_value;
    db->Get(snapshot_options, "key", &snapshot_value);
    std::cout << "Snapshot: " << snapshot_value << std::endl;  // value1
    
    // 释放快照
    db->ReleaseSnapshot(snapshot);
}
```

## Compaction 观察

### 监控 Compaction

```cpp
#include <leveldb/db.h>
#include <leveldb/env.h>

class CompactionListener : public leveldb::EventListener {
 public:
  void OnCompactionCompleted(
      leveldb::DB* db,
      const leveldb::CompactionInfo& info) override {
    std::cout << "Compaction completed:\n"
              << "  Input: L" << info.level << " " << info.input_files.size()
              << " files\n"
              << "  Output: L" << info.output_level << " "
              << info.output_files.size() << " files\n"
              << "  Time: " << info.elapsed_millis << " ms\n";
  }
};

// 注册监听器
leveldb::Options options;
options.listeners.push_back(std::make_shared<CompactionListener>());
```

### 触发 Compaction

```cpp
// 手动触发 Compaction
void TriggerCompaction(leveldb::DB* db) {
    // LevelDB 会自动触发，也可以手动调用
    db->CompactRange(nullptr, nullptr);  // 压缩全部范围
}

// 压缩特定范围
void CompactRange(leveldb::DB* db,
                  const std::string& start,
                  const std::string& end) {
    leveldb::Slice start_slice(start);
    leveldb::Slice end_slice(end);
    db->CompactRange(&start_slice, &end_slice);
}
```

## 性能基准测试

### 简单 Benchmark

```cpp
#include <chrono>
#include <random>

void Benchmark(leveldb::DB* db, int num_ops) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1000000);
    
    // 写入测试
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_ops; i++) {
        std::string key = "key-" + std::to_string(dis(gen));
        std::string value = "value-" + std::to_string(i);
        db->Put(leveldb::WriteOptions(), key, value);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto write_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Write " << num_ops << " ops in " << write_time.count()
              << " ms (" << num_ops * 1000.0 / write_time.count() << " ops/s)" << std::endl;
    
    // 读取测试
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_ops; i++) {
        std::string key = "key-" + std::to_string(dis(gen));
        std::string value;
        db->Get(leveldb::ReadOptions(), key, &value);
    }
    end = std::chrono::high_resolution_clock::now();
    auto read_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Read " << num_ops << " ops in " << read_time.count()
              << " ms (" << num_ops * 1000.0 / read_time.count() << " ops/s)" << std::endl;
}
```

### 批量写入对比

```cpp
void BatchBenchmark(leveldb::DB* db, int batch_size) {
    auto start = std::chrono::high_resolution_clock::now();
    
    leveldb::WriteBatch batch;
    for (int i = 0; i < batch_size; i++) {
        batch.Put("batch-key-" + std::to_string(i), "value");
    }
    db->Write(leveldb::WriteOptions(), &batch);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Batch write " << batch_size << " ops in " << time.count()
              << " us (" << batch_size * 1000000.0 / time.count() << " ops/s)" << std::endl;
}
```

## 要点总结

- **编译**：CMake 编译，依赖 Snappy
- **CRUD**：`Put` 写入，`Get` 读取，`Delete` 删除
- **批量写入**：`WriteBatch` 减少锁和 WAL 开销
- **快照**：`GetSnapshot` 创建，`ReleaseSnapshot` 释放
- **Compaction**：自动触发，可手动调用 `CompactRange`

## 思考题

1. 单条写入和批量写入的性能差异有多大？如何测量？
2. Snapshot 长时间不释放会占用什么资源？
3. 如何观察 LevelDB 的 Compaction 行为？