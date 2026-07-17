# 存储引擎规格

## 功能概述

磁盘文件存储引擎，负责数据的持久化存储。

## 核心概念

### 页面（Page）

- **大小**：8KB（8192字节）
- **结构**：
  ```
  ┌────────────────────────────────────────┐
  │  Page Header (16 bytes)                │
  │  ├── page_id: uint32 (4B)              │
  │  ├── page_type: uint8 (1B)             │
  │  ├── checksum: uint16 (2B)             │
  │  └── free_space_offset: uint16 (2B)    │
  │  Reserved (7B)                         │
  ├────────────────────────────────────────┤
  │  Page Body (8176 bytes)                │
  │  └── 存储实际数据                       │
  └────────────────────────────────────────┘
  ```

### 页面类型

| 类型 | 值 | 用途 |
|------|-----|------|
| PAGE_FREE | 0 | 空闲页 |
| PAGE_DATA | 1 | 数据页 |
| PAGE_INDEX | 2 | 索引页 |
| PAGE_OVERFLOW | 3 | 溢出页（大字段） |
| PAGE_META | 4 | 元数据页 |

### 缓存池（Buffer Pool）

- **大小**：可配置，默认 1000 页（约 8MB）
- **淘汰策略**：LRU（最近最少使用）
- **状态**：
  - `Pinned`：页面被持有，不可淘汰
  - `Dirty`：页面被修改，需要刷盘
  - `Clean`：页面未修改，可直接淘汰

## 接口设计

```c
// 初始化
int storage_init(const char *db_path, size_t page_size, size_t pool_size);

// 关闭
void storage_shutdown(void);

// 页面操作
page_id_t page_alloc(page_type_t type);
int page_free(page_id_t page_id);
int page_read(page_id_t page_id, page_t **out_page);
int page_write(page_id_t page_id, page_t *page);
int page_pin(page_id_t page_id);      // 持有页面
int page_unpin(page_id_t page_id);    // 释放页面

// 刷盘
int buffer_flush_all(void);           // 刷所有脏页
int buffer_flush_page(page_id_t pid); // 刷单页
int buffer_flush_db(void);            // 刷整个数据库

// 元数据
int storage_get_meta(const char *key, void *value, size_t *len);
int storage_set_meta(const char *key, const void *value, size_t len);
```

## 文件布局

```
{db_path}/
├── data.db      # 主数据文件
│   ├── 文件头 (1 page): 数据库元信息
│   └── 数据页 (N pages): 存储实际数据
└── wal.log      # WAL日志（见 transaction-wal）
```

## 设计决策

1. **内存映射 vs 读写系统调用**
   - 选择：读写系统调用（pread/pwrite）
   - 理由：更灵活，可精确控制缓存

2. **并发控制**
   - 使用读写锁保护缓存池
   - 单写者多读者模式

## 验收标准

- [ ] 能创建新的数据库文件
- [ ] 能分配和释放页面
- [ ] 页面读写正确
- [ ] 缓存池正确缓存页面
- [ ] LRU淘汰策略工作正常
- [ ] 脏页能正确刷盘
- [ ] 数据库关闭后数据不丢失