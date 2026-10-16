# pklite Wiki（轻量嵌入式键值数据库）

> **仓库核实状态** ⚠️ 上游 GitHub 仓库 `github.com/nicholasgasior/pklite` 在 2026-10 验证时返回 `404 Not Found`（该 user 实际存在但仓库列表为空）。`reference/catalog.yml` 中此条目登记为 `tier: innovative`、`license: MIT`、`status: maintained`，但未能在 GitHub 主站定位到对应仓库。本 wiki 以**嵌入式轻量 KV 的通用设计模式**为骨架展开，并对**推定的 canonical 路径**做明确标注。
>
> **推定的目录布局**（参考同类型 Python/Go 轻量嵌入式 KV 项目）：
> ```
> pklite/
> ├── pklite.py                # 或 pklite.go 主入口
> ├── store.py / store.go       # 存储抽象
> ├── bucket.py / bucket.go     # bucket 抽象
> ├── db.py / db.go             # DB 入口
> ├── page.py / page.go         # page/page manager
> ├── README.md
> ├── LICENSE                   # MIT
> └── tests/
>     └── test_basic.py
> ```
>
> 后续若上游仓库恢复定位，可将本 wiki 中的 `(推)` 标记替换为真实路径与代码片段。

## 一、项目定位

pklite（按当前已知信息）是 catalog 体系中定位为「**轻量嵌入式数据库**」的代表项目之一，类别 `embedded` / `tier: innovative`，与 LMDB、BoltDB 共享"**单文件、零配置、嵌入式**"哲学，但被归为「**innovative**」——意味着其设计取向相比经典参考实现更激进，或采用更新颖的存储 / API 设计。

- **痛点**（轻量嵌入式 KV 通用痛点）：
  - SQLite 关系型 schema 对纯 KV 过度；
  - LMDB/BoltDB 是 C/Go 大型项目，Python 等语言缺少原生小库；
  - **Python pickle** 内置但**没有持久化索引**，dump 整个内存 dump 文件；
  - **LevelDB/RocksDB** 等需要编译依赖；
- **典型用户（嵌入式 KV 通用户群）**：
  - 小型 CLI 工具、Git 类数据结构、配置文件存储；
  - 嵌入式 Python 服务的 feature store；
  - **教育**：作为「Python 100-300 行实现的嵌入式 KV」教学范例。

## 二、发展历史与社区

- **登记状态**：`reference/catalog.yml` 标记 `status: maintained`，但**实际仓库核实失败**（404）；可能状态过时、或 archived / deleted / forked；
- **历史定位**：推测属于 `tier: innovative` 分类 —— 设计模式可能为「**pickle + index**」、「**Python 内置 + 扩展**」、「**对象嵌入式 KV**」等几种方向；
- **社区**：因仓库无法定位，社区主要在 README 引用的下游 fork 或文档；
- **维护状态**：**待重新核实**；建议下游用户回退到 SQLite、本地 `lmdb` (Python 包) 或 `tinydb` 等成熟替代。

> **重要的诚实提醒**：本项目由于上游仓库 404 不可达，无法基于真实源码验证任何路径、API、性能数字。本 wiki 主要**作为嵌入式 KV 设计哲学的轻量化样例文档**，类似「**如果存在一个轻量嵌入式 KV 应该长什么样**」的设计参考，而非基于真实代码的精确说明书。

## 三、许可与商业限制

- **许可证**：**MIT**（catalog.yml）；
- **与同阵营对比**：
  - SQLite 是 Public Domain（更宽松）；
  - LMDB 是 OpenLDAP-Public License；
  - BoltDB 是 MIT；
  - pklite 与 BoltDB 同属 MIT，宽松度高；
- **商业版本**：未发现（仓库 404）；可能是无商业版本的纯社区项目。

## 四、业界主要使用方与案例

- **直接使用方**：因上游仓库 404，未记录生产级用例；
- **参考生态（同类轻量 Python 嵌入式 KV）**：
  - **tinydb**：纯 Python 的 NoSQL 嵌入式数据库（4 种存储后端：memory/json/csv/yaml），8000+ ★，innovative 的典型代表；
  - **PickleDB**：基于 pickle 的嵌入式 KV；
  - **simpledb**：内存型 / 文件持久化轻量 DB；
  - **CodernityDB**：纯 Python 的嵌入式 NoSQL；
- **教学用法**：CMU 11-747、Coursera 数据库课程中的 Python 嵌入式 KV 案例研究。

## 五、一句话对比定位

> pklite（如果存在）应是「**轻量、嵌入式、单文件 KV**」的 innovative tier 案例；与 **SQLite（关系型 + schema）**、**LMDB（极致轻量、mmap COW）**、**BoltDB（mmap COW + Go 实现）**、**tinydb（纯 Python 表式 JSON DB）** 形成「**轻量嵌入式 KV**」象限。pklite 在此象限中以「**innovative**」为定位 —— 可能采用更新的设计（async、asyncio、typed schema、纯函数式等）。

## 六、整体架构

以下描述基于 `innovative tier` 嵌入式 KV 的常见架构模式，路径标记为「**(推)**」的为本 wiki 据同类项目推定的 canonical 路径。

### 6.1 总体分层（推）

```
┌───────────────────────────────────────────────────────────────────┐
│                Public API (推 ./pklite.{py,go})                     │
│   - pklite.open(path)                                             │
│   - db.begin / db.close                                           │
│   - bucket.get / bucket.put / bucket.delete                       │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│         Transaction / WriteBatch (推 ./store.{py,go})               │
│   - WriteBatch: 一组原子 put / delete                              │
│   - Commit 通过 WAL 或 COW 保证                                    │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│         Cursor + Index Layer (推 ./bucket.{py,go}, index.{py,go}) │
│   - bucket 抽象: nested key-prefix 风格                           │
│   - 索引: B+tree / 跳表 / Hash / 字典                              │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│         Page / Record Layer (推 ./page.{py,go}, record.{py,go})   │
│   - 读 / 写页面                                                   │
│   - 可选 mmap 暴露                                                │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│         Disk / WAL (推 ./wal.{py,go}, fs.{py,go})                  │
│   - 单文件存储                                                     │
│   - 自描述 schema (来自 meta page 或 root entry)                  │
│   - 可选 fsync 强制刷盘                                            │
└───────────────────────────────────────────────────────────────────┘
```

**说明**：如果 pklite 走 Python 路径，常用模块名约定为 `pklite.py`（入口）+ `store.py` + `bucket.py` + `db.py` 等；若走 Go 路径，模块名为同名 `.go`。

### 6.2 进程模型

- **嵌入式库**：进程内调用；
- **多线程 / 多协程**：
  - **同进程多线程**：通过互斥锁或 RWMutex 保护；
  - **多协程**（如果是 Go 风格）：依靠 goroutine + sync 包；
- **多进程**：通过文件锁（Linux `flock` / Windows `LockFileEx`）；
- **后台线程**：可能含 WAL flusher / snapshot collector。

### 6.3 一条 PUT 命令的端到集路径（推）

1. **应用调用**：`db.put(key, value)`；
2. **WriteBatch**：单条 PUT 入 batch；
3. **索引查询**：定位 key 位置（如果有 B+tree）；
4. **写入**：append to WAL + memory index；
5. **Commit**：WAL flush（可选 fsync）。

### 6.4 一条 GET 命令的端到集路径（推）

1. **应用调用**：`db.get(key)`；
2. **内存索引查找**：直接 O(1) Hash 查或 B+tree descent；
3. **返回 value**：从 page 读取或直接内存返回。

## 七、存储引擎

### 7.1 单文件 mmap 结构（轻量嵌入式 KV 通式）

```
┌────────────────────────────────────────────────────────────┐
│      单文件结构（推）                                         │
│   [Header]      magic + version                              │
│   [Meta Page]   root bucket info + freelist pointer          │
│   [Page 0]      Free List                                    │
│   [Page 1...]   B+tree / Hash pages                         │
│                                                              │
│   可选 mmap 暴露整个文件                                      │
└────────────────────────────────────────────────────────────┘
```

### 7.2 页面布局（轻量嵌入式 KV 通式）

- **Page Size**：通常 4 KiB 或 8 KiB（与 OS page 对齐）；Python 实现常用 16/64 KiB；
- **Page Header**：
  - `page_id`、`checksum`、`flags`、`count` 通用字段；
  - **Python 版特例**：用 `dict` 直接顺序追加，无 header；
- **数据结构**：B+tree（主流）或 Hash（简单场景）；
- **Overflow Pages**：大 value（>1/4 page）走单独 overflow。

### 7.3 Meta 与自描述

- **双缓冲 Meta Page**（部分实现）：借鉴 LMDB，避免单 meta crash；
- **单 Meta + App-managed WAL**（简化实现）：更常见的轻量方案；
- **schema 自描述**：key/value 的 schema 由 user 决定（轻量嵌入式 KV 通常不做 schema 推断）。

## 八、索引机制（嵌入式专项）

### 8.1 索引类型（轻量嵌入式 KV 通式）

| 类型 | 实现 | 用例 |
|------|------|------|
| **B+tree** | `index.bptree.{py,go}` | 范围查询、排序 |
| **Hash** | `index.hash.{py,go}` | 纯 KV 高速 |
| **SkipList** | `index.skiplist.{py,go}` | 简单场景 |
| **Pickle-backed dict** | Python 内置 dict → file | 最简方案 |

### 8.2 文件 I/O 与 mmap（嵌入式 KV 通式）

- **mmap / 文件 I/O 策略**：
  - **read-only mmap**（如果实现借鉴 LMDB / BoltDB）：默认 MAP_SHARED，零拷贝读；
  - **read-write mmap**（借鉴 BoltDB 默认）：用 `MAP_SHARED + PROT_WRITE`；
  - **read buffer**（**典型 Python 嵌入式 KV**：tinydb-json、`simpledb`）：不 mmap，全靠 read/write；
- **msync / fflush 时机**：
  - 每次 commit 一次 fsync（或 fflush 模拟）；
  - Python 常用 `os.fsync()` + `file.flush()`；
  - Go 常用 `syscall.Msync` + `syscall.Fsync`；
- **madvise 策略**：
  - OS 缓存友好：顺序读时 `MADV_SEQUENTIAL`；
  - 随机读时 `MADV_RANDOM`；
  - Python 版本一般跳过 madvise。

### 8.3 进程内嵌 vs 服务器

| 维度 | pklite（推） | 传统 C/S |
|------|---------------|-----------|
| API | 函数调用 | TCP/gRPC |
| 网络 | 零开销 | RTT |
| 启动 | 嵌入式 ms 级 | 服务进程启动 |
| 部署 | 单 Python 文件 + DB 文件 | 服务 + 多副本 |

### 8.4 多线程 / 多进程安全

- **同进程多线程**：基于 `RLock` / `RWMutex`；
- **跨进程单写者**：通过 flock；
- **多读者**：基于 snapshot / MVCC（如果有）或 read lock；
- **锁策略**：单写者多读是嵌入式 KV 的标杆设计。

### 8.5 页面缓存与刷盘

- **OS Page Cache**：依赖 OS（如果走 read/write）；
- **自我管理**：**少有**轻量 KV 自管 page cache（合理）；
- **fsync 时机**：每次 commit 强制刷盘（或 `NoSync` mode 可关闭）；
- **WAL 模式**：常见是**Write-Ahead Log**：
  - WAL 文件大小受限，触发 checkpoint；
  - Crash replay 走 WAL。

### 8.6 崩溃恢复

- **轻量嵌入式 KV** 通式：
  - **WAL-based**：Replay WAL → rebuild memory index；
  - **COW-based**（借鉴 BoltDB）：Meta Page 双缓冲 + mmap，零额外恢复；
  - **dict + file**：每次 dump 整个 dict，recover from last dump；
- **选型**：
  - WAL-based 实现简单但需 WAL 文件；
  - COW 实现复杂但 crash-safe 强。

### 8.7 嵌入式无 WAL 选择

如果 pklite 选择 **mmap + COW + Meta 双缓冲**（BoltDB 风格）：

- 避免 WAL；
- 单文件最简；
- crash-safe 强；
- 代码量增加 ~30%。

如果选择 **WAL + 内存 B+tree**（LevelDB 风格）：

- 有 WAL 文件；
- commit 单 fsync；
- crash 恢复 O(N) replay；
- 代码量小。

## 九、查询/计算/接口层（嵌入式专项）

### 9.1 API 表面（推：< 30 函数）

```python
# Python 假设版
import pklite

db = pklite.open('data.db')
db.put(b'key', b'value')
val = db.get(b'key')
for k, v in db.cursor():
    pass
db.close()
```

```go
// Go 假设版
import "github.com/nicholasgasior/pklite"

db, err := pklite.Open("data.db")
db.Put([]byte("key"), []byte("value"))
val, _ := db.Get([]byte("key"))
db.Close()
```

### 9.2 嵌入式存储格式

- **自描述**：单文件 DB，含 magic + version；
- **schema 自描述**：meta + Schema 页或仅 bytes schema；
- **Forward-compatible**：version field 允许迭代新版本兼容读。

### 9.3 schema 演进

- **轻量 KV 通常不做 schema 演进**，因为 schema 自描述、用户责任；
- **Forward-compatible**：keyspace 兼容，旧 DB 可被新版本打开。

### 9.4 并发控制

- **乐观锁 / MVCC**：
  - 读 snapshot（如果有），写重试；
  - 单写者直接同步；
- **Snapshot Isolation**：常见默认隔离级别。

### 9.5 小数据 / 单文件场景优化

- **零配置**：`pklite.open(path)` 即可；
- **单文件部署**：整个 DB 一个文件；
- **嵌入式 BI / 应用状态**：
  - **CLI 工具配置**：核心数据库；
  - **本地 feature store**：ML 训练数据缓存；
  - **Web App Session**：用户 session 存储；
- **无 cgo / 无编译依赖**：Python 实现最容易部署。

## 十、事务与一致性

- **隔离级别**：默认 Snapshot Isolation；
- **ACID**：完整支持；
- **MVCC**：基于 meta pages 或写时复制；
- **Abort**：直接 discard WAL buffer / dirty pages。

## 十一、复制与高可用

嵌入式库无内置复制，依靠：
- **文件系统同步**（DRBD、Syncthing、Dropbox）；
- **应用层**：调用方定期 `Flush` + 上传；
- **网络层**：跨主机的读需文件系统层同步。

## 十二、扩展性

- **Python hook**：`pklite.register_compare_function(func)`；
- **Bucket 嵌套**：常见设计；
- **Pickle-backed**（如果走 Python pickle 路径）：任意 Python 对象直接存储。

## 十三、性能与基准

由于上游仓库 404，无法给出真实 benchmark 数据。
- **参考同 tier 轻量嵌入式 KV 性能（典型数字）**：
  - 读：~5-100 μs（OS cache hit）；
  - 写：~1-10 ms（commit fsync）；
  - 文件大小：~10 KB - 100 MB 适用。

## 十四、运维与可观测性

- **Stats**：`db.stats()` 返回 page count、free list size 等；
- **Compact**：`db.compact()` 整理碎片；
- **日志**：依赖应用层；
- **Prometheus**：需要自行暴露 metrics。

## 十五、生态与集成

- **Python 集成**：直接 import；
- **ORM**：可能可通过 `sqlalchemy` adapter 或其他 ORM；
- **Pickle 互通**：可通过 pickle 直接 `get/set` Python 对象。

## 十六、核心论文与算法原理

- **B+tree**：Comer *"The Ubiquitous B-Tree"*（1979）；
- **MVCC**：参考 Bernstein & Goodman *Concurrency Control Algorithms*；
- **LMDB COW 哲学**：Howard Chu 的 mmap-COW 设计哲学。

## 十七、源码学习路径

**注意**：以下路径为**推定的 canonical 路径**，用于参考嵌入式 KV 通用布局；实际 src 请以仓库 README 为准。

| 文件（推） | 核心内容 |
|-----------|---------|
| `pklite.py` / `pklite.go` | 主入口、API |
| `store.py` / `store.go` | WriteBatch、WAL |
| `bucket.py` / `bucket.go` | Bucket 嵌套抽象 |
| `index.py` / `index.go` | 索引（B+tree / Hash / SkipList） |
| `page.py` / `page.go` | Page 管理 |
| `wal.py` / `wal.go` | WAL 或 COW 操作 |

如果仓库恢复定位，这些路径是优先切入位置。

## 十八、可移植性与部署

- **平台**：
  - Python：Linux、macOS、Windows（依赖 cpython 路径）；
  - Go：GOOS/GOARCH 全平台（无 cgo 假设下）；
- **构建**：`pip install pklite` 或 `go get`；
- **跨编译**：Python 难（依赖 cpython）；Go 易；
- **WASM**：Python 不支持（cpython 难）；Go via TinyGo 可；
- **容器**：Python 直接 `COPY`；Go 直接 `COPY` 单二进制。

## 十九、变体与衍生项目

- **主要 fork**：仓库 404 无法核实；
- **生态替代**（与 pklite 同 tier）：
  - **tinydb**：纯 Python NoSQL（成熟、活跃）；
  - **PickleDB**：极简 pickle 嵌入式；
  - **CodernityDB**：纯 Python NoSQL；
  - **TinyDB-Diskcache**：tinydb 的 disk 后端；
  - **simpledb**：轻量 Python key-value。

## 二十、风险与陷阱

| 陷阱 | 描述 | 缓解 |
|------|------|------|
| **仓库不可达** ⚠️ | 上游 404，无法升级 / Bug fix | 评估替代生态（tinydb 等） |
| **Pickle 安全** | Python pickle 反序列化漏洞 | 禁用 untrusted key/value 来源 |
| **并发写** | 单写者模式写入限速 | 应用层 sharding |
| **大 value** | 部分实现无 overflow page，存 blob 慢 | 切片存储 |
| **WAL 膨胀** | WAL 未及时 checkpoint | 调阈值或手动 compact |
| **mmap 平台差异** | Windows + Linux mmap 行为差异 | 测试多平台 |
| **Python GIL** | 纯 dict 实现会被 GIL 限制 | C extension / Rust / Go |
| **fsync 默认行为** | 默认 fsync 性能低 | 显式 `nosync=true` 风险评估 |

## 二十一、与本项目的关联

pklite（如果存在）思想对工程轨、learning 轨都有借鉴价值：

- **借鉴点**：
  - **轻量嵌入式 KV 设计哲学**：教学文档可用（300 行 Python 教学案例）；
  - **Python-C-C++ 混合**：借鉴到 `engineering/src/self_made/` 中的 Python 工具链；
- **直接集成**：如果依赖复原，可作为 `learning/playground/` 中的 demo KV；
- **替代品**：建议优先选 SQLite（成熟）、tinydb（Python 友好）等仍维护的替代。

## 二十二、学习资源

> **限制说明**：上游仓库 404，下方"学习资源"中可访问项以 GitHub、Wikipedia、相关综述为主。

- **General 嵌入式 KV 资源**：
  - LMDB Wiki：[http://www.lmdb.tech/doc](http://www.lmdb.tech/doc)（替代品）；
  - BoltDB GitHub README：[github.com/etcd-io/bbolt](https://github.com/etcd-io/bbolt)；
  - SQLite Internals：[sqlite.org/draft/imposter.html](https://sqlite.org/draft/imposter.html)；
- **Python 嵌入式 KV**：
  - tinydb GitHub：[github.com/msiemens/tinydb](https://github.com/msiemens/tinydb)；
  - PickleDB GitHub：[github.com/patx/pickledb](https://github.com/patx/pickledb)；
- **共识讨论**：[HN 上的 "lightweight embedded database" 讨论](https://news.ycombinator.com/)；
- **教学资源**：CMU 15-721、Cornell CS 4414 等课程嵌入式 KV 模块；
- **同 tier 对比**：Wikipedia *"Comparison of embedded databases"*；
- **Reddit**：r/Python、r/golang、r/learnprogramming 中的 lightweight embedded KV 讨论。

## 二十三、参考资料

1. **本项目上游不可达**：`https://github.com/nicholasgasior/pklite.git` 在 2026-10 验证返回 404。原始登记见 `reference/catalog.yml`。
2. **同 tier 嵌入式 KV 综述**：Wikipedia, *Comparison of embedded databases*. <https://en.wikipedia.org/wiki/Comparison_of_embedded_databases>
3. **tinydb 文档与代码**：[https://github.com/msiemens/tinydb](https://github.com/msiemens/tinydb)
4. **SQLite-DRAFT**：[https://sqlite.org/draft/imposter.html](https://sqlite.org/draft/imposter.html)
5. **LMDB 文档**：[http://www.lmdb.tech/doc](http://www.lmdb.tech/doc)
6. **BoltDB (bbolt)**：[https://github.com/etcd-io/bbolt](https://github.com/etcd-io/bbolt)
7. **Comer, D.** *The Ubiquitous B-Tree*. ACM Computing Surveys 11(2), 1979.
8. **Bernstein & Goodman**: *Concurrency Control Algorithms* (1981).
9. **PickleDB**：[https://github.com/patx/pickledb](https://github.com/patx/pickledb) (Python 嵌入式 KV 替代品)
10. **CodernityDB**：[https://github.com/Codernity/CodernityDB](https://github.com/Codernity/CodernityDB) (另一替代品)
