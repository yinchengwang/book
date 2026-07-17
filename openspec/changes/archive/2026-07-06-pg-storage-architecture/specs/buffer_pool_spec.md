# Buffer Pool 规格

## 1. 概述

Buffer Pool 是数据库的内存缓存层，用于缓存磁盘页面，减少磁盘 I/O。

## 2. 架构

```
┌─────────────────────────────────────────────────────┐
│                 Buffer Pool Manager                 │
├─────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────┐   │
│  │              Buffer Descriptors             │   │
│  │  ┌────┐ ┌────┐ ┌────┐ ┌────┐ ... ┌────┐   │   │
│  │  │ B0 │ │ B1 │ │ B2 │ │ B3 │     │ Bn │   │   │
│  │  └────┘ └────┘ └────┘ └────┘     └────┘   │   │
│  └─────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────┐   │
│  │              Page Data Area                 │   │
│  │  ┌────────┐ ┌────────┐ ┌────────┐          │   │
│  │  │ Page 0 │ │ Page 1 │ │ Page 2 │ ...      │   │
│  │  └────────┘ └────────┘ └────────┘          │   │
│  └─────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────┐   │
│  │           Hash Table (快速查找)             │   │
│  │    (relfilenode, blocknum) → buffer_id      │   │
│  └─────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────┐   │
│  │           Clock-Sweep Replacer              │   │
│  │                 ▲                           │   │
│  │                 │ next_victim               │   │
│  └─────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

## 3. Buffer 状态

```
BufferDesc 状态标志（atomic）:
┌──────────────────────────────────────────────────────┐
│ [31..20]    │ [19]    │ [18]   │ [17]  │ [16..0]    │
│  unused     │  dirty  │  usage │ pinned│ refcount   │
└──────────────────────────────────────────────────────┘

状态组合:
- BM_VALID:        页面数据有效
- BM_DIRTY:        页面被修改，需要写回
- BM_PINNED:       页面被 pin 住，不能置换
- BM_IO_IN_PROGRESS: 正在从磁盘读取
- BM_IO_COMPLETED:    读取完成
```

## 4. API 规格

### 4.1 初始化

```c
/**
 * @brief 初始化 Buffer Pool
 * @param nbuffers buffer 数量
 * @return 0 成功，-1 失败
 */
int buf_init(int nbuffers);

/**
 * @brief 关闭 Buffer Pool
 */
void buf_shutdown(void);
```

### 4.2 页面读取

```c
/**
 * @brief 读取页面到 Buffer
 * @param rel  Relation
 * @param blocknum 块号
 * @return Buffer 描述符，失败返回 NULL
 */
BufferDesc *buf_read(Relation rel, BlockNumber blocknum);

/**
 * @brief 获取新页面（用于插入）
 * @param rel Relation
 * @return Buffer 描述符
 */
BufferDesc *buf_new(Relation rel);

/**
 * @brief 获取扩展 Relation 的新页面
 * @param rel Relation
 * @return Buffer 描述符
 */
BufferDesc *buf_new_page(Relation rel);
```

### 4.3 页面释放

```c
/**
 * @brief 释放 Buffer（减少 pin 计数）
 * @param buf Buffer 描述符
 */
void buf_release(BufferDesc *buf);

/**
 * @brief Unpin 并标记为脏
 * @param buf Buffer 描述符
 */
void buf_dirty(BufferDesc *buf);
```

### 4.4 页面刷盘

```c
/**
 * @brief 刷写所有脏页到磁盘
 */
void buf_flush_all(void);

/**
 * @brief 刷写指定 Relation 的脏页
 * @param rel Relation
 */
void buf_flush_relation(Relation rel);

/**
 * @brief 刷写单个 Buffer
 * @param buf Buffer 描述符
 */
void buf_write(BufferDesc *buf);
```

### 4.5 查询

```c
/**
 * @brief 获取 Buffer 内容指针
 * @param buf Buffer 描述符
 * @return 页面数据指针
 */
void *buf_get_data(BufferDesc *buf);

/**
 * @brief 获取 Buffer 对应的块号
 * @param buf Buffer 描述符
 * @return 块号
 */
BlockNumber buf_get_blocknum(BufferDesc *buf);

/**
 * @brief 检查 Buffer 是否脏
 * @param buf Buffer 描述符
 * @return true 脏
 */
bool buf_is_dirty(BufferDesc *buf);
```

## 5. 数据结构

```c
// Buffer 描述符
typedef struct BufferDesc_s {
    uint32_t    buf_id;           // 唯一 ID (0 到 N-1)
    atomic_int  state;            // 状态标志
    uint32_t    relfilenode;      // 物理文件节点
    BlockNumber blocknum;         // 块号
    int         usage_count;      // Clock-Sweep 计数
    LSN         last-written;     // 最后写入的 LSN
    
    slock_t     buf_hdr_lock;     // 头部锁
    LWLock      content_lock;     // 内容锁（读写）
    
    // Hash 链表
    uint32_t    hash_prev;
    uint32_t    hash_next;
} BufferDesc;

// Buffer Pool
typedef struct BufferPool_s {
    BufferDesc  *descriptors;     // 描述符数组
    char        **buffers;        // 页面数据数组
    int         nbuffers;         // Buffer 数量
    int         next_victim;      // Clock 指针
    
    // Hash 表
    uint32_t    *hash_table;      // Hash 桶
    uint32_t    hash_size;        // Hash 大小
    
    // 统计
    uint64_t    hits;             // 命中次数
    uint64_t    misses;           // 未命中次数
    uint64_t    writes;           // 写入次数
} BufferPool;
```

## 6. 算法

### 6.1 Clock-Sweep 置换

```
Algorithm ClockReplacement:
1. while true:
2.     buf = &descriptors[next_victim]
3.     
4.     if buf.pinned or buf.dirty:
5.         next_victim = (next_victim + 1) % nbuffers
6.         continue
7.     
8.     if buf.usage_count > 0:
9.         buf.usage_count--
10.        next_victim = (next_victim + 1) % nbuffers
11.        continue
12.    
13.    // 找到可置换的 buffer
14.    return buf
```

### 6.2 Hash 查找

```
HashKey = (relfilenode << 32) | blocknum
HashBucket = HashKey % hash_size

在 hash_table[bucket] 链表中查找匹配的 Buffer
```

## 7. 并发控制

- **Buffer 头部锁** (`buf_hdr_lock`): 保护 Buffer 描述符字段
- **内容锁** (`content_lock`): 
  - 读操作获取共享锁
  - 写操作获取排他锁
- **Hash 表锁**: 保护 Hash 链表操作

## 8. 与 KV 存储集成

```
Buffer Pool → KV Store:
┌──────────────────────────────────┐
│ ReadBuffer(rel, blocknum):       │
│   1. Hash 查找 (rel, blocknum)   │
│   2. 如果命中，返回 buffer       │
│   3. 如果未命中:                 │
│      a. 获取空 buffer (Clock)   │
│      b. kv_read(rel, blocknum)  │
│      c. 复制数据到 buffer       │
│      d. 加入 Hash 表            │
│   4. Pin buffer                 │
│   5. 返回                       │
└──────────────────────────────────┘
```

## 9. 限制

- Buffer 大小固定，启动时配置
- 不支持直接扩展/收缩 Buffer Pool
- 脏页刷盘时机由调用者控制
