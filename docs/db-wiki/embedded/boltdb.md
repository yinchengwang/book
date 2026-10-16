# BoltDB Wiki（嵌入式 B+tree 键值数据库，Go 实现）

> 参考版本：BoltDB 仓库主线（boltdb v1.3.x 末代，作者 Ben Johnson / `github.com/boltdb/bolt`）；当前主线已 archival，由 `etcd-io/bbolt` fork 维护（v1.4+ 含 TLS、Remove txn、并发优化等）。本 wiki 以 `bbolt` v1.3+ 为基准。

## 一、项目定位

BoltDB 是 Ben Johnson（`fly.io` 创始人之一）用 Go 编写的**纯 Go、嵌入式 B+tree 键值数据库**，灵感来自 LMDB。它使用 mmap + Copy-on-Write 提供 ACID 事务、单写者多读者、零配置单文件部署，**完全没有 cgo 依赖**，完美契合 Go 生态的嵌入式 KV 需求。

- **痛点**：
  - LMDB 是 C 库，Go 集成需要 cgo（牺牲静态二进制 + 跨编译便利）；
  - LevelDB 是 C++ + 编译依赖，Go 集成通常有 binding 损耗；
  - SQLite 在 Go 生态里没有"原生 KV"模式；
  - 纯 Go 嵌入式 KV 需求（Git、Consul、InfluxDB 早期、etcd watch cache）；
- **典型用户**：Consul（早期 backend，原生 BoltDB）、InfluxDB（早期 TSM pre-aggregation index）、etcd/bbolt（fork）、NATS Streaming/STAN、Prometheus（rule storage）、Fly.io 应用配置。

## 二、发展历史与社区

- **起源**：2013 年 Ben Johnson 在 `SkyDB` 基础上开始 BoltDB，使用 Go 重写，目标是「Go 生态的纯嵌入式 KV」；
- **里程碑**：
  - 2013-11：v0.1（alpha）；
  - 2015-08：v1.0 稳定；
  - 2016-2018：v1.1-1.3 系列完善 freelist、并发错误修复；
  - **2018-07 归档**：Ben Johnson 离开项目，仓库归档但 fork 继续开发；
  - **2019+**：CoreOS/Red Hat 接手，fork 为 `etcd-io/bbolt`，增加 TLS、并发回滚等；
  - 2021-2024：bbolt v1.3.x 系列与 etcd 同步版本化（v3.5、v3.6）；
- **维护状态**：主线仓库 archived，社区维护转向 bbolt；每月小 PR 提交到 bbolt；
- **社区**：GitHub Discussions（bbolt）、CNCF Slack `#bbolt`。

## 三、许可与商业限制

- **许可证**：**MIT License**（`LICENSE` 在仓库根，boltdb 与 bbolt 同）；
  - 极宽松、几乎无限制；
  - 商业使用、修改、分发、闭源衍生均可；
- **商业版本**：无；
- **与 etcd 关系**：bbolt 由 etcd 团队维护，作为 etcd 默认存储后端之一（v3.4+ 主要用，但 watch/lease 部分仍用 Bolt）。

## 四、业界主要使用方与案例

- **Consul**：早期 backend（BoltDB），v1.7+ 仍支持 BoltDB；
- **InfluxDB**：早期 (1.x) TSM 索引与 metadata；
- **etcd (bbolt fork)**：v3.5 起 watch 存储与 lease storage（与 wal 并存）；
- **NATS Streaming / JetStream 早期**：持久化 store；
- **Prometheus**：rule/alert manager 的 storage；
- **Nomad**：早期 state backend；
- **Fly.io**：Ben Johnson 自己公司的应用状态（讽刺的是它用 BoltDB，但 Fly.io 主业务已是 serverless）；
- **Skygear、Kivik**：嵌入式应用；
- **教学**：CMU 15-721 的「纯 Go 实现 mmap + B+tree」案例。

## 五、一句话对比定位

> BoltDB 是「**纯 Go、零 cgo、mmap + COW 单写多读**」的嵌入式 B+tree KV；与 **LMDB（极致轻量 C）**、**LevelDB（LSM C++）**、**Badger（Go LSM/WiscKey）**、**SQLite（关系 + schema）** 形成「**嵌入式 KV**」象限。BoltDB 设计相对 LMDB 更工程化、API 更友好，并保留了 LMDB 的「mmap COW」哲学。

## 六、整体架构

BoltDB 是单 Go 模块（`github.com/boltdb/bolt`）实现，核心约 7000 行 Go，结构非常清晰：

### 6.1 总体分层

```
┌──────────────────────────────────────────────────────────────────┐
│             Public API (bolt.go / db.go 顶部)                      │
│   bolt.Open / DB.Close                                          │
│   DB.Update / DB.View                                           │
│   DB.Begin (read-write / read-only)                             │
│   Bucket.Get / Put / Delete / Cursor                            │
└────────────────────┬─────────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────────┐
│               Transaction Layer (tx.go)                           │
│   - type Tx struct { meta *meta; root pgid; ... }                │
│   - tx.Commit / tx.Rollback                                     │
│   - tx.Write / tx.ReadOnly 标志                                  │
└────────────────────┬─────────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────────┐
│               Cursor + B+tree Layer                              │
│   - cursor.go (cursor 移动、duplicate key)                      │
│   - bucket.go (Bucket 数据抽象)                                 │
│   - node.go (B+tree 节点: branch / leaf)                         │
└────────────────────┬─────────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────────┐
│               Page Layer (page.go)                               │
│   - page.go: type Page struct { id pgid; flags uint16; ... }   │
│   - leafPageElement / branchPageElement / freelist struct       │
│   - 直接在 mmap 内存上操作                                              │
└────────────────────┬─────────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────────┐
│               Free List (freelist.go)                            │
│   - 两种实现：array (老) + hashmap (新)                         │
│   - 跟踪 free / pending free pages                              │
│   - 启动时读 freelist 页，重建 in-memory state                    │
└────────────────────┬─────────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────────┐
│               DB Meta + Disk (db.go, meta.go)                    │
│   - 双 meta page (meta0 / meta1)                                   │
│   - mmap 整个 DB 文件 (syscall.Mmap)                              │
│   - Db 状态: writemem, writeog sync                              │
└──────────────────────────────────────────────────────────────────┘
```

源码入口：
- 主入口：`db.go:DB.Open`、`db.go:DB.Update`、`db.go:DB.View`；
- 事务：`tx.go:Tx`；
- 索引：`db.go:DB.Cursor`、`cursor.go:Cursor`、`bucket.go:Bucket`；
- Page / Free：`page.go`、`freelist.go`；
- 节点：`node.go:node`、`node.go:branchPageElement`、`node.go:leafPageElement`；
- Meta：`meta.go:meta`。

### 6.2 进程模型

BoltDB 是**嵌入式库**：

- **同进程多协程**：
  - 通过 `db.rwlock sync.Mutex` 保护写事务串行化；
  - 多读事务**可并发**（基于 read-only transaction snapshot）；
- **多进程**：
  - 通过 OS file lock（`flock`/`LockFileEx`）获得写独占；
  - 多进程读通过 mmap 自然共享；
- **后台线程**：无（Open 时不创建 goroutine，sync 直接调 syscall.Msync）；
- **同步原语**：`flock` on Unix / `LockFileEx` on Windows。

### 6.3 一条 PUT 命令的端到端路径

1. **应用调用**：`db.Update(func(tx *bolt.Tx) error { return tx.Bucket(...).Put(k, v) })`；
2. **开启写事务**：`db.begin(true /*writable*/)` → `db.rwlock.Lock()` + 跨进程 `flock`；
3. **取得 root bucket**：检查 root bucket 是否存在；
4. **Cursor**：`Bucket.Cursor()` 创建游标；
5. **定位 key**：`cursor.go:Cursor.Seek()`，从 root page 向下找；
6. **插入**：
   - 节点未满：直接 `node.put(k, v)`；
   - 节点满：split + 递归向上分裂；
7. **记录 dirty**：modified pages 入 `tx.pages[pgid]*page`;
8. **Rebalance**（可选）：合并稀疏节点；
9. **Rebuild freelist**：`Freelist.write` 把 pending free pages 写入 freelist page；
10. **Commit**：`Tx.Commit()` 写 meta pages + `flock` 解锁 + 释放 rwlock。

### 6.4 一条 GET 命令的端到端路径

1. **应用调用**：`db.View(func(tx *bolt.Tx) error { return tx.Bucket(...).Get(k) })`；
2. **开启读事务**：`db.begin(false /*read-only*/)`，snapshot = 当前 meta；
3. **取得 root bucket**：从 `tx.meta.root` 拿到 root；
4. **Cursor 搜索**：`cursor.go:Cursor.First()` / `Cursor.Seek(k)`；
5. **Descent B+tree**：每层 page + node 二分；
6. **匹配**：找到 key 返回 value，否则 nil；
7. **返回**：`tx.Commit()`（no-op，因为 read-only）。

### 6.5 一条 COMMIT 的端到集路径

1. **应用调用**：`Tx.Commit()`；
2. **Free List 序列**：把 `tx.pending` 写入 freelist page；
3. **Spill**：`Spill()` 把内存 dirty pages 写到 mmap（`page.write()` 直接写 mmap）；
4. **Meta 写入**：
   - 找到非 in-use 的 meta slot；
   - 序列化 `meta{version, ...}` → page[idx 0/1]；
   - `tx.db.file.SyncFile()`（`MS_SYNC` 等价物）；
5. **更新 `db.meta`**：切换 txid；
6. **释放锁**：flock + rwlock。

## 七、存储引擎

### 7.1 文件结构

```
┌──────────────────────────────────────────────────────────────┐
│      BoltDB File: my.db                                       │
│                                                              │
│   [Page 0]    Meta page 0                                    │
│   [Page 1]    Meta page 1                                    │
│   [Page 2]    Free List page (or null)                       │
│   [Page 3]    Root of bucket "root"                          │
│   [Page 4...] B+tree 节点 (branch / leaf pages)              │
│                                                              │
│   - 整个文件通过 syscall.Mmap 暴露到进程虚拟地址             │
│   - 读：直接返回 mmap 字节指针                                │
│   - 写：Copy-on-Write 复制 page 到新位置                       │
└──────────────────────────────────────────────────────────────┘
```

### 7.2 Page Layout（`page.go`）

```go
// db.go + page.go
type pgid uint64

type Page struct {
    id       pgid
    flags    uint16   // branchPageFlag / leafPageFlag / metaPageFlag / freelistPageFlag
    count    uint16   // number of elements
    overflow uint32   // overflow page id for large values
}
```

- **Page Size**：默认 **4 KiB**（`db.pageSize`），不能修改运行时；
- **Overflow Pages**：超过 1/4 page size 的 value 走 overflow pages；
- **Page Flags**：
  - `0x01` PagemetaFlag (meta)
  - `0x02` PagemetaFlag
  - `0x04` leafPageFlag
  - `0x08` branchPageFlag
  - `0x10` freelistPageFlag

### 7.3 Meta Page 双缓冲

```go
// meta.go
type meta struct {
    Magic    uint32  // 固定 magic number
    Version  uint32  // BoltDB version
    PageSize uint32  // 4096
    Flags    uint32  // 0 或 freelist 全局标记
    Root     bucket  // 根 bucket 的 {root pgid, seq}
    Freelist pgid    // freelist page id
    Pgid     pgid    // txid (current)
    Checksum uint64  // SHA-1-like checksum of serialized data
}
```

- 两个 meta slot 在 page 0 与 page 1；
- Commit 时把 txid 递增 + 写入非活跃 slot；
- 启动时检查两个 slot，选 txid 大的作为当前 meta。
- bbolt 增加了 SHA1 checksum：避免 meta 损坏后静默错误。

### 7.4 Free List（`freelist.go`）

```go
type Freelist struct {
    freelistType  FreelistType  // FreelistArray / FreelistHashMap
    ids           []pgid        // array 模式
    hashmap       map[pgid]uint64  // hashmap 模式: pending -> allocate seq
    pending       map[pgid]*tx  // 待 free pages 与事务对应
    allocatorPage pgid         // 我们的 allocator 总页
    allocateIndex uint64
}
```

- **Array 模式**：简单 `[]pgid`，读快但写放大；
- **HashMap 模式（bbolt 默认）**：O(1) 分配复用，避免 integer overflow；
- **Pending pages**：事务的 dirty page 在 commit 时才真正加入 free list；
- **向后兼容**：老文件以 array 模式打开，**autoupgrade** 为 hashmap。

## 八、索引机制（嵌入式专项）

### 8.1 B+tree 唯一索引

BoltDB **只有 B+tree**，无其他索引类型：

- **结构**：
  - 内部节点（branch）含 `[child_pgid, key]*n`；
  - 叶子节点（leaf）含 `[key, value]*n`；
- **Key Ordering**：默认 bytewise lexicographic，可调用 `Bucket.SetKeyCompare()` 自定义；
- **Sub-Bucket**：嵌套 B+tree（bucket 是逻辑 KV，每个 bucket 也是 B+tree）；
- **Inplace vs NotInplace**：
  - Put 时若节点未满且 fit：inplace modify page；
  - 否则 split + new pages。

### 8.2 Node 操作（`node.go`）

```go
type node struct {
    bucket     *Bucket
    isLeaf     bool
    key        []byte
    pgid       pgid
    parent     *node
    children   nodes
    inodes     inodes  // branch/leaf elements
}

func (n *node) put(key, value []byte, pgid pgid, flags uint32) {
    // split if need, recursive ancestor update
}
```

- **`put`**：插入 entry；split 阈值 `fillThreshold=0.5`；
- **`remove`**：删除 entry；rebalance 阈值 `fillThreshold=0.5`；
- **`split`**：节点满时分裂，父节点加新 child pointer。

### 8.3 mmap / 文件 I/O 策略

与 LMDB 不同但接近：

- **read-only mmap**：`syscall.Mmap(fd, ...MAP_SHARED)` → 整个 DB 暴露为内存；
- **read-write mmap**：默认 **非 writemapped**，写时 memcpy 到内存；
  - BoltDB 早期有 `db.UseMmap = true`，但默认是 `writemapped=false`；
  - bbolt v1.3 改回 `writemapped` 默认 true；
- **msync 时机**：
  - Default: `MetaSync = true`，每次 Commit 同步刷 meta；
  - `NoSync` = false;
  - `NoFreelistSync`;
- **madvise 策略**：
  - OpenDB 时调用 `syscall.Madvise(b, syscall.MADV_RANDOM)`；
  - 主程序在前会维护 cache hint（每次 read 后可选 `MADV_WILLNEED` 或 `MADV_DONTNEED`）；
- **POSIX file lock**：默认 `flock`，确保跨进程写互斥。

### 8.4 进程内嵌 vs 服务器

| 维度 | BoltDB | LMDB |
|------|--------|------|
| 语言 | Go (纯) | C |
| 网络开销 | 零（库调用） | 零（库调用） |
| 跨进程共享 | mmap + flock | mmap + 系统锁 |
| 多线程 | goroutine + sync.Mutex | pthread mutex |
| 静态二进制 | **是**（无 cgo）| 否（libc / pthread） |
| 跨编译友好 | **是** | 否 |

### 8.5 多线程 / 多进程安全

BoltDB 是 Go 生态最常用嵌入式 KV 的原因之一：

- **同进程协程**：
  - 多读并发：基于 `RLocker()` 方式；并发无锁（snapshot）；
  - 写事务串行：基于 `db.rwlock`；
- **跨进程**：
  - 写：flock 互斥（同一文件同一进程唯一写者）；
  - 读：mmap 自然共享，无需任何锁；
- **事务嵌套**：
  - 不支持嵌套 write tx；
  - 不支持 read tx 内嵌 read tx；

### 8.6 页面缓存与刷盘

- **依赖 OS Page Cache**：BoltDB 主要相信 OS；
- **`db.UseMmap`**：
  - 默认 `false`：使用 read buffer，不直接通过 mmap；
  - `true`：直接 mmap；
- **fsync 时机**：
  - 默认每次 Commit 一次 fsync；
  - `NoSync=true` 可关闭（牺牲安全性）；
- **dirty page tracking**：基于 `tx.pages` map（pgid → mmap memory）。

### 8.7 崩溃恢复

- **机制**：COW + Meta Page 双缓冲（与 LMDB 相同思路）；
- **崩溃后**：
  - 启动时读 meta[0] / meta[1]，选 txid 大的；
  - bbolt 还检查 meta checksum 是否合法；
  - 数据回滚到最后一次成功 Commit；
- **优势**：
  - 无 WAL 文件；
  - 恢复时间 O(1)；
- **代价**：
  - 写放大：每个事务重写 root-to-leaf 路径；
  - COW 复制开销：B+tree 高扇出，路径 cost = log(N)；

## 九、查询/计算/接口层（嵌入式专项）

### 9.1 API 表面（核心 < 30 函数）

```go
// DB
func Open(path string, mode os.FileMode, options *Options) (*DB, error)
func (db *DB) Close() error
func (db *DB) Update(fn func(*Tx) error) error
func (db *DB) View(fn func(*Tx) error) error
func (db *DB) Begin(writable bool) (*Tx, error)
func (db *DB) Stats() Stats

// Tx
func (tx *Tx) Bucket(name []byte) *Bucket
func (tx *Tx) CreateBucket(name []byte) (*Bucket, error)
func (tx *Tx) CreateBucketIfNotExists(name []byte) (*Bucket, error)
func (tx *Tx) DeleteBucket(name []byte) error
func (tx *Tx) Commit() error
func (tx *Tx) Rollback() error
func (tx *Tx) Cursor() *Cursor
func (tx *Tx) ForEach(fn func([]byte, *Bucket) error) error

// Bucket
func (b *Bucket) Get(key []byte) []byte
func (b *Bucket) Put(key []byte, value []byte) error
func (b *Bucket) Delete(key []byte) error
func (b *Bucket) Cursor() *Cursor
func (b *Bucket) NextSequence() (uint64, error)
func (b *Bucket) Stats() BucketStats

// Cursor
func (c *Cursor) First() (key []byte, value []byte)
func (c *Cursor) Seek(seek []byte) (key []byte, value []byte)
func (c *Cursor) Next() (key []byte, value []byte)
func (c *Cursor) Prev() (key []byte, value []byte)
```

### 9.2 嵌入式存储格式

- **单文件**：一个 `.db` 文件；
- **自描述**：meta 页内嵌 magic + version + page size；
- **多 bucket**：根 bucket 内嵌 sub-bucket 的 B+tree（bucket id = pgid）；
- **兼容性**：bbolt 自动检测 freelist 模式 / meta version，autoupgrade。

### 9.3 Schema 演进（Forward-compatible）

- **bucket 创建 / 删除**：通过 `Meta.versions` + `Freelist` 协作完成；
- **破坏性变更**：bbolt 1.3 起 SHA-1 checksum，破坏 meta 立即报错；
- **跨版本**：老 bbolt 读到新 DB 文件支持（基于 meta.version 字段）；bbolt 增加字段不会破坏。

### 9.4 并发控制

- **乐观锁 / MVCC**：
  - 读 snapshot：每次 `Begin(false)` capture `db.meta`；
  - 写检查：tx `pending.pages` 与 `db.meta` 切换状态；
  - Commit 阶段：freelist 写入 + meta 切换；
- **隔离级别**：Snapshot Isolation；
- **冲突检测**：
  - 同进程：`db.rwlock`，写事务互斥；
  - 跨进程：`flock`；
  - 应用层：**write-write conflict** 不会自动 abort，需要调用方避免；
- **脏读**：
  - 读永远看到 commit 时一致的 snapshot；
  - 不会看到未 commit 的中间状态。

### 9.5 小数据 / 单文件场景优化

- **零配置**：`bolt.Open(path, 0600, nil)` 即可；
- **单文件部署**：整个 DB 一个文件；
- **嵌入式 BI / 应用状态**：
  - **Hydra Config Store**：服务发现；
  - **OAuth state store**：短期 token KV；
  - **User profile cache**：嵌入式 user KV；
- **Git 风格 layer KV**：本地对象数据库（GO 实现 Git-like 库可用 BoltDB）；
- **Go-embedded 优势**：
  - **无 cgo**：跨编译无障碍，wasm 也支持；
  - **类型友好**：Go struct 直接 via MsgPack/Protobuf 进 BoltDB。

## 十、事务与一致性

- **隔离级别**：默认 **Snapshot Isolation**；
- **ACID**：完整支持；
- **MVCC**：基于 meta pages 双缓冲 + page id 引用；
- **写事务串行**：所有写事务同步执行；
- **NO 事务嵌套**；
- **Abort**：直接 discard pending pages。

## 十一、复制与高可用

BoltDB 是**嵌入式库**，无内置复制：

- **`bolt.bolt` 工具**：`bolt` CLI 工具做 inspect / dump / compact / page 计数等；
- **应用层复制**：调用方定期拷贝 `.db` 文件（flock 文件锁时能保证一致）；
- **etcd/bbolt 集群复制**：bbolt 在 etcd 内部用 Raft 协议复制，不依赖 Bolt 本身。

## 十二、扩展性

- **Go hook 友好**：`Stats()` API、`DB.GoRoutine` 可注入；
- **Bucket 嵌套**：天然支持 logical schema 分层；
- **Custom Comparators**：`Bucket.SetKeyCompare()` `Bucket.SetValueCompare()`；
- **无 SQL layer**：BoltDB 仅 KV，无 SQL；上层可堆叠 `sql-bolt`、`dbmate` 等迁移工具。

## 十三、性能与基准

- **典型性能**（参考 bench）：
  - **读**：~1-10 μs（OS cache 命中）；
  - **写**：~5-50 ms（commit fsync 限制）；
  - **Batch write**：100K record/s（在 SSD 上）；
- **vs Badger（Go LSM）**：
  - 读：BoltDB 写优势 2-5x（mmap + 简单 B+tree）；
  - 范围扫描：BoltDB 与 Badger 相当；
  - 写吞吐：Badger 高 2-10x（LSM 顺序写）；
- **官方 bench**：`bench_test.go` 中 KV benchmarks；
- **TIKV/etcd 内置 bench**：bbolt v1.3 在 etcd v3.5 测试中表现稳定。

## 十四、运维与可观测性

- **Stats**：
  - `db.Stats()`：返回 `*Stats{cgo calls, page count, free page count, ...}`；
  - `bucket.Stats()`：返回 bucket 内 fork count, inline count, overflow count；
- **CLI**：`bolt` 工具：info, pages, buckets, get, put, rm, compact, check 等；
- **健康检查**：`db.View(...)` 验证 DB 可达；
- **Compact**：API 调用 `db.Compact()` 重新整理 freelist（优化碎片）。
- **日志**：无内置日志；依赖应用层 log。

## 十五、生态与集成

- **客户端 SDK**：Go（native）、其他语言通过（CGo shim）；
- **中间件**：sql-bolt（SQL 接口）；
- **服务发现**：Consul、etcd、Nomad；
- **监控**：Prometheus 通过 etcd bbolt exporter；
- **迁移工具**：bolt-migrate、bbolt-migrate；
- **替代 / Fork**：
  - **etcd-io/bbolt**：主线 fork；
  - **NATS Streaming**（已退役，由 JetStream 接替）；
  - **SkyDB**：Ben Johnson 的早期同类项目；
  - **Storm、BoltStore**：BoltDB 之上的 ORM / Store。

## 十六、核心论文与算法原理

- **B+tree**：Comer *"The Ubiquitous B-Tree"* (1979)；
- **MVCC + 页级版本**：LMDB 同款「无 WAL」设计哲学；
- **Bernstein & Goodman**：Serializable Transaction *Concurrency Control Algorithms* (1981)；
- **Gray & Reuter**：*Transaction Processing: Concepts and Techniques*；
- **BoltDB blog**：Ben Johnson *"Building BoltDB with Howard Chu"*（2014）借鉴 LMDB 设计哲学；
- **Howard Chu LMDB 文档**：作为相关参考。

## 十七、源码学习路径

必读清单（按从浅到深顺序）：

1. **入口**：`db.go:Open`；
2. **事务**：`tx.go:Begin` + `tx.go:Commit`；
3. **Bucket**：`bucket.go:Bucket` + `bucket.go:put/put/put`；
4. **Cursor**：`cursor.go:Cursor.Seek/Next/Prev`；
5. **Node**：`node.go:node.read` / `node.go:node.write` / `node.go:node.spill`；
6. **Page**：`page.go:page()` 工厂函数；
7. **Free List**：`freelist.go:Freelist.write` + `Freelist.allocate`；
8. **Meta**：`meta.go:meta.writeTo` / `meta.go:meta.readFrom`；
9. **mmap**：`db.go:mmap` + `db.go:munmap`；
10. **Sync**：`db.go:Sync` (commit fsync)。

源码规模：约 **9000 行 Go**（含 `bolt` 工具），核心库约 **6000 行**。

## 十八、可移植性与部署

- **平台**：Linux、macOS、FreeBSD、Windows（依赖 syscall.Mmap 等 OS 抽象）；
- **构建**：`go get github.com/boltdb/bolt`（archived → `go.etcd.io/bbolt`）；
- **跨编译**：纯 Go，无 cgo 依赖，GOOS/GOARCH 全平台支持；
- **WASM**：`bbolt` 在 wasm 上无法 mmap（syscall.Mmap 不支持），需要适配；
- **容器**：无外部进程，开箱即用。

## 十九、变体与衍生项目

- **etcd-io/bbolt**：主线 fork，加入 TLS、并发回滚、checksum；
- **BoltStore**：第三方 ORM；
- **sky-bolt**：Skygear 派生项目；
- **bolt-mvcc**：第三方 MVCC 增强版（基于 BoltDB）；
- **skydb**：早期 non-B+tree 偏 kv-bolt 的 SkyGear 项目（archived）。

## 二十、风险与陷阱

| 陷阱 | 描述 | 缓解 |
|------|------|------|
| **Page Size 固定** | 创建后无法修改 page size | 创建时一次确定 |
| **Mapsize 不可动态扩展** | 与 LMDB 类似，文件大小限制 | 监控并最终搬迁 |
| **单写者** | 写入吞吐被单线程限制 | 跨实例 sharding / 应用层 fan-out |
| **COW 写放大** | B+tree 路径分裂时复制 | 选择 fill threshold + 批量提交 |
| **Long-running read tx** | 持有大量 page 不释放 | 应用层加超时 |
| **flock on NFS** | NFS 文件系统不支持 flock | 避免 NFS |
| **No Realtime 同步** | 多进程默认一写多读 | 协调应用层 |
| **write-write 冲突** | 不会自动 abort，应用层自行设计 | 用 `tx.OnCommit` hook |

## 二十一、与本项目的关联

BoltDB 思想对工程轨、Go 工具开发有借鉴价值：

- **借鉴点**：
  - **mmap + COW + 双缓冲 meta**：与 LMDB 同款，可作为 `engineering/src/db/` 的 WAL+Checkpoint 替代方案（适合 read-heavy 场景）；
  - **纯 Go 易移植**：`engineering/cmake/` 是 CMake，若要做工具层可考虑借鉴 BoltDB 设计（含 Go 端口）；
  - **Bucket 嵌套**：`engineering/include/db/storage.h` 中的 Table 系统可借鉴嵌套结构；
- **直接集成**：通过 cgo 调用 liblmdb 风格但用 BoltDB 的无 cgo ；
- **Go 工具栈**：未来 `engineering/tools/` 用 Go 写时，BoltDB 是首选 KV 持久化。

## 二十二、学习资源

- **官方文档**：仓库 README + Wiki（archived，可通过 Internet Archive 查看）；
- **bbolt 文档**：[https://github.com/etcd-io/bbolt](https://github.com/etcd-io/bbolt)；
- **Ben Johnson 博客**：[https://web.archive.org/web/2018/http://benbjohnson.com/](https://web.archive.org/web/2018/http://benbjohnson.com/)（BoltDB 系列 4 篇 blog post）；
- **GopherAcademy 演讲**：BoltDB 设计与实现；
- **源码注释**：`db.go` 顶部「Getting Started」+ 各模块 godoc；
- **Keith Rarick BoltDB 分享**：dotGo 2014 演讲视频；
- **etcd-io/bbolt Wiki**：[https://github.com/etcd-io/bbolt/wiki](https://github.com/etcd-io/bbolt/wiki)；
- **Reddit r/golang**：BoltDB 应用案例；
- **HN 讨论**：BoltDB v1.0 发布时贴（2015）。

## 二十三、参考资料

1. Ben Johnson. *BoltDB Design Blog Series*. [https://web.archive.org/web/2018/http://benbjohnson.com/](https://web.archive.org/web/2018/http://benbjohnson.com/)
2. Comer, D. *The Ubiquitous B-Tree*. ACM Computing Surveys 11(2), 1979.
3. Chen et al. *etcd: A Distributed, Reliable Key-Value Store for the Linux Workload*. USENIX OSDI 2020.
4. BoltDB GitHub (archived): <https://github.com/boltdb/bolt>
5. etcd-io/bbolt 仓库: <https://github.com/etcd-io/bbolt>
6. Howard Chu. LMDB 文档 (BoltDB 设计哲学来源): <http://www.lmdb.tech>
7. Bernstein & Goodman. *Concurrency Control Algorithms*. MIT Press 1981.
8. Keith Rarick Boltdb 2014 dotGo talk.
9. Dragonboat / TiKV 借鉴 bbolt 的 storage 层源码参考.
10. Ben Johnson. *Bits of Go: BoltDB Internals*. 2016.
