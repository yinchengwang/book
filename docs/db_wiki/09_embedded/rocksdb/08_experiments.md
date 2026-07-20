# RocksDB 动手实验

## 学习目标

- 掌握 RocksDB 的 C++ 编译和使用
- 动手实现 CRUD、列族操作、事务
- 了解性能基准测试方法

## 环境准备

### 安装依赖

```bash
# Ubuntu
sudo apt-get install -y build-essential cmake \
    liblz4-dev libzstd-dev libsnappy-dev \
    libbz2-dev zlib1g-dev

# macOS
brew install cmake lz4 zstd snappy bzip2 zlib
```

### 编译 RocksDB

```bash
# 下载源码
git clone https://github.com/facebook/rocksdb.git
cd rocksdb

# 编译
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DWITH_SNAPPY=ON \
         -DWITH_LZ4=ON \
         -DWITH_ZSTD=ON
make -j8

# 安装（可选）
sudo make install
```

### 项目集成

```cmake
# CMakeLists.txt
find_package(RocksDB REQUIRED)

target_link_libraries(your_target
    rocksdb
    lz4
    zstd
    snappy
    z
    bz2
)
```

## 基础 CRUD

### 创建和打开数据库

```cpp
#include <rocksdb/db.h>
#include <iostream>

int main() {
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    
    rocksdb::Status s = rocksdb::DB::Open(options, "/tmp/rocksdb_test", &db);
    if (!s.ok()) {
        std::cerr << "Open failed: " << s.ToString() << std::endl;
        return 1;
    }
    
    std::cout << "RocksDB database opened" << std::endl;
    
    // 使用数据库
    // ...
    
    delete db;
    return 0;
}
```

### Put / Get / Delete

```cpp
// 写入
rocksdb::Status s = db->Put(rocksdb::WriteOptions(), "key1", "value1");
if (!s.ok()) {
    std::cerr << "Put failed: " << s.ToString() << std::endl;
}

// 读取
std::string value;
s = db->Get(rocksdb::ReadOptions(), "key1", &value);
if (s.ok()) {
    std::cout << "Read: key1 = " << value << std::endl;
} else if (s.IsNotFound()) {
    std::cout << "Key not found" << std::endl;
}

// 删除
s = db->Delete(rocksdb::WriteOptions(), "key1");
if (!s.ok()) {
    std::cerr << "Delete failed: " << s.ToString() << std::endl;
}
```

### 完整 CRUD 示例

```cpp
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <iostream>

int main() {
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    
    rocksdb::Status s = rocksdb::DB::Open(options, "/tmp/rocksdb_demo", &db);
    if (!s.ok()) {
        std::cerr << "Open failed: " << s.ToString() << std::endl;
        return 1;
    }
    
    // 批量写入
    for (int i = 0; i < 100; i++) {
        std::string key = "key-" + std::to_string(i);
        std::string value = "value-" + std::to_string(i);
        db->Put(rocksdb::WriteOptions(), key, value);
    }
    std::cout << "Inserted 100 key-value pairs" << std::endl;
    
    // 遍历所有 key
    rocksdb::Iterator* it = db->NewIterator(rocksdb::ReadOptions());
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
    it = db->NewIterator(rocksdb::ReadOptions());
    for (it->Seek("key-50"); it->Valid() && it->key().ToString() < "key-60"; it->Next()) {
        std::cout << "  " << it->key().ToString()
                  << " => " << it->value().ToString() << std::endl;
    }
    delete it;
    
    delete db;
    return 0;
}
```

## 列族操作

### 创建和使用列族

```cpp
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <vector>

int main() {
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    
    // 打开数据库，创建列族
    std::vector<rocksdb::ColumnFamilyDescriptor> column_families;
    column_families.push_back(rocksdb::ColumnFamilyDescriptor(
        rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions()));
    column_families.push_back(rocksdb::ColumnFamilyDescriptor(
        "metadata", rocksdb::ColumnFamilyOptions()));
    column_families.push_back(rocksdb::ColumnFamilyDescriptor(
        "data", rocksdb::ColumnFamilyOptions()));
    
    std::vector<rocksdb::ColumnFamilyHandle*> handles;
    rocksdb::Status s = rocksdb::DB::Open(rocksdb::DBOptions(),
                                          "/tmp/rocksdb_cf",
                                          column_families,
                                          &handles,
                                          &db);
    if (!s.ok()) {
        std::cerr << "Open failed: " << s.ToString() << std::endl;
        return 1;
    }
    
    // 使用不同列族写入
    db->Put(rocksdb::WriteOptions(), handles[1], "key1", "metadata_value");
    db->Put(rocksdb::WriteOptions(), handles[2], "key1", "data_value");
    
    // 从不同列族读取
    std::string value;
    db->Get(rocksdb::ReadOptions(), handles[1], "key1", &value);
    std::cout << "metadata: " << value << std::endl;
    
    db->Get(rocksdb::ReadOptions(), handles[2], "key1", &value);
    std::cout << "data: " << value << std::endl;
    
    // 清理
    for (auto h : handles) {
        delete h;
    }
    delete db;
    return 0;
}
```

### 动态创建列族

```cpp
rocksdb::ColumnFamilyHandle* cf;
s = db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), "new_cf", &cf);
if (s.ok()) {
    db->Put(rocksdb::WriteOptions(), cf, "key", "value");
    delete cf;
}

// 删除列族
s = db->DropColumnFamily(cf);
```

## 事务操作

### TransactionDB 使用

```cpp
#include <rocksdb/utilities/transaction_db.h>

int main() {
    rocksdb::TransactionDB* txn_db;
    rocksdb::Options options;
    options.create_if_missing = true;
    
    rocksdb::TransactionDBOptions txn_options;
    rocksdb::Status s = rocksdb::TransactionDB::Open(
        options, txn_options, "/tmp/rocksdb_txn", &txn_db);
    if (!s.ok()) {
        std::cerr << "Open failed: " << s.ToString() << std::endl;
        return 1;
    }
    
    // 开始事务
    rocksdb::Transaction* txn = txn_db->BeginTransaction(rocksdb::WriteOptions());
    
    // 事务操作
    txn->Put("key1", "value1");
    txn->Put("key2", "value2");
    
    // 提交
    s = txn->Commit();
    if (!s.ok()) {
        std::cerr << "Commit failed: " << s.ToString() << std::endl;
        txn->Rollback();
    }
    
    delete txn;
    delete txn_db;
    return 0;
}
```

### 乐观事务

```cpp
// 乐观并发控制
rocksdb::TransactionOptions txn_options;
txn_options.lock_timeout = 1000000;  // 1 秒

rocksdb::Transaction* txn = txn_db->BeginTransaction(
    rocksdb::WriteOptions(), txn_options);

// 读取-修改-写入
std::string value;
txn->GetForUpdate(rocksdb::ReadOptions(), "counter", &value);
int count = std::stoi(value);
count++;
txn->Put("counter", std::to_string(count));

s = txn->Commit();
if (s.IsBusy()) {
    std::cout << "Conflict detected, retry needed" << std::endl;
    txn->Rollback();
}

delete txn;
```

## 性能基准测试

### 使用 db_bench

```bash
# 编译 db_bench
cd rocksdb/build
make db_bench

# 写入测试
./db_bench --benchmarks=fillseq --db=/tmp/bench --num=1000000

# 随机写入测试
./db_bench --benchmarks=fillrandom --db=/tmp/bench --num=1000000

# 读取测试
./db_bench --benchmarks=readrandom --db=/tmp/bench --num=1000000

# 范围查询测试
./db_bench --benchmarks=seekrandom --db=/tmp/bench --num=1000000
```

### 自定义基准测试

```cpp
#include <chrono>
#include <random>

void Benchmark(rocksdb::DB* db, int num_ops) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1000000);
    
    // 写入测试
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_ops; i++) {
        std::string key = "key-" + std::to_string(dis(gen));
        std::string value = "value-" + std::to_string(i);
        db->Put(rocksdb::WriteOptions(), key, value);
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
        db->Get(rocksdb::ReadOptions(), key, &value);
    }
    end = std::chrono::high_resolution_clock::now();
    auto read_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Read " << num_ops << " ops in " << read_time.count()
              << " ms (" << num_ops * 1000.0 / read_time.count() << " ops/s)" << std::endl;
}
```

### 批量写入对比

```cpp
void BatchBenchmark(rocksdb::DB* db, int batch_size) {
    auto start = std::chrono::high_resolution_clock::now();
    
    rocksdb::WriteBatch batch;
    for (int i = 0; i < batch_size; i++) {
        batch.Put("batch-key-" + std::to_string(i), "value");
    }
    db->Write(rocksdb::WriteOptions(), &batch);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Batch write " << batch_size << " ops in " << time.count()
              << " us (" << batch_size * 1000000.0 / time.count() << " ops/s)" << std::endl;
}
```

## 监控和统计

### 获取统计信息

```cpp
// 获取数据库统计
std::string stats;
db->GetProperty("rocksdb.stats", &stats);
std::cout << stats << std::endl;

// 获取内存使用
uint64_t memtable_usage;
db->GetIntProperty("rocksdb.cur-size-all-mem-tables", &memtable_usage);
std::cout << "MemTable usage: " << memtable_usage << " bytes" << std::endl;

// 获取 Block Cache 命中率
uint64_t block_cache_hit, block_cache_miss;
db->GetIntProperty("rocksdb.block-cache-hit", &block_cache_hit);
db->GetIntProperty("rocksdb.block-cache-miss", &block_cache_miss);
std::cout << "Block Cache hit rate: "
          << (double)block_cache_hit / (block_cache_hit + block_cache_miss)
          << std::endl;
```

## 要点总结

- **编译**：CMake 编译，需要压缩库依赖
- **CRUD**：`Put` 写入，`Get` 读取，`Delete` 删除
- **列族**：`CreateColumnFamily` 创建，指定列族操作
- **事务**：`TransactionDB` + `BeginTransaction`
- **基准测试**：`db_bench` 工具 + 自定义测试

## 思考题

1. 列族之间如何共享 WAL？如何保证跨列族事务原子性？
2. db_bench 的 fillseq 和 fillrandom 性能差异有多大？为什么？
3. 如何监控 RocksDB 的 Compaction 状态？