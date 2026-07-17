# Linux FUSE 用户态文件系统学习笔记

本文档记录 FUSE 用户态文件系统在项目工程代码中的实际应用对照。

## 工程对照

### 用户态文件系统与数据库虚拟文件系统（VFS）层对照

FUSE 将文件系统实现从内核态移到用户态，通过操作表（fuse_operations）实现多态分发。数据库的虚拟文件系统（VFS）层也是类似的设计模式：定义统一接口，让不同存储引擎实现各自的操作语义。两者的核心思想都是"接口抽象 + 多态实现"。

### 1. fuse_operations 与数据库存储引擎接口

FUSE 的 `fuse_operations` 是一组回调函数指针，数据库的存储引擎也是通过类似的操作接口实现多态：

```c
// c:\code\book\engineering\src\db\storage\buffer\bufmgr.c
// Buffer Pool 的 buffer_get_page() 函数
// 类比 FUSE 的 getattr：都是通过标识符获取数据

// FUSE:  getattr(ino)    → 返回文件属性
// DB:    buffer_get_page(page_id) → 返回数据页面

BufferDesc *buffer_get_page(BufferPool *pool, uint32_t relfilenode,
                            uint32_t blocknum) {
    // 1. 在 Hash 表中查找（类比 FUSE 在文件表中查找 inode）
    // 2. 未命中时从磁盘读取（类比 FUSE 从后端存储读取）
    // 3. 返回内存中的页面描述符（类比 FUSE 返回 stat 结构）
}
```

**操作表对照：**

| FUSE 回调 | 功能 | 数据库对应操作 |
|-----------|------|---------------|
| getattr | 获取文件属性 | buffer_get_page / rel_get_tuple_desc |
| readdir | 读取目录 | table_scan / index_scan |
| read | 读取文件数据 | heap_get_tuple / btree_search |
| write | 写入文件数据 | heap_insert_tuple / heap_update_tuple |
| create | 创建文件 | catalog_create_table / rel_create |
| unlink | 删除文件 | catalog_drop_table |
| fsync | 持久化数据 | wal_flush / checkpoint |

### 2. VFS 层的多态分发

FUSE 通过 `/dev/fuse` 将 VFS 请求转发给用户态守护进程，数据库的存储引擎也有类似的分发机制：

```c
// c:\code\book\engineering\src\db\storage\kv\kv_engine.c
// KV 存储引擎：实现了类似 fuse_operations 的操作集
// 通过统一的 storage_engine 接口进行多态调用

// 类比 FUSE 的请求分发:
// VFS → /dev/fuse → libfuse → fuse_operations.getattr()
// SQL → storage_engine → kv_engine → kv_get()
```

### 3. 内存文件系统与内存存储引擎

FUSE 可以实现纯内存的文件系统（如本演示的 memfs），数据库同样有内存存储引擎：

```c
// c:\code\book\engineering\src\db\storage\mm\mm_storage.c
// 内存存储引擎：数据完全驻留在内存中
// 类比 FUSE memfs：都是易失性存储

// FUSE memfs:    进程内存 → 重启丢失
// DB mm_storage: 共享内存 → 进程重启可恢复（通过 mmap 文件）
```

### 4. 多引擎架构的统一抽象

数据库支持多种存储引擎（KV/Vector/Timeseries/Document/Graph 等），每种引擎实现统一的操作接口：

```c
// c:\code\book\engineering\src\db\storage\mm\mm_pool.c
// 多模态存储引擎的统一内存池管理
// 类比 FUSE: 不同的 fuse_operations 实现 → 不同的文件系统

// FUSE 可挂载: ext4-fuse, ntfs-3g, s3fs, sshfs ...
// DB 可切换:  HeapAM, BTreeAM, KVEngine, VectorEngine ...
```

### 5. 关键设计模式对照

| 设计模式 | FUSE 体现 | 数据库体现 |
|----------|----------|-----------|
| 策略模式 | fuse_operations 回调表 | storage_engine 虚函数表 |
| 适配器模式 | /dev/fuse ↔ VFS 桥接 | SQL 解析器 ↔ 执行器 |
| 工厂模式 | fuse_main() 创建会话 | kv_open() / table_open() |
| 代理模式 | 用户态守护进程代理 I/O | Buffer Pool 代理磁盘 I/O |
| 观察者模式 | FUSE 事件通知 | WAL 日志 + 复制流 |

FUSE 框架的核心价值在于证明了"文件系统不必在内核中实现"，这一思想直接启发了数据库领域的用户态存储引擎设计。工程中的多模态存储引擎（mm_storage）正是借鉴了这种"接口统一、实现多样"的架构，使得 KV、向量、时序、文档等不同数据模型可以共用同一套 Buffer Pool 和 WAL 基础设施。

## 参考源码

- `c:\code\book\engineering\src\db\storage\buffer\bufmgr.c` — Buffer Pool 缓存管理
- `c:\code\book\engineering\src\db\storage\kv\kv_engine.c` — KV 存储引擎
- `c:\code\book\engineering\src\db\storage\mm\mm_storage.c` — 内存存储引擎
- `c:\code\book\engineering\src\db\storage\mm\mm_pool.c` — 内存池管理
- `c:\code\book\engineering\src\db\storage\wal\wal.c` — WAL 日志持久化
- `c:\code\book\engineering\src\db\storage\rel\rel.c` — Relation 抽象
