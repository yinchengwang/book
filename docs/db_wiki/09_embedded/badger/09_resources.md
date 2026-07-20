# Badger 学习资源

## 学习目标

- 掌握 Badger 源码目录结构
- 了解关键源码文件
- 建立源码阅读路径

## 源码路径

### 项目仓库

```
reference/open-source/badger/  # 源码镜像（非 CMake 构建）
```

### 目录结构

```
badger/
├── badger.go              # DB 主入口
├── db.go                  # DB 核心实现
├── txn.go                 # 事务实现
├── oracle.go              # MVCC 时间戳管理
├── iterator.go            # 迭代器实现
├── level_handler.go       # LSM 层级管理
├── compact.go             # Compaction 实现
├── stream.go              # 流式读取
│
├── table/                 # SSTable 实现
│   ├── builder.go         # SSTable 构建
│   ├── table.go           # SSTable 读取
│   ├── iterator.go        # Block 迭代器
│   └── bloom_filter.go    # Bloom Filter
│
├── value/                 # ValueLog 实现
│   ├── value.go           # ValueLog 核心
│   ├── entry.go           # Entry 格式
│   └── gc.go              # GC 实现
│
├── skl/                   # SkipList MemTable
│   └── skl.go             # 跳表实现
│
├── y/                     # 通用工具
│   ├── error.go           # 错误处理
│   ├── file_dsync.go      # 文件操作
│   └── metrics.go         # 指标
│
├── options/               # 配置选项
│   └── options.go         # 压缩等选项
│
└── pb/                    # Protobuf 定义
    └── pb.proto           # SSTable 格式
```

## 关键源码文件

### 核心入口

| 文件 | 职责 | 关键函数 |
|------|------|---------|
| `badger.go` | 包入口 | `Open()`, `OpenManaged()` |
| `db.go` | DB 核心实现 | `DB` 结构体, `get()`, `update()` |
| `txn.go` | 事务实现 | `Txn`, `Get()`, `Set()`, `Commit()` |

### LSM 结构

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `level_handler.go` | 层级管理 | `levelHandler`, `getTables()` |
| `compact.go` | Compaction | `compactionBuilder`, `runCompaction()` |
| `manifest.go` | 元数据管理 | `manifest`, `createManifest()` |

### ValueLog

| 文件 | 职责 | 关键函数 |
|------|------|---------|
| `value/value.go` | ValueLog 核心 | `ValueLog`, `Open()`, `Read()` |
| `value/entry.go` | Entry 格式 | `Entry`, `Encode()` |
| `value/gc.go` | 垃圾回收 | `RunGC()`, `pickValueLog()` |

### SSTable

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `table/builder.go` | SSTable 构建 | `TableBuilder`, `Add()` |
| `table/table.go` | SSTable 读取 | `Table`, `Block()` |
| `table/iterator.go` | Block 迭代器 | `BlockIterator`, `Next()` |

### 事务层

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `oracle.go` | 时间戳管理 | `oracle`, `readTs()`, `commitTs()` |
| `watermark.go` | 水印追踪 | `WaterMark`, `Begin()`, `End()` |

### SkipList MemTable

| 文件 | 职责 | 关键函数 |
|------|------|---------|
| `skl/skl.go` | SkipList 实现 | `Skiplist`, `Add()`, `Get()` |

## 源码阅读路径

### 写入路径

```
1. badger.go: Open()              # 初始化 DB
2. txn.go: Set()                  # 设置键值
3. skl/skl.go: Add()              # 写入 MemTable
4. value/value.go: Open()         # 写入 ValueLog
5. compact.go: runCompaction()    # 触发 Compaction
```

### 读取路径

```
1. txn.go: Get()                  # 读取请求
2. skl/skl.go: Find()             # MemTable 查找
3. level_handler.go: get()        # LSM 层级查找
4. table/table.go: Index()        # SSTable 索引
5. value/value.go: Read()         # ValueLog 读取
```

### Compaction 流程

```
1. compact.go: run()              # Compaction 入口
2. level_handler.go: mergeTables()# 合并 SSTable
3. table/builder.go: Add()        # 构建新 SSTable
4. manifest.go: applyChange()     # 更新元数据
```

### ValueLog GC 流程

```
1. value/gc.go: pickValueLog()    # 选择 GC 候选
2. value/gc.go: runGC()           # 执行 GC
3. value/value.go: Rewrite()      # 重写有效数据
4. db.go: vlogSize()              # 更新指针
```

## 设计文档

### 官方文档

- **GitHub Wiki**: https://github.com/dgraph-io/badger/wiki
- **API 文档**: https://pkg.go.dev/github.com/dgraph-io/badger/v4

### 关键设计文章

1. **"Badger: Fast Key-Value DB in Go"** — Dgraph 博客
   - 键值分离设计动机
   - ValueLog GC 策略
   - 与 RocksDB 对比

2. **"Designing Badger"** — Design.md in repo
   - LSM-Tree 设计决策
   - 事务模型选择
   - 并发控制机制

3. **"WISCKEY: Separating Keys from Values"**
   - 键值分离理论基础
   - 写放大量化分析

## 扩展学习

### 相关项目

| 项目 | 说明 |
|------|------|
| Dgraph | 图数据库，Badger 的设计初衷 |
| Ristretto | Dgraph 的高性能缓存库 |
| Badger v1/v2/v3/v4 | 版本演进历史 |

### 性能分析工具

```go
import "runtime/pprof"

// CPU 性能分析
f, _ := os.Create("cpu.prof")
pprof.StartCPUProfile(f)
// ... 运行基准测试
pprof.StopCPUProfile()

// 内存分析
memProf, _ := os.Create("mem.prof")
pprof.WriteHeapProfile(memProf)
```

### 可视化工具

- **pprof 工具链**: `go tool pprof cpu.prof`
- **Badger 内置指标**: `db.Tables()`, `db.Size()`

## 要点总结

- **源码入口**：`badger.go` → `db.go` → `txn.go`
- **核心结构**：LSM(`level_handler.go`) + ValueLog(`value/value.go`)
- **事务管理**：`oracle.go` + `watermark.go`
- **Compaction**：`compact.go` → `table/builder.go`

## 思考题

1. Badger 的 ValueLog 设计与 RocksDB 的 BlobDB 有什么区别？
2. 为什么 Badger 选择 SkipList 而不是其他数据结构作为 MemTable？
3. `oracle.go` 中的 Watermark 机制如何保证事务隔离？