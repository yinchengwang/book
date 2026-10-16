# LMDB Wiki（Lightning Memory-Mapped Database 嵌入式 KV）

> 参考版本：LMDB 主线（LMDB 0.9.30+ 系列，作者 Howard Chu / Symas Corp.，OpenLDAP 出品，本 wiki 以 `mdb.c` 主线行为为基准）。

## 一、项目定位

LMDB（Lightning Memory-Mapped Database，闪电内存映射数据库）是 Symas 公司 / OpenLDAP 团队开发的一款**极致轻量、单文件、内存映射、零配置**的嵌入式 B+tree 键值数据库，代码量约 6000 行 C，**全部 API 不到 50 个**，却能提供 ACID 事务、MVCC 单写多读、跨进程共享读。

- **痛点**：
  - SQLite 的 schema 约束对纯 KV 太重；
  - LevelDB/RocksDB 的 compaction 行为复杂且影响 read latency 稳定性；
  - BerkeleyDB 商业许可、双重授权限制了嵌入式使用；
  - 嵌入场景需要「打开即用」、零配置、跨进程安全读；
- **典型用户**：OpenLDAP 后端（slapd-mdb）、Unbound DNS 缓存、PostgreSQL（早期 PG 嵌入式默认）、KDB+/JetBrains YouTrack、RocksDB / BoltDB 设计参考；C 教程推荐作为「mmap + B+tree 事务」教学样本。

## 二、发展历史与社区

- **起源**：2011 年初 Howard Chu（OpenLDAP 核心维护者）在 `libmdb`（Sybase MDB 的 Linux 移植）基础上重写为 `liblmdb`，目标为 OpenLDAP 后端；
- **里程碑**：
  - 2011-08：随 OpenLDAP 2.4.26 首次发布；
  - 2013：v0.9.x 引入 doubling meta pages；
  - 2015-2017：v0.9.21-23 引入 dup-sort fix、`MDB_NOTLS`、`MDB_NORDAHEAD` 等选项；
  - 2020-2022：v0.9.29-30 改进 meta 页稳定、FreeBSD bug 修复；
  - 2023-2024：与 OpenLDAP 同步演进，0.9.31+ 主要修 bug；
- **维护状态**：低频维护（一年 5-10 个 commit），代码稳定、功能冻结；社区靠 Symas Corp 商业支持；
- **社区**：OpenLDAP mailing list 邮件流；GitHub `LMDB/lmdb` 仓库。

## 三、许可与商业限制

- **许可证**：**OpenLDAP Public License**（基于 BSD-3-Clause）；
  - 中文：与 BSD-3 类似宽松、保留版权即可商用、修改、分发；
  - 不与 GPL 冲突（有人称之为「企业友好 BSD」）；
- **商业版本**：无；Symas 提供商业支持与咨询服务；
- **MPL-1.1 类比**：常被比作 OpenLDAP-Public License，介于 BSD 与 Apache 之间。

## 四、业界主要使用方与案例

- **OpenLDAP**：`slapd-mdb` 后端默认存储（v2.4.26+）；
- **Unbound DNS Resolver**：缓存后端；
- **Knot DNS**：zone 数据存储；
- **Samba DC**：复制元数据 backends（部分模块）；
- **JetBrains YouTrack**：早期版本；
- **PowerDNS**：部分组件；
- **PostgreSQL**：早期教程《使用 LMDB 作为 PostgreSQL 扩展数据存储》案例；
- **RocksDB** 设计参考：mdb.c 的 FreeDB / Overflow IDs 等设计影响了 RocksDB；
- **教学**：CMU 15-721、UC Irvine《Database Internals》参考文献。

## 五、一句话对比定位

> LMDB 是「**单文件、mmap 暴露、COW 无 WAL、单写多读**」的极致轻量 KV 引擎；与 **LevelDB（LSM、compaction 抖动）**、**RocksDB（重功能、配置繁琐）**、**SQLite（关系型、有 schema）**、**BerkeleyDB（GPL exception + 商业混合）** 形成「**纯 KV 嵌入式**」象限。LMDB 适合读多写少的嵌入式场景（LDAP、DNS、配置缓存）。

## 六、整体架构

LMDB 是 **一个 .c 文件 + 一个 .h 文件** 的极限架构。整个 mdb.c 约 6000 行，核心数据结构和模块高度内聚。

### 6.1 总体分层

```
┌──────────────────────────────────────────────────────────────────┐
│                Public API (libraries/liblmdb/lmdb.h)              │
│   mdb_env_open / mdb_env_close                                   │
│   mdb_txn_begin / mdb_txn_commit / mdb_txn_abort                  │
│   mdb_put / mdb_get / mdb_del / mdb_cursor_open                   │
│   mdb_dbi_open / mdb_dbi_close                                   │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│           Transaction Layer (mdb.c: mdb_txn_*)                     │
│   - 单写事务 (MDB_txn) = 一组 page id + 操作                     │
│   - 读事务 (read-only) 持有 MDB_READ_TX_ID + 0 page list          │
│   - 写事务 = 单写者模式 (mutex + 串行化)                          │
│   - Commit 时通过 Copy-on-Write 修改 meta page (双缓冲)         │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│           Cursor + B+tree Layer (mdb.c: mdb_cursor_*)             │
│   - MDB_cursor: 当前 page + 当前 slot index                       │
│   - B+tree 节点 (internal / leaf / overflow / dup)               │
│   - Cursor move_next/move_prev / search                          │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│         Page Cache Layer (lmdb 通过 mmap 完成)                   │
│   - 整个数据库 mmap 为线性内存                                    │
│   - page_id ↔ 偏移计算 = page_id * PAGESIZE + BASE                │
│   - dirty page 通过 mdb_page_flush + msync 刷盘                  │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│         Meta Page 双缓冲 (mdb.c: mdb_env_write_meta)              │
│   - meta[0] / meta[1]: 当前快照 in-use, 另一个待切换             │
│   - txn commit 阶段原子切换 + msync                              │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│         OS mmap / VFS                                            │
│   - mmap() MAP_SHARED 暴露整个 DB 文件                           │
│   - msync(MS_SYNC) 在 meta 切换时强制刷盘                        │
│   - posix_fallocate 扩展文件大小                                  │
└──────────────────────────────────────────────────────────────────┘
```

源码入口：
- API 头：`libraries/liblmdb/lmdb.h`
- 实现：`libraries/liblmdb/mdb.c`（核心 ~6000 行）
- 工具集：`libraries/liblmdb/midl.c` (`mdb_stat`、`mdb_copy`、`mdb_dump`、`mdb_load`)

### 6.2 进程模型

LMDB 是**嵌入式库**，无独立进程：

- **同一进程多线程**：
  - 写线程独占（通过 `MDB_env.me_txkey` TLS / mutex）；
  - 多个读线程并发（META 切换时不阻塞读）；
- **多进程**：
  - 写者单进程（跨进程写互斥，需要 OS file lock）；
  - 读可多进程同时打开（基于 mmap 默认共享）；
- **后台线程**：无（所有 IO 同步）；
- **同步原语**：Linux 上 `MUTEX=` sysv sem；macOS 上 `fcntl` + flock。

### 6.3 一条 PUT 命令的端到端路径

1. **应用调用**：`mdb_put(txn, dbi, &key, &data, flags=0)`；
2. **dbi 校验**：`mdb_cursor_set0` 前检查 dbi->md_db_toggle / dbi->md_db_me 使用准备；
3. **加写锁**：`mdb_txn_begin` 阶段已获取 (mutex or single-writer flag)；
4. **Search cursor**：`mdb_cursor_set` 在 B+tree 查找插入位置；
5. **空间分配**：`mdb_page_new(mc->mc_txn, 1 /*leaf*/)` 从 free list / spill / file alloc 取页；
6. **COW 写页**：把旧 page 复制到新 page，修改新 page；写溢出页追加新 page；
7. **Cursor 更新**：指向新 page，调整 sibling pointer；
8. **返回**：`MDB_SUCCESS`。

### 6.4 一条 GET 命令的端到端路径

1. **应用调用**：`mdb_get(txn, dbi, &key, &data)`；
2. **读事务 snapshot**：`txn->mt_txid` + `mdb_find_oldest` 判断快照可见；
3. **B+tree descent**：`mdb_page_search_root` → `mdb_page_get`；
4. **二分查找 slot**：`mdb_node_search`；
5. **MVCC 过滤**：`mdb_xcursor_init` + dup-sort 检查；
6. **返回 data**：从 `MDB_NODE_DATA(page, slot)` 取值指针（甚至可能是 mmap 中直接的内存指针）。

### 6.5 一条 COMMIT 的端到端路径

1. **应用调用**：`mdb_txn_commit(txn)`；
2. **Free List 合并**：把新释放的 page 推进 `dirty list`；
3. **Meta 写入**：写 `MDB_meta` 到 `meta[0]` 或 `meta[1]`；
4. **msync**：`mdb_env_sync0`(MS_SYNC) 强制刷盘；
5. **Meta Toggle**：`me_txn->mt_txnid++` + atomic flip；
6. **释放写锁**：unlock mutex；
7. **返回**：`MDB_SUCCESS`。

## 七、存储引擎

### 7.1 单文件 mmap 结构

```
┌──────────────────────────────────────────────────────────────┐
│               File: data.mdb + lock.mdb                       │
│                                                              │
│   [Page 0]    Meta Page (snapshot 0/1)                        │
│   [Page 1]    Root page of free DBIs                          │
│   [Page 2]    Free page (常用 TXN/FREE 头)                   │
│   [Page 3]    Root Page of main DB (Default DB)              │
│   [Page 4..N] B+tree internal / leaf / overflow pages         │
│                                                              │
│   - 整个文件 mmap 到进程虚拟地址空间                          │
│   - 读直接返回内存指针，零拷贝                                │
│   - 写通过 Copy-on-Write (COW) 复制 page                      │
└──────────────────────────────────────────────────────────────┘
```

### 7.2 Page Layout

```cpp
// 来自 mdb.c: MDB_PAGE
typedef struct MDB_PAGE {
    uint64_t   mp_pgno;      // page id
    uint16_t   mp_pad;       // 保留
    uint16_t   mp_flags;     // P_LEAF / P_DIRTY / ...
    union {
        struct MDB_branch_page { indx_t   mp_lower; indx_t   mp_upper; } pb;
        struct MDB_leaf_page  { indx_t   mp_lower; indx_t   mp_upper; } pb;
        struct MDB_overflow  { uint64_t  ovpg_pgno; size_t  ovpg_sz; } ov;
    } mp_node;
    // Page data: bytes[mp_pgno..mp_upper]
} MDB_PAGE;
```

- **Page Size**：默认 4096 bytes（`mdb_env_set_mapsize` 控制总大小）；
- **Node Layout**：`MDB_node` 含 `{ key, data, flag }`；多 key 单 page 紧凑存放；
- **Overflow Page**：大于 1/4 page_size 的 value 单独存到 overflow page，leaf 节点只存指针；
- **Sub-page**：dup-sort 的多值通过 sub-page 嵌入。

### 7.3 Meta Page 双缓冲

```cpp
typedef struct MDB_meta {
    uint32_t mm_magic;        // magic number
    uint32_t mm_version;      // LMDB version
    void    *mm_address;      // mmap base addr
    size_t   mm_mapsize;      // 映射大小
    size_t   mm_dbsize;       // 已使用大小
    pgno_t   mm_last_pgno;    // 最大 page id + 1
    uint32_t mm_txnid;        // 当前事务 ID
    MDB_db   mm_dbs[2];       // main DB + free DB root info
    // ...
} MDB_meta;
```

- `meta[0]` 与 `meta[1]` 互为备份；
- Commit 写入「非当前 in-use」的那个，原子切换 `me_txnid`；
- 任意时刻至少一个 meta 是上一次 commit 的稳定快照；
- 启动时扫两个 meta，选 `me_txnid` 较大的生效。

### 7.4 FreeDB 与管理

- `meta->mm_dbs[FREE_DB]`：跟踪所有 free page；
- 由 `mdb_freelist_save` + `mdb_frelist_restore` 维护；
- 保证崩溃恢复时 free list 不漏不放。

## 八、索引机制（嵌入式专项）

### 8.1 B+tree 唯一索引

LMDB **只有 B+tree 一种索引**，没有 hash/art 等其他模式。

- **结构**：内部节点（branch）和叶子节点（leaf），顶层为 Root；
- **Key Ordering**：默认 lexicographic（lexicographical byte compare），可调用 `mdb_env_set_compare` + `mdb_set_compare` 自定义比较函数；
- **MVCC 多版本**：每个 B+tree page 保留的指针指向 page id，通过 page id 间接访问避免 COW 的代价。

### 8.2 mmap / 文件 I/O 策略

LMDB 是**唯一以 mmap 为首要 IO 模型**的主流嵌入式 KV：

- **read-only mmap**：默认 `MAP_SHARED`；
  - **零拷贝读**：返回 mmap 中字节指针，无需 `read()` 系统调用；
  - **多进程共享读**：多个进程 mmap 同一个文件，OS page cache 自然共享，无需 IPC；
- **read-write mmap**：通过 `MDB_WRITEMAP` flag 启用，`PROT_WRITE`；
  - 直接写 mmap 内存；
  - `MS_ASYNC` 异步 / `MS_SYNC` 同步刷盘；
- **madvise 策略**：
  - 默认 `MADV_RANDOM`（逐步扫描）；
  - 顺序扫描时设 `MADV_SEQUENTIAL`；
  - 大量访问前 `MADV_WILLNEED` 预读；
- **msync 时机**：
  - 默认每次 Commit `MS_SYNC`；
  - 用 `MDB_NOSYNC` flag 可关闭（性能 ↑，安全性 ↓）；
  - 用 `MDB_MAPASYNC` 异步刷盘 + 后台 sync；
- **POSIX 扩展**：`posix_fallocate` 预分配文件大小以避免突发 I/O。

### 8.3 进程内嵌模式 vs 服务器模式

| 维度 | LMDB（嵌入式） | 传统 C/S |
|------|-----------------|-----------|
| API | 函数调用 `mdb_*` 直接执行 | TCP/gRPC + 网络序列化 |
| 网络开销 | **零** | 满载 RTT + 协议解析 |
| 跨进程共享 | mmap 自然支持 | RPC |
| 部署 | 单 .c .h + 单一 .mdb 文件 | 服务器进程 + 多副本 |

### 8.4 多线程 / 多进程安全

LMDB 的并发模型是其核心设计特征。

- **单写者**（single writer）：
  - **同进程多线程**：通过 `MUTEX`（pthread mutex）保护，需配置 `MDB_NOTLS=0`；
  - **跨进程单写者**：跨进程写互斥需要 OS 支持（Linux syscall flock）；
  - **写串行化**：同一时刻只有一个写事务活着；
- **多读者**（multi reader）：
  - 同进程读事务**无锁并发**（基于 `me_numreaders` + TLS slot）；
  - 跨进程读 **无需任何锁**（基于 mmap，OS page fault 处理）；
  - 读事务 snapshot 由 `mt_txnid` 决定；
- **死锁检测**：
  - 防止同一线程重入写事务；
  - 防止 prepare 写事务并发 commit；
  - `mdb_txn_renew` / `mdb_txn_reset` 管理长事务。

### 8.5 页面缓存与刷盘

- **OS Page Cache**：依赖 OS 自动换入（首次访问触发 page fault，OS 调页）；
- **自管**：**无**（与 DuckDB 截然不同），LMDB 完全相信 OS；
- **fsync 时机**：
  - 默认每次 Commit `MS_SYNC`，确保 1 个完整事务 = 1 次 fsync；
  - 写入路径 = `mdb_env_sync0(force=1)`，原子切换 meta 后调用 `fdatasync(fd)`；
- **dirty page tracking**：通过 `MDB_PAGE_PDIRTY` 标记 dirty page；
- **write amplification**：仅 1（每次 Commit 把 dirty pages 一次性写盘）。

### 8.6 崩溃恢复

LMDB 的崩溃恢复模型极其简洁，因为**没有传统意义的 WAL**：

- **机制**：Copy-on-Write (COW) + Meta Page 双缓冲；
- **崩溃后**：
  - 启动时读 `meta[0]` 和 `meta[1]`，选 `mt_txnid` 较大的；
  - 数据自然恢复到最后一次成功的 Commit；
  - Last Dirty Pages 可能未落盘但也无副作用（未 commit 等于不存在）；
- **优势**：
  - 零额外恢复代码；
  - 无 WAL 文件；
  - 每次 Commit 仅一次 fsync（理论最低开销）；
- **代价**：
  - 写时复制成本：B+tree 内层修改会复制到顶部祖先链；
  - 数据文件膨胀：未重用 free page 时延后回收。

### 8.7 嵌入式无 WAL

| 引擎 | 持久化机制 | 是否需要 WAL |
|------|-------------|---------------|
| LevelDB / RocksDB | LSM + WAL | 是 |
| BoltDB | mmap + COW + Free Page 重用 | 否（类似 LMDB） |
| LMDB | mmap + COW + Meta 双缓冲 | 否 |
| SQLite | B+tree + Journal/WAL | 是 |
| DuckDB | Row Group + WAL + Checkpoint | 是 |

LMDB 与 BoltDB 共享「**mmap + COW 实现 ACID**」哲学，但 BoltDB（Go）有显式 Free Page / Page Allocation，LMDB（C）则更接近 Linux Berkeley mmap 极致设计。

## 九、查询/计算/接口层（嵌入式专项）

### 9.1 API 表面极简（< 50 函数）

```c
// Environment
int mdb_env_create(MDB_env **env);
int mdb_env_open(MDB_env *env, const char *path, unsigned int flags, mdb_mode_t mode);
int mdb_env_close(MDB_env *env);
int mdb_env_set_mapsize(MDB_env *env, size_t size);
int mdb_env_set_maxdbs(MDB_env *env, MDB_dbi dbs);
int mdb_env_set_maxreaders(MDB_env *env, unsigned int readers);
int mdb_env_sync(MDB_env *env, int force);
mdb_size_t mdb_env_get_mapsize(MDB_env *env);

// Transaction
int mdb_txn_begin(MDB_env *env, MDB_txn *parent, unsigned int flags, MDB_txn **txn);
int mdb_txn_commit(MDB_txn *txn);
int mdb_txn_abort(MDB_txn *txn);
mdb_txnid_t mdb_txn_id(MDB_txn *txn);

// DBI
int mdb_dbi_open(MDB_txn *txn, const char *name, unsigned int flags, MDB_dbi *dbi);
int mdb_dbi_flags(MDB_txn *txn, MDB_dbi dbi, unsigned int *flags);
int mdb_dbi_close(MDB_env *env, MDB_dbi dbi);

// KV
int mdb_put(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data, unsigned int flags);
int mdb_get(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data);
int mdb_del(MDB_txn *txn, MDB_dbi dbi, MDB_val *key, MDB_val *data);

// Cursor
int mdb_cursor_open(MDB_txn *txn, MDB_dbi dbi, MDB_cursor **cur);
int mdb_cursor_get(MDB_cursor *cur, MDB_val *key, MDB_val *data, MDB_cursor_op op);
int mdb_cursor_put(MDB_cursor *cur, MDB_val *key, MDB_val *data, unsigned int flags);
int mdb_cursor_del(MDB_cursor *cur);
int mdb_cursor_close(MDB_cursor *cur);

// Misc
int mdb_set_compare(MDB_txn *txn, MDB_dbi dbi, MDB_cmp_func *cmp);
int mdb_set_dupsort(MDB_txn *txn, MDB_dbi dbi, MDB_cmp_func *cmp);
```

### 9.2 嵌入式存储格式（Self-describing）

- **单文件**：整个 DB 一个 `.mdb` 文件；
- **自描述**：page header 中含 magic + version，启动时检查；
- **Forward-compatible 演进**：当前 LMDB 几乎冻结；通过 `MDB_db.md_format` 字段加版本信息；
- **跨平台**：x86 / ARM / RISC-V / ppc 等都支持（前提是对齐 + endian 小端）。

### 9.3 嵌入式序列化（自管理 schema）

- **DBS**: `main DB`（用户 KV）+ `named DBs`（多个 logical table）+ `free DB`；
- **schema 由调用方约定**（用户决定 key/value 字节顺序）：
  - OpenLDAP 的 LDIF（dn → entry）；
  - DNS 资源记录的 wireformat；
  - 文本格式（JSON、protobuf、MessagePack）；
- 无内置 schema 校验：用户责任。

### 9.4 并发控制

- **Multi-Version Concurrency Control (MVCC)**：
  - 通过 `mt_txnid` + meta pages 版本切换；
  - **读事务** 永远看到 commit 时一致的 snapshot；
  - **写事务** 串行化；
- **Optimistic Concurrency**：
  - 写 commit 时检查 `meta` 切换未发生冲突；
  - LMDB 简化：单写者 + COW，避免了大部分冲突场景；
- **Isolation Level**：Snapshot Isolation。

### 9.5 小数据 / 单文件场景优化

- **零配置**：`mdb_env_open(path, 0, mode)` 即可；
- **单文件部署**：整个 DB 一个文件 + 1 lock 文件（`lock.mdb`）；
- **嵌入式 LDAP / DNS / Cache**：标准用法；
- **嵌入式 BI / 科学计算**：LMDB 不擅长（无 SQL），但在 ML feature store 中可作为底层 KV；
- **启动速度**：~1ms（仅一次 mmap，无 schema bootstrap 流程）。

## 十、事务与一致性

- **隔离级别**：默认 **Snapshot Isolation**；
- **ACID**：完整支持；
- **MVCC**：基于 meta page 双缓冲 + page_id 间接引用；
- **单写者**：所有写事务串行化，跨进程通过 OS 锁；
- **Abort / Rollback**：直接丢弃 page_id list 与 dirty 状态；
- **Nested Transaction**：支持（parent/child，但 child 不能并行 commit）。

## 十一、复制与高可用

LMDB 是**嵌入式库**，无内置复制；高可用方案：

- **`mdb_copy`**：`mdb_copy.c` 工具在 active LMDB 上做一致快照（基于 `MDB_NOLOCK` + `MDB_RDONLY`）；
- **Syncthing / DRBD**：OS-level 文件复制；
- **应用层复制**：调用方定期从某个读事务生成文件备份；
- **网络层 read failover**：跨进程读自然支持，跨主机的读需要 `sshfs` 或自定义同步。

## 十二、扩展性

LMDB 的设计目标是「**最小化**」，**故意不提供扩展 API**：

- 自定义 compare function（`mdb_set_compare`）；
- 自定义 dup-sort 函数（`mdb_set_dupsort`）；
- 自定义 hash（少数加密场景）；
- **但**：没有 User-Defined Function、没有 SQL、没有触发器。

变体/扩展（社区）：

- **libhdb**（Lightning homogeneous database）：Symas 提供的 hash 化扩展；
- **MDBX**（Faster LMDB fork）：Erast Potapo（github.com/erthink/erthink），更高性能的 LMDB 改进版（atomic spill、serialization fixes 等）。

## 十三、性能与基准

- **典型性能数字**：
  - **读**：微秒级（OS page cache 命中）甚至 ns 级（mmap 直接返回指针）；
  - **写**：COW 开销 = 修改一条路径上的所有祖先页；典型 1ms-5ms / Commit（视 map size）；
  - **TPS**：写 50K-200K / 单核 / Commit batch；
- **官方 benchmark**：
  - 一致脚本 `liblmdb-bench`：见 `bench/mdb_bench.c`（提供但非权威）；
- **第三方 benchmark**：
  - **io2014 benchmark**：LMDB 读优于 LevelDB、BerkeleyDB、Kyoto Cabinet；
  - **mongo-rdb benchmark**：LMDB 不适合文档模型（KV 单层）；
  - **MessagePack Speed Test**：LMDB vs HashFlatMap → LMDB 持久化场景 OK；纯内存场景 mdb_get 比 hash map 慢 5-10x。

## 十四、运维与可观测性

- **状态工具**：`mdb_stat.c`（打印每个 DB 的统计：root page、branch / leaf page 数、overflow page 数）；
- **诊断**：`mdb_dump.c`（DB 文本 dump）、`mdb_load.c`（文本 dump 重新 load）；
- **拷贝**：`mdb_copy.c`（hot-copy / cold-copy）；
- **日志**：LMDB 内部不写日志；调用方需要外部记录；
- **Prometheus** 等需自建 exporter。

## 十五、生态与集成

- **C API**：原生；
- **多语言绑定**：
  - Python：`py-lmdb`（隐含轮子，全新实现，作者 Howard Chu 之外的第三方）；
  - Java：`lmdbjni`、`LMDBJava`；
  - Go：`github.com/bmatsuo/go-lmdb`（archived）；
  - Rust：`lmdb-rs`；
  - Ruby：`lmdb` gem；
- **嵌入式数据库引擎**：OpenLDAP、Knot DNS、PowerDNS、Unbound；
- **教学**：CMU 15-721（教材《Database System Implementation》第 11 章 mmap 案例）；
- **变体**：MDBX、LightningDB、libhdb。

## 十六、核心论文与算法原理

- **Classic B+tree**：Comer *"The Ubiquitous B-Tree"*（1979）；
- **MVCC via page-tables + mmap**：传统 Olson & Chamberlin 风格，LMDB 走 meta-page 路径；
- **MMAP + COW**：本质沿用 Ousterhout 等 1980 论文（LMDB 简化版本）；
- **OpenLDAP 论文**：Howard Chu *"LMDB: The Memory-Mapped Database"*（OpenLDAP.org.cn）；
- **Linear Hashing**：Litwin *"Linear Hashing: A New Tool for File and Table Addressing"*（1980）。LMDB 不直接使用，但对扩展（hdb）重要。

## 十七、源码学习路径

必读清单（按从浅到深顺序）：

1. **API 头**：`libraries/liblmdb/lmdb.h`（约 700 行，全 API）；
2. **关键数据结构**：`mdb.c:96` `MDB_PAGE`、`MDB_meta`、`MDB_node`、`MDB_db`；
3. **环境打开**：`mdb_env_open`（`mdb.c:4185`）；
4. **事务开始**：`mdb_txn_begin`（`mdb.c:2091`）；
5. **写入**：`mdb_put`（`mdb.c:5125`）+ `mdb_cursor_put`（`mdb.c:5876`）；
6. **读取**：`mdb_get`（`mdb.c:5102`）+ `mdb_cursor_get`（`mdb.c:6013`）；
7. **提交**：`mdb_txn_commit`（`mdb.c:2594`）；
8. **Meta 切换**：`mdb_env_write_meta`（`mdb.c:4250`）+ `mdb_env_sync0`；
9. **Free List**：`mdb_freelist_save`（`mdb.c:3679`）；
10. **Page 操作**：`mdb_page_get`（`mdb.c:1793`）、`mdb_page_new`、`mdb_page_alloc`；
11. **MMAP**：`mdb_page_unmap` + `mdb_page_map` 视图管理。

源码规模：约 **6000 行 C**（单一 mdb.c），配套 lmdb.h + tools（mdb_stat、mdb_dump、mdb_load、mdb_copy）。

## 十八、可移植性与部署

- **平台**：
  - Linux、FreeBSD、macOS、AIX、Windows（GNU MSYS2 / Visual Studio 兼容）；
  - 主要限制：OS 的 `mmap` API 差异、fcntl vs flock vs posix sem 互斥差异；
- **构建**：`Makefile`（无 CMake），一行 `make` 即可；
- **嵌入**：仅需 `lmdb.c` + `lmdb.h` + `midl.c` + `<pthread.h>` 等 OS 头；
- **网络部署**：不可直接部署，需应用层负责；
- **WASM**：理论上可行（无 fork / mmap 模拟）。

## 十九、变体与衍生项目

- **MDBX**（Lightning Memory-Mapped Database 增强版）：
  - 作者 Erast Potapo；
  - 性能优化（atomic spill、serialization consistency）；
  - 推荐用于新项目（已合并多个 LMDB 改进）；
- **Lightning DB**：项目历史的原始代码；
- **libhdb**：Symas 内置 LDAP 的扩展，含 hash index；
- **Encrypted LMDB**：社区项目（不含在官方中）。

## 二十、风险与陷阱

| 陷阱 | 描述 | 缓解 |
|------|------|------|
| **mapsize 限制** | 创建后不能扩展 mapsize 超过初始值（除非重建） | 启动前一次预估好 `mdb_env_set_mapsize` |
| **commit 后 fsync** | 默认每次 commit 一次 fsync（OS 慢） | `MDB_MAPASYNC` flag 或批量 commit |
| **单写者吞吐** | 单线程写入无法水平扩展；QPS 写入 50K 上限 | 应用层 sharding / multi-DB sharding |
| **read-only 必须 reopen** | 在 RO flag 下的 mmap 不能写入 | 必然时切换 RW |
| **lock 文件** | 需要单独 lock.mdb，权限敏感 | chmod + chown 正确配置 |
| **mdb_env_close 延迟** | 当 `MDB_NOTLS=0` 时未 TLS 线程问题 | 严格调 `mdb_txn_reset/abort/renew` |
| **mapsize ≠ 文件大小** | 在写时的 posix_fallocate 才能扩展 | 启动时 `MADV_HUGEPAGE` |
| **OS 锁不可移植** | macOS vs Linux 锁差异，导致死锁 | 注意 `MDB_NOTLS` 与 `MDB_USE_SUBDB` |
| **不可重入** | 一个 thread 不能重入嵌套 write txn | 设计上避开重入；只能嵌套只读 |
| **memory fragmentation** | COW 复制 B+tree 路径到 root | 频繁写场景谨慎 |

## 二十一、与本项目的关联

LMDB 的设计哲学对工程轨有借鉴价值：

- **借鉴点**：
  - **mmap COW + Meta 双缓冲**：`engineering/src/db/` 当前实现基于 `WAL + Checkpoint`，可考虑 LMDB 风格做只读快照 cache；
  - **B+tree 内嵌实现**：`engineering/src/index/` 中 B+tree 实现可参考 mdb.c 中精简的元数据布局；
  - **API 极简**：当前 `engineering/include/db/kv.h` API 面较臃肿，可考虑精简；
- **直接集成**：可通过 `liblmdb` 直接完成 LDAP、DNS 等嵌入式应用；
- **量化对比**：`engineering/include/db/mm_storage.h` 中的 KV 与 LMDB benchmark 对比能反映自家系统短板。

## 二十二、学习资源

- **官方文档**：[http://www.lmdb.tech/doc/](http://www.lmdb.tech/doc/)（基本与 `lmdb.h` 等价）；
- **OpenLDAP Wiki**：[https://www.openldap.org/doc/admin24/slapd-mdb.html](https://www.openldap.org/doc/admin24/slapd-mdb.html)（LDAP 集成视角）；
- **Howard Chu 演讲**：2014 LDAPCon / LinuxCon talk 视频；
- **教材**：Hellerstein 《Database System Implementation》第 11 章 mmap；
- **MDBX**：[https://github.com/erthink/erthink](https://github.com/erthink/erthink)（增强版 fork）；
- **GitHub**：[https://github.com/LMDB/lmdb](https://github.com/LMDB/lmdb)（主仓库）与 [https://git.openldap.org/openldap/openldap/-/tree/master/libraries/liblmdb](https://git.openldap.org/openldap/openldap)；
- **Slack / 邮件列表**：openldap-technical@openldap.org；
- **变体**：[https://github.com/erthink/erthink](https://github.com/erthink/erthink)（MDBX）；
- **Reddit**：r/embedded、r/sysadmin 偶尔提及。

## 二十三、参考资料

1. Howard Chu. *LMDB: The Memory-Mapped Database*. OpenLDAP.org 官方文档.
2. Comer, D. *The Ubiquitous B-Tree*. ACM Computing Surveys 11(2), 1979.
3. Olson, M. *Choosing a Database Backend for OpenLDAP — How and Why*. OpenLDAP Foundation, 2014.
4. Howard Chu 演讲: <http://symas.com/mdb/20130225/>
5. Lightning Memory-Mapped Database (LMDB) Wiki: <http://www.lmdb.tech>
6. OpenLDAP slapd-mdb(5) manual: <https://www.openldap.org/software/man.cgi?query=slapd-mdb>
7. LMDB GitHub: <https://github.com/LMDB/lmdb>
8. MDBX 仓库: <https://github.com/erthink/erthink>
9. Linear Hashing (Litwin, 1980) - LMDB 扩展 libhdb 参考.
10. Taras Glek 的 LMDB 性能分析: <https://taras.glek.info/blog/2016-06-17-lmdb-vs-leveldb.html>
