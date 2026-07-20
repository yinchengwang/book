# Badger 动手实验

## 学习目标

- 掌握 Badger 的 Go 环境搭建
- 动手实现 CRUD 操作和事务
- 了解性能基准测试方法

## 环境准备

### 安装 Go

```bash
# 下载 Go 1.21+
wget https://go.dev/dl/go1.21.5.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.21.5.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin

# 验证
go version
```

### 创建项目

```bash
mkdir badger-demo && cd badger-demo
go mod init badger-demo
go get github.com/dgraph-io/badger/v4
```

## 基础 CRUD

### 创建和打开数据库

```go
package main

import (
    "fmt"
    "log"

    "github.com/dgraph-io/badger/v4"
)

func main() {
    // 打开数据库
    db, err := badger.Open(badger.DefaultOptions("./badger_data"))
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    fmt.Println("Badger 数据库已打开")
}
```

### Put / Get / Delete

```go
// 写入
err = db.Update(func(txn *badger.Txn) error {
    err := txn.Set([]byte("hello"), []byte("world"))
    return err
})

// 读取
err = db.View(func(txn *badger.Txn) error {
    item, err := txn.Get([]byte("hello"))
    if err != nil {
        return err
    }
    val, err := item.ValueCopy(nil)
    if err != nil {
        return err
    }
    fmt.Printf("读取: %s = %s\n", "hello", val)
    return nil
})

// 删除
err = db.Update(func(txn *badger.Txn) error {
    err := txn.Delete([]byte("hello"))
    return err
})
```

### 完整 CRUD 示例

```go
package main

import (
    "fmt"
    "log"

    "github.com/dgraph-io/badger/v4"
)

func main() {
    db, err := badger.Open(badger.DefaultOptions("./badger_data"))
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    // 写入多条数据
    err = db.Update(func(txn *badger.Txn) error {
        for i := 0; i < 5; i++ {
            key := []byte(fmt.Sprintf("key-%d", i))
            val := []byte(fmt.Sprintf("value-%d", i))
            if err := txn.Set(key, val); err != nil {
                return err
            }
        }
        return nil
    })
    if err != nil {
        log.Fatal(err)
    }
    fmt.Println("写入 5 条数据完成")

    // 遍历所有 key
    err = db.View(func(txn *badger.Txn) error {
        opts := badger.DefaultIteratorOptions
        opts.PrefetchSize = 10
        it := txn.NewIterator(opts)
        defer it.Close()

        for it.Rewind(); it.Valid(); it.Next() {
            item := it.Item()
            key := string(item.Key())
            val, _ := item.ValueCopy(nil)
            fmt.Printf("  %s = %s\n", key, val)
        }
        return nil
    })
    if err != nil {
        log.Fatal(err)
    }

    // 删除 key-2
    err = db.Update(func(txn *badger.Txn) error {
        return txn.Delete([]byte("key-2"))
    })
    if err != nil {
        log.Fatal(err)
    }
    fmt.Println("删除 key-2 完成")
}
```

## 事务操作

### 乐观事务

```go
// 原子递增
err = db.Update(func(txn *badger.Txn) error {
    item, err := txn.Get([]byte("counter"))
    if err == badger.ErrKeyNotFound {
        return txn.Set([]byte("counter"), []byte{0, 0, 0, 0, 0, 0, 0, 1})
    }
    if err != nil {
        return err
    }

    var val []byte
    val, err = item.ValueCopy(nil)
    if err != nil {
        return err
    }

    // 递增
    count := binary.LittleEndian.Uint64(val)
    count++
    buf := make([]byte, 8)
    binary.LittleEndian.PutUint64(buf, count)
    return txn.Set([]byte("counter"), buf)
})
```

### 事务冲突处理

```go
import (
    "math/rand"
    "sync"
)

func concurrentUpdate(db *badger.DB, key []byte) {
    var wg sync.WaitGroup
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for retry := 0; retry < 3; retry++ {
                err := db.Update(func(txn *badger.Txn) error {
                    // 读取-修改-写入
                    item, err := txn.Get(key)
                    if err != nil {
                        return err
                    }
                    val, _ := item.ValueCopy(nil)
                    count := binary.LittleEndian.Uint64(val)
                    count++
                    buf := make([]byte, 8)
                    binary.LittleEndian.PutUint64(buf, count)
                    return txn.Set(key, buf)
                })
                if err == badger.ErrConflict {
                    // 冲突重试
                    continue
                }
                return
            }
        }(i)
    }
    wg.Wait()
}
```

## WriteBatch 批量写入

```go
func batchWrite(db *badger.DB, n int) error {
    wb := db.NewWriteBatch()
    defer wb.Cancel()

    for i := 0; i < n; i++ {
        key := []byte(fmt.Sprintf("batch-key-%d", i))
        val := make([]byte, 100)  // 100 字节值
        rand.Read(val)
        if err := wb.Set(key, val); err != nil {
            return err
        }
    }
    return wb.Flush()
}
```

## 备份与恢复

```go
import (
    "bytes"
    "io/ioutil"
)

// 备份到文件
func backupDB(db *badger.DB, path string) error {
    f, err := os.Create(path)
    if err != nil {
        return err
    }
    defer f.Close()
    _, err = db.Backup(f, 0)
    return err
}

// 从文件恢复
func restoreDB(path, dataDir string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()

    db, err := badger.Open(badger.DefaultOptions(dataDir))
    if err != nil {
        return err
    }
    defer db.Close()

    return db.Load(f, 100)
}
```

## 性能基准测试

### 写入基准

```go
// badger_bench_test.go
package main

import (
    "testing"
    "math/rand"

    "github.com/dgraph-io/badger/v4"
)

func BenchmarkBadgerWrite(b *testing.B) {
    db, _ := badger.Open(badger.DefaultOptions("./bench_data"))
    defer db.Close()

    b.ResetTimer()
    for i := 0; i < b.N; i++ {
        key := []byte(fmt.Sprintf("key-%d", i))
        val := make([]byte, 100)
        rand.Read(val)

        db.Update(func(txn *badger.Txn) error {
            return txn.Set(key, val)
        })
    }
}

func BenchmarkBadgerBatchWrite(b *testing.B) {
    db, _ := badger.Open(badger.DefaultOptions("./bench_data"))
    defer db.Close()

    b.ResetTimer()
    for i := 0; i < b.N; i++ {
        wb := db.NewWriteBatch()
        for j := 0; j < 100; j++ {
            key := []byte(fmt.Sprintf("key-%d-%d", i, j))
            val := make([]byte, 100)
            rand.Read(val)
            wb.Set(key, val)
        }
        wb.Flush()
    }
}

func BenchmarkBadgerRead(b *testing.B) {
    db, _ := badger.Open(badger.DefaultOptions("./bench_data"))
    defer db.Close()

    // 预写入数据
    db.Update(func(txn *badger.Txn) error {
        for i := 0; i < 10000; i++ {
            txn.Set([]byte(fmt.Sprintf("key-%d", i)), []byte("value"))
        }
        return nil
    })

    b.ResetTimer()
    for i := 0; i < b.N; i++ {
        key := []byte(fmt.Sprintf("key-%d", i%10000))
        db.View(func(txn *badger.Txn) error {
            _, err := txn.Get(key)
            return err
        })
    }
}
```

运行基准测试：

```bash
go test -bench=. -benchmem
```

## ValueLog GC 实验

```go
// 观察 ValueLog GC 效果
func valueLogGcDemo(db *badger.DB) {
    // 写入大量数据触发 ValueLog 增长
    for i := 0; i < 100000; i++ {
        key := []byte(fmt.Sprintf("key-%d", i))
        val := make([]byte, 1024)  // 1KB value
        rand.Read(val)

        db.Update(func(txn *badger.Txn) error {
            return txn.Set(key, val)
        })
    }

    // 删除一半数据，产生无效 ValueLog 条目
    for i := 0; i < 50000; i++ {
        key := []byte(fmt.Sprintf("key-%d", i))
        db.Update(func(txn *badger.Txn) error {
            return txn.Delete(key)
        })
    }

    // 手动触发 GC
    for {
        err := db.RunValueLogGC(0.5)  // 50% 阈值
        if err == badger.ErrNoRewrite {
            break
        }
    }
    fmt.Println("ValueLog GC 完成")
}
```

## 要点总结

- **环境搭建**：Go 1.21+，`go get github.com/dgraph-io/badger/v4`
- **CRUD 操作**：`Update` 写入，`View` 读取，`Delete` 删除
- **事务**：乐观事务自动重试，WriteBatch 批量写入
- **GC 实验**：`RunValueLogGC` 回收 ValueLog 空间
- **基准测试**：单条写入 vs 批量写入的性能差异

## 思考题

1. 单条写入和 WriteBatch 批量写入的性能差异有多大？为什么？
2. ValueLog GC 触发后，对前台读写性能有什么影响？
3. 如何通过调整 MemTable 大小和 Compactor 数量优化写入性能？