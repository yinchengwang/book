# Linux NFS 网络文件系统学习笔记

本文档记录 NFS 网络文件系统在项目工程代码中的实际应用对照。

## 工程对照

### 网络存储与分布式数据库远程存储访问

NFS 是经典的网络文件系统，通过网络将远程文件系统透明地挂载到本地。在分布式数据库系统中，远程存储访问是核心能力之一，其设计理念与 NFS 有诸多共通之处，但在一致性、性能和容错方面有更高的要求。

### 1. 远程页面访问 vs NFS READ/WRITE RPC

NFS 将文件 I/O 系统调用映射为 RPC 请求（READ/WRITE/COMMIT），工程中的 Buffer Pool 同样需要从远程节点获取数据页面：

```c
// c:\code\book\engineering\src\db\storage\buffer\bufmgr.c
// Buffer Pool 的 buffer_read_page() 函数
// 负责从磁盘（或远程节点）读取页面到内存缓存
// 类比 NFS 客户端的 READ RPC：请求远程数据块

static int buffer_read_page(BufferPool *pool, BufferDesc *buf) {
    // 从磁盘读取页面 → 类似 NFS READ RPC
    // 缓存到 Buffer Pool → 类似 NFS 客户端页缓存
    disk_read_page(pool->disk, buf->relfilenode, buf->blocknum, ...);
    buf->state |= BM_VALID;
    pool->reads++;
    return 0;
}
```

**对比分析：**

| 概念 | NFS | 数据库远程存储 |
|------|-----|---------------|
| 数据单元 | 文件字节范围 | 数据库页面（通常 8KB） |
| 传输协议 | NFS RPC over TCP/UDP | 自定义协议 over TCP |
| 缓存层 | 客户端页缓存 + 属性缓存 | Buffer Pool + 本地磁盘 |
| 一致性模型 | close-to-open | MVCC 快照隔离 / 分布式事务 |
| 故障处理 | hard/soft 挂载 + 重传 | 副本切换 + Raft 选主 |

### 2. NFS 缓存一致性与数据库缓存失效

NFS 的 close-to-open 一致性模型是一种弱一致性保证，数据库则需要更强的一致性：

```c
// c:\code\book\engineering\src\db\storage\buffer\bufmgr.c
// Clock-Sweep 置换算法中的脏页回写
// 类比 NFS 客户端将修改刷新到服务器

// NFS: close() 时刷新修改 → 服务器可见
// DB:  检查点/WAL 刷盘 → 持久化 + 副本可见
if (buf->state & BM_DIRTY) {
    disk_write_page(pool->disk, buf->relfilenode, buf->blocknum, ...);
    buf->state &= ~BM_DIRTY;
    pool->writes++;
}
```

### 3. NFS 锁定管理与数据库分布式锁

NFS 的 lockd/statd 提供网络文件锁，数据库则需要更精细的锁管理：

```c
// c:\code\book\engineering\src\db\storage\lock\lock_mgr.c
// 数据库锁管理器：支持行锁、页锁、表锁多粒度
// 类比 NFS lockd：将锁请求转换为网络协议消息

// NFS lockd:  fcntl(F_SETLK) → NLM_LOCK RPC → 服务器锁
// DB lockmgr:  LockAcquire()  → 锁表检查      → 授予/等待
```

### 4. 分布式存储中的数据分片

NFS 可挂载多个服务器的不同导出目录（通过 autofs 或手动挂载），数据库的分片路由有类似概念：

```c
// 工程中的 KV 存储引擎通过一致性哈希实现数据分布
// c:\code\book\engineering\src\db\storage\kv\kv_engine.c
// 类比: NFS 将不同目录挂载到不同服务器

// NFS:  /data/shard1 → server1:/export/shard1
//       /data/shard2 → server2:/export/shard2
// DB:   hash(key) % N → 路由到对应分片节点
```

### 5. 关键差异总结

| 维度 | NFS | 数据库远程存储 |
|------|-----|---------------|
| 一致性要求 | 弱一致（close-to-open） | 强一致（ACID 事务） |
| 性能优化 | rsize/wsize 调优 + 缓存 | Buffer Pool + 预读 + 批量写入 |
| 故障恢复 | hard/soft 重试 + 租约恢复 | WAL 恢复 + 副本选举 + 检查点 |
| 并发控制 | 文件锁 (fcntl/flock) | MVCC + 意向锁 + 死锁检测 |
| 数据模型 | 层次目录 + 文件 | 关系表 / KV / 文档 / 图 |

NFS 的设计哲学是"透明地扩展本地文件系统到网络"，核心权衡在于简单性与一致性。数据库远程存储则是"为分布式事务而生的存储协议"，在一致性、可用性和分区容忍性之间根据 CAP 理论做精确取舍。理解 NFS 的协议设计有助于更好把握分布式存储系统中的远程数据访问模式。

## 参考源码

- `c:\code\book\engineering\src\db\storage\buffer\bufmgr.c` — Buffer Pool 缓存管理
- `c:\code\book\engineering\src\db\storage\page\disk.c` — 磁盘页面读写
- `c:\code\book\engineering\src\db\storage\lock\lock_mgr.c` — 锁管理器
- `c:\code\book\engineering\src\db\storage\kv\kv_engine.c` — KV 存储引擎
- `c:\code\book\engineering\src\db\storage\wal\wal.c` — WAL 日志
- `c:\code\book\engineering\src\db\storage\txn\mvcc.c` — MVCC 多版本并发控制
