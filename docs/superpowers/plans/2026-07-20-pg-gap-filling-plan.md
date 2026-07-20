# PG 对标补齐实施计划（Wave 1-6 全部）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将关系型数据库从 45% PG 对标补齐到 ~100%，覆盖 BTree 分裂、MVCC 版本链、CLOG、HOT、TOAST、并行执行、2PC、WAL 归档等 22 个模块

**Architecture:** 6 个 Wave 串行执行（W1→W2→W3→W4→W5→W6），每 Wave 内部任务可并行

**Tech Stack:** C11、C++17、CMake 3.20+、GoogleTest

**Spec:** [docs/superpowers/specs/2026-07-20-pg-gap-filling-design.md](../specs/2026-07-20-pg-gap-filling-design.md)

---

## Global Constraints

- **构建系统**：CMake 3.20+，Ninja 生成器，输出到 `build/engineering/`
- **代码规范**：C11/C++17，所有注释使用中文（CLAUDE.md）
- **Commit Message**：使用中文，遵循 Conventional Commits
- **测试框架**：GoogleTest（vendored），`.cpp` 测试文件
- **编译验证**：每个 Task 完成后 `ninja -j4` 全量编译通过 + 所有现有测试通过
- **OpenSpec 纪律**：只提交与变更相关的代码（不混提交）
- **现有接口保留**：不破坏已存在的 `storage_ops_t`、`IndexAmRoutine`、`guc_*` 等 API
- **依赖关系**：W2 依赖 W1（CLOG），W3 依赖 W2（Heap 改造），W5 依赖 W1（CLOG + WAL），W6 依赖 W1（BTree）

---

## Wave 1：基础（3-4 周）

### Task W1.1：BTree 页面格式扩展

**Files:**
- Modify: `engineering/include/db/access/btree/btpage.h`
- Test: `engineering/test/db/storage/btree_test.cpp`

**目标：** 在 BTPageHeader 中增加 `btpo_rightlink`、`btpo_cycleid`，扩展 `btpo_flags` 支持 `BTP_ROOT/BTP_LEAF/BTP_INTERNAL/BTP_HALF_DEAD`

- [ ] **Step 1：写测试验证结构体大小**

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "db/access/btree/btpage.h"
}

TEST(BTreePageTest, HeaderHasRightlink) {
    BTPageHeader header;
    memset(&header, 0, sizeof(header));
    // 验证 btpo_rightlink 字段存在且可写入
    header.btpo_rightlink = 42;
    EXPECT_EQ(header.btpo_rightlink, 42);
    // 验证 btpo_flags 可组合赋值
    header.btpo_flags = BTP_LEAF | BTP_ROOT;
    EXPECT_TRUE(header.btpo_flags & BTP_LEAF);
    EXPECT_TRUE(header.btpo_flags & BTP_ROOT);
}
```

- [ ] **Step 2：修改 btpage.h**

在 `BTPageHeader` 结构体中添加：
```c
BlockNumber btpo_rightlink;  // 右兄弟页指针（用于并发分裂后的扫描）
uint32 btpo_cycleid;         // 并发分裂标识
```

扩展 `btpo_flags`：
```c
#define BTP_LEAF       0x01  // 叶节点
#define BTP_ROOT       0x02  // 根节点
#define BTP_INTERNAL   0x04  // 内部节点
#define BTP_HALF_DEAD  0x08  // 分裂中的半死状态
```

- [ ] **Step 3：编译 + 测试通过**

```bash
cmake -S engineering -B build/engineering
ninja -j4 -C build/engineering test_btree
ctest --test-dir build/engineering --output-on-failure -R btree
```

- [ ] **Step 4：提交**

```bash
git add engineering/include/db/access/btree/btpage.h engineering/test/db/storage/btree_test.cpp
git commit -m "feat: BTree 页面格式扩展（btpo_rightlink + btpo_cycleid + btpo_flags 增强）"
```

---

### Task W1.2：BTree 叶节点分裂

**Files:**
- Create: `engineering/src/db/storage/access/btree/btree_split.c`
- Create: `engineering/include/db/access/btree/btree_split.h`
- Modify: `engineering/src/db/storage/access/btree/btreeam.c`
- Modify: `engineering/src/db/storage/access/btree/CMakeLists.txt`
- Test: `engineering/test/db/storage/btree_split_test.cpp`

**目标：** 叶节点满时，找中间键值，右半部分搬到新页，更新兄弟链接，插入父节点

- [ ] **Step 1：写分裂测试**

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "db/access/btree/btree_split.h"
#include "db/access/btree/btreeam.h"
}

TEST(BTreeSplitTest, LeafSplit) {
    // 创建 BTree 并插入 2 倍页面容量的数据
    Relation rel = btree_create("test_idx");
    for (int i = 0; i < BTREE_MAX_KEYS * 2; i++) {
        int ret = btinsert(rel, i, i);
        if (i >= BTREE_MAX_KEYS) {
            // 超过页面容量时应触发分裂而不是返回错误
            EXPECT_EQ(ret, 0);
        }
    }
    // 验证所有数据可检索
    for (int i = 0; i < BTREE_MAX_KEYS * 2; i++) {
        IndexScanDesc scan = bt_beginscan(rel, 0, NULL);
        // ... 扫描验证存在性
    }
    btree_destroy(rel);
}
```

- [ ] **Step 2：实现 `btree_split_leaf()` 函数**

```c
// btree_split.c
#include "db/access/btree/btree_split.h"
#include "db/access/btree/btpage.h"
#include "db/storage/bufmgr.h"

// 在叶节点页面上找到中间键值
int btree_split_find_pivot(Page page, int *pivot_index) {
    BTPageHeader *header = (BTPageHeader *)page;
    int total = header->btpo_count;
    *pivot_index = total / 2;  // 取中间位置
    return 0;
}

// 叶节点分裂：将右半部分搬到新页
int btree_split_leaf(Relation rel, BlockNumber old_blkno, BlockNumber *new_blkno) {
    Page old_page = buffer_read_page(rel, old_blkno);
    BTPageHeader *old_header = (BTPageHeader *)old_page;
    
    // 分配新页面
    *new_blkno = buffer_alloc_page(rel);
    Page new_page = buffer_read_page(rel, *new_blkno);
    BTPageHeader *new_header = (BTPageHeader *)new_page;
    
    // 找到中间键
    int pivot_idx;
    btree_split_find_pivot(old_page, &pivot_idx);
    
    // 将中间键之后的键值对搬到新页
    // ... 移动键值对逻辑
    
    // 更新兄弟链接
    new_header->btpo_next = old_header->btpo_next;
    old_header->btpo_next = *new_blkno;
    new_header->btpo_prev = old_blkno;
    new_header->btpo_rightlink = old_header->btpo_rightlink;
    new_header->btpo_level = old_header->btpo_level;
    new_header->btpo_flags = BTP_LEAF;
    
    // 更新旧页的键值计数
    old_header->btpo_count = pivot_idx;
    
    // 将中间键插入父节点
    // 如果有父节点 → btree_insert_internal(parent, pivot_key, *new_blkno)
    // 如果无父节点 → btree_split_root(rel, old_blkno, *new_blkno)
    
    buffer_mark_dirty(rel, old_blkno);
    buffer_mark_dirty(rel, *new_blkno);
    return 0;
}
```

- [ ] **Step 3：修改 btinsert 调用分裂**

在 `btreeam.c` 的 `btinsert` 中，如果页面已满，调 `btree_split_leaf` 而不是返回错误。

- [ ] **Step 4：编译 + 测试通过**

```bash
cmake -S engineering -B build/engineering
ninja -j4 -C build/engineering test_btree_split
ctest --test-dir build/engineering --output-on-failure -R btree_split
```

- [ ] **Step 5：提交**

```bash
git add engineering/src/db/storage/access/btree/btree_split.c engineering/include/db/access/btree/btree_split.h engineering/src/db/storage/access/btree/btreeam.c
git commit -m "feat: BTree 叶节点分裂实现"
```

---

### Task W1.3：BTree 内部节点分裂 + 根节点分裂

**Files:**
- Modify: `engineering/src/db/storage/access/btree/btree_split.c`
- Test: `engineering/test/db/storage/btree_split_internal_test.cpp`

**目标：** 内部节点满时，中间键值提升到父节点，递归分裂直到根；根节点满时创建新根

- [ ] **Step 1：实现 `btree_split_internal()`**

内部节点分裂类似叶节点，但需要处理子节点指针的移动。

- [ ] **Step 2：实现 `btree_split_root()`**

创建新根节点（level+1），原根拆为左右两个孩子，新根的第一个键是中间键。

- [ ] **Step 3：测试 3 层 BTree**

插入足够数据（`BTREE_MAX_KEYS^2` 以上）触发多层分裂，验证数据完整。

- [ ] **Step 4：提交**

---

### Task W1.4：BTree 并发 Latch

**Files:**
- Modify: `engineering/src/db/storage/access/btree/btreeam.c`
- Modify: `engineering/src/db/storage/access/btree/btree_split.c`
- Test: `engineering/test/db/storage/btree_concurrent_test.cpp`

**目标：** 每页面 `rwlock_t`，读锁共享，写锁互斥，页面号排序获取避免死锁

- [ ] **Step 1：在 BTPageHeader 中添加 `rwlock_t` 锁字段**

```c
typedef struct BTPageHeader {
    // ... 现有字段
    BlockNumber btpo_rightlink;
    uint32 btpo_cycleid;
    rwlock_t btpo_lock;       // 页面级读写锁
    uint16 btpo_flags;
    uint16 btpo_count;
    BlockNumber btpo_prev;
    BlockNumber btpo_next;
} BTPageHeader;
```

- [ ] **Step 2：实现读锁/写锁获取函数**

```c
void btree_lock_page(BlockNumber blkno, bool exclusive);
void btree_unlock_page(BlockNumber blkno);
```

- [ ] **Step 3：扫描时加读锁，插入时加写锁**

扫描：`btree_lock_page(blkno, false)` → 读 → `btree_unlock_page(blkno)` → 通过 `btpo_next` 跳到下一页

插入：先对路径上的页面按 blkno 排序，依次加写锁

- [ ] **Step 4：并发测试**

```cpp
TEST(BTreeConcurrentTest, MultiThreadInsert) {
    std::vector<std::thread> threads;
    const int N = 10000;
    for (int t = 0; t < 4; t++) {
        threads.push_back(std::thread([&, t]() {
            for (int i = t; i < N; i += 4) {
                btinsert(rel, i, i);
            }
        }));
    }
    for (auto &th : threads) th.join();
    // 验证 N 条数据全部存在
}
```

- [ ] **Step 5：提交**

---

### Task W1.5：CLOG 持久化

**Files:**
- Create: `engineering/src/db/storage/txn/clog.c`
- Create: `engineering/include/db/storage/txn/clog.h`
- Modify: `engineering/src/db/storage/txn/mvcc.c`
- Modify: `engineering/src/db/storage/CMakeLists.txt`
- Test: `engineering/test/db/storage/clog_test.cpp`

**目标：** CLOG 持久化到 `pg_xact/` 目录，SLRU 缓存，替换内存实现

- [ ] **Step 1：写 CLOG 测试**

```cpp
TEST(ClogTest, SetAndGet) {
    ASSERT_EQ(clog_init("./test_data/pg_xact"), 0);
    
    clog_set_status(100, TRANSACTION_STATUS_COMMITTED);
    EXPECT_EQ(clog_get_status(100), TRANSACTION_STATUS_COMMITTED);
    
    clog_set_status(200, TRANSACTION_STATUS_ABORTED);
    EXPECT_EQ(clog_get_status(200), TRANSACTION_STATUS_ABORTED);
    
    clog_shutdown();
}

TEST(ClogTest, PersistenceAfterRestart) {
    ASSERT_EQ(clog_init("./test_data/pg_xact"), 0);
    clog_set_status(300, TRANSACTION_STATUS_COMMITTED);
    clog_flush();
    clog_shutdown();
    
    // 重启后读取
    ASSERT_EQ(clog_init("./test_data/pg_xact"), 0);
    EXPECT_EQ(clog_get_status(300), TRANSACTION_STATUS_COMMITTED);
    clog_shutdown();
}
```

- [ ] **Step 2：实现 CLOG 文件格式**

```c
#define CLOG_BITS_PER_XACT   2
#define CLOG_XACTS_PER_BYTE  4
#define CLOG_XACTS_PER_PAGE  (BLCKSZ * CLOG_XACTS_PER_BYTE)
#define CLOG_XACTS_PER_SEG   (256 * 1024 * CLOG_XACTS_PER_BYTE)  // 256KB 段

// 2bit 编码
#define CLOG_STATUS_IN_PROGRESS  0x0
#define CLOG_STATUS_COMMITTED    0x1
#define CLOG_STATUS_ABORTED      0x2
#define CLOG_STATUS_PREPARED     0x3
```

- [ ] **Step 3：实现 SLRU 缓存**

```c
#define CLOG_CACHE_PAGES 64

typedef struct ClogCacheEntry {
    bool valid;
    bool dirty;
    TransactionId base_xid;   // 该页面覆盖的起始 XID
    uint8 data[BLCKSZ];       // 缓存页面数据
    BlockNumber segno;        // 段文件序号
    int lru_prev, lru_next;   // LRU 链表
} ClogCacheEntry;
```

- [ ] **Step 4：替换 mvcc.c 中的内存实现**

```c
// 原来：
void mvcc_mark_committed(TransactionId xid) {
    g_xact_status[xid] = TRANSACTION_STATUS_COMMITTED;
}
// 改为：
void mvcc_mark_committed(TransactionId xid) {
    clog_set_status(xid, TRANSACTION_STATUS_COMMITTED);
}
```

- [ ] **Step 5：编译 + 测试通过**

- [ ] **Step 6：提交**

---

### Task W1.6：脏页刷盘

**Files:**
- Modify: `engineering/src/db/storage/buffer/bufmgr.c`
- Modify: `engineering/include/db/storage/bufmgr.h`
- Test: `engineering/test/db/storage/buf_flush_test.cpp`

**目标：** `buf_write` 真正写盘（通过 `pwrite`），写前确保 WAL 已刷盘

- [ ] **Step 1：写测试**

```cpp
TEST(BufferFlushTest, WriteToDisk) {
    buf_init("./test_data", 100);
    
    Buffer buf = buf_new(rel, 0);
    Page page = buf_get_page(buf);
    // 写入数据
    memcpy(page->data, "hello", 5);
    buf_mark_dirty(buf);
    
    // 刷盘
    EXPECT_EQ(buf_write(buf), 0);
    
    // 重新读取验证
    buf_clear(buf);
    buf_read(rel, 0);
    EXPECT_EQ(memcmp(page->data, "hello", 5), 0);
    
    buf_shutdown();
}
```

- [ ] **Step 2：实现 `buf_write()`**

```c
int buf_write(Buffer buf) {
    BufferDesc *desc = &g_buffer_descriptors[buf];
    
    // 确保 WAL 已刷盘
    if (desc->lsn > wal_get_flush_lsn()) {
        wal_flush();
    }
    
    // 计算文件偏移
    off_t offset = desc->tag.blockNum * BLCKSZ;
    
    // pwrite 写盘
    ssize_t written = pwrite(desc->fd, desc->page, BLCKSZ, offset);
    if (written != BLCKSZ) {
        return -1;
    }
    
    desc->flags &= ~BM_DIRTY;
    return 0;
}
```

- [ ] **Step 3：修改 `buf_flush_all()` 调用 `buf_write()`**

- [ ] **Step 4：提交**

---

### Task W1.7：后台写进程（Background Writer）

**Files:**
- Create: `engineering/src/db/storage/buffer/bgwriter.c`
- Create: `engineering/include/db/storage/buffer/bgwriter.h`
- Modify: `engineering/src/db/storage/buffer/CMakeLists.txt`
- Test: `engineering/test/db/storage/bgwriter_test.cpp`

**目标：** 独立线程，周期 100ms 扫描脏页，每批写 10 页

- [ ] **Step 1：实现后台写进程**

```c
static void *bgwriter_main(void *arg) {
    while (!g_bgwriter_stop) {
        usleep(g_bgwriter_delay * 1000);  // 默认 100ms
        
        int written = 0;
        for (int i = 0; i < g_bgwriter_max_pages && written < g_bgwriter_max_pages; i++) {
            int buf_id = (g_bgwriter_next + i) % NBuffers;
            BufferDesc *desc = &g_buffer_descriptors[buf_id];
            
            if ((desc->flags & BM_DIRTY) && !(desc->flags & BM_PIN_COUNT)) {
                buf_write(buf_id);
                written++;
            }
        }
        g_bgwriter_next = (g_bgwriter_next + written) % NBuffers;
    }
    return NULL;
}
```

- [ ] **Step 2：注册 GUC 参数**

```c
register_int("bgwriter_delay", 100, 10, 10000, "后台写进程扫描间隔（毫秒）");
register_int("bgwriter_lru_maxpages", 10, 0, 1000, "每次最大写页数");
```

- [ ] **Step 3：集成到 `buf_init/buf_shutdown`**

```c
int buf_init(const char *data_dir, int nbuffers) {
    // ... 现有代码
    bgwriter_start();
    return 0;
}

void buf_shutdown(void) {
    bgwriter_stop();
    // ... 现有代码
}
```

- [ ] **Step 4：提交**

---

## Wave 2：版本链核心（4-5 周）

### Task W2.1：Heap 元组头改造（t_ctid + infomask）

**Files:**
- Modify: `engineering/include/db/access/heapam.h`
- Test: `engineering/test/db/storage/heap_tuple_test.cpp`

**目标：** `HeapTupleHeaderData` 增加 `t_ctid`（`ItemPointerData`）、`t_infomask`（32bit）、`t_hoff`

- [ ] **Step 1：写测试**

```cpp
TEST(HeapTupleTest, HeaderHasCtid) {
    HeapTupleHeaderData header;
    memset(&header, 0, sizeof(header));
    
    ItemPointerSet(&header.t_ctid, 1, 10);
    EXPECT_EQ(header.t_ctid.ip_blkid, 1);
    EXPECT_EQ(header.t_ctid.ip_posid, 10);
    
    header.t_infomask = HEAP_XMIN_COMMITTED | HEAP_UPDATED;
    EXPECT_TRUE(header.t_infomask & HEAP_XMIN_COMMITTED);
}
```

- [ ] **Step 2：修改 `heapam.h` 中的 `HeapTupleHeaderData`**

- [ ] **Step 3：编译 + 测试通过**

- [ ] **Step 4：提交**

---

### Task W2.2：heap_update 版本链

**Files:**
- Modify: `engineering/src/db/storage/access/heap/heapam.c`
- Test: `engineering/test/db/storage/heap_update_test.cpp`

**目标：** UPDATE 创建新版本，旧 `t_ctid` 指向新位置，旧 `t_xmax` = 当前 XID，新 `t_xmin` = 当前 XID

- [ ] **Step 1：写测试**

```cpp
TEST(HeapUpdateTest, VersionChain) {
    // INSERT 一条元组
    HeapTuple tuple = heap_insert(rel, values);
    ItemPointer old_ctid = tuple->t_ctid;
    
    // UPDATE 该元组
    HeapTuple new_tuple = heap_update(rel, old_ctid, new_values);
    
    // 验证旧元组的 t_ctid 指向新位置
    Page page = buf_get_page(old_ctid->ip_blkid);
    HeapTupleHeader old_header = (HeapTupleHeader)(page->data + ...);
    EXPECT_TRUE(ItemPointerEquals(&old_header->t_ctid, &new_tuple->t_ctid));
    EXPECT_EQ(old_header->t_xmax, current_xid);
    EXPECT_TRUE(old_header->t_infomask & HEAP_UPDATED);
}
```

- [ ] **Step 2：实现 heap_update 新流程**

- [ ] **Step 3：编译 + 测试通过**

- [ ] **Step 4：提交**

---

### Task W2.3：heap_delete 标记删除

**Files:**
- Modify: `engineering/src/db/storage/access/heap/heapam.c`
- Test: `engineering/test/db/storage/heap_delete_test.cpp`

**目标：** DELETE 设 `t_xmax` = 当前 XID，不物理删除

---

### Task W2.4：HOT 实现

**Files:**
- Modify: `engineering/src/db/storage/access/heap/heapam.c`
- Test: `engineering/test/db/storage/hot_test.cpp`

**目标：** 同页 UPDATE 未修改索引键时，不修改索引，版本链通过 HOT 标志追踪

---

### Task W2.5：VACUUM 死元组清理

**Files:**
- Modify: `engineering/src/db/tools/vacuum/vacuum.c`
- Test: `engineering/test/db/tools/vacuum_test.cpp`

**目标：** 遍历页面标记死元组，压缩活元组合并空闲空间，全空页面回收

---

### Task W2.6：VACUUM 索引清理

**Files:**
- Add: `engineering/src/db/tools/vacuum/vacuum_index.c`
- Test: `engineering/test/db/tools/vacuum_index_test.cpp`

**目标：** 遍历索引删除指向死元组的索引项，HOT 链全死时删除

---

### Task W2.7：VACUUM Freeze

**Files:**
- Modify: `engineering/src/db/tools/vacuum/vacuum.c`

**目标：** `t_xmin` 早于 `oldest_xid_limit` 时标记 FROZEN，避免 XID wraparound

---

## Wave 3：存储扩展（4-5 周）

### Task W3.1：TOAST 实现

**Files:**
- Create: `engineering/src/db/storage/access/toast/toast.c`
- Create: `engineering/include/db/access/toast.h`
- Test: `engineering/test/db/storage/toast_test.cpp`

**目标：** 元组 > 2KB 时 pglz 压缩，压缩后仍超限则外存到 `pg_toast_<oid>`

---

### Task W3.2：预读（Read-ahead）

**Files:**
- Modify: `engineering/src/db/storage/buffer/bufmgr.c`
- Test: `engineering/test/db/storage/readahead_test.cpp`

**目标：** `buf_read_ahead()` 异步预读 N 页，全表扫描时触发

---

### Task W3.3：ANALYZE 实现

**Files:**
- Create: `engineering/src/db/tools/analyze/analyze.c`
- Create: `engineering/include/db/tools/analyze.h`
- Modify: `engineering/src/db/sql/planner.c`
- Test: `engineering/test/db/tools/analyze_test.cpp`

**目标：** 采样扫描，计算 MCV + 直方图 + NDV，持久化到 pg_statistic

---

### Task W3.4：planner 集成统计信息

**Files:**
- Modify: `engineering/src/db/sql/planner.c`
- Test: `engineering/test/db/sql/planner_statistics_test.cpp`

**目标：** `planner_get_table_stats` 读 pg_statistic，`estimate_selectivity` 用直方图

---

### Task W3.5：序列/自增

**Files:**
- Create: `engineering/src/db/core/sequence.c`
- Create: `engineering/include/db/sequence.h`
- Test: `engineering/test/db/core/sequence_test.cpp`

**目标：** `CREATE SEQUENCE` + `nextval/currval/setval` + SERIAL 伪类型

---

### Task W3.6：外键约束强制检查

**Files:**
- Modify: `engineering/src/db/sql/nodeModifytable.c`
- Modify: `engineering/include/db/storage/catalog/catalog.h`
- Test: `engineering/test/db/sql/foreign_key_test.cpp`

**目标：** INSERT/UPDATE 子表检查父表主键，DELETE 父表检查子表引用

---

### Task W3.7：CHECK 约束强制检查

**Files:**
- Modify: `engineering/src/db/sql/nodeModifytable.c`
- Test: `engineering/test/db/sql/check_constraint_test.cpp`

**目标：** INSERT/UPDATE 时求值约束表达式，返回 false 则拒绝

---

### Task W3.8：Double Write

**Files:**
- Create: `engineering/src/db/storage/buffer/double_write.c`
- Create: `engineering/include/db/storage/buffer/double_write.h`
- Test: `engineering/test/db/storage/double_write_test.cpp`

**目标：** 脏页刷盘时先批量写 Double Write 文件，再写实际位置

---

## Wave 4：查询加速（4-5 周）

### Task W4.1：并行执行框架

**Files:**
- Create: `engineering/include/db/sql/parallel.h`
- Modify: `engineering/src/db/sql/nodeGather.c`
- Test: `engineering/test/db/sql/parallel_test.cpp`

**目标：** `ParallelScanState` 共享计数器，Gather 节点汇聚结果

---

### Task W4.2：并行 SeqScan

**Files:**
- Modify: `engineering/src/db/sql/nodeSeqscan.c`
- Test: `engineering/test/db/sql/parallel_seqscan_test.cpp`

**目标：** N 个 worker 通过 `ParallelScanState` 分配页面范围，并行扫描

---

### Task W4.3：分区表创建

**Files:**
- Modify: `engineering/src/db/storage/catalog/catalog.c`
- Test: `engineering/test/db/storage/partition_create_test.cpp`

**目标：** `CREATE TABLE t (...) PARTITION BY RANGE (col)` + `pg_partition` 表

---

### Task W4.4：分区裁剪

**Files:**
- Modify: `engineering/src/db/sql/planner.c`
- Modify: `engineering/src/db/sql/nodeSeqscan.c`
- Test: `engineering/test/db/sql/partition_prune_test.cpp`

**目标：** planner 检查 WHERE 条件中的分区键，只扫描匹配分区

---

### Task W4.5：MergeJoin 算子

**Files:**
- Create: `engineering/src/db/sql/nodeMergejoin.c`
- Create: `engineering/include/db/sql/nodeMergejoin.h`
- Test: `engineering/test/db/sql/mergejoin_test.cpp`

**目标：** 排序后归并连接，同时扫描内外两个有序列表

---

## Wave 5：事务深度（4-5 周）

### Task W5.1：2PC PREPARE

**Files:**
- Modify: `engineering/src/db/storage/txn/txn_xact.c`
- Modify: `engineering/src/db/storage/wal/wal.c`
- Test: `engineering/test/db/storage/twophase_test.cpp`

**目标：** `PREPARE TRANSACTION` 写 WAL 日志，CLOG 标记 PREPARED

---

### Task W5.2：COMMIT/ROLLBACK PREPARED

**Files:**
- Modify: `engineering/src/db/storage/txn/txn_xact.c`
- Test: `engineering/test/db/storage/twophase_commit_test.cpp`

**目标：** `COMMIT PREPARED` / `ROLLBACK PREPARED`，崩溃恢复时自动处理

---

### Task W5.3：子事务/保存点

**Files:**
- Modify: `engineering/src/db/storage/txn/txn_xact.c`
- Test: `engineering/test/db/storage/subxact_test.cpp`

**目标：** SAVEPOINT + ROLLBACK TO SAVEPOINT + RELEASE SAVEPOINT

---

## Wave 6：高级特性（4-5 周）

### Task W6.1：WAL 段文件管理

**Files:**
- Modify: `engineering/src/db/storage/wal/wal.c`
- Test: `engineering/test/db/storage/wal_segment_test.cpp`

**目标：** 16MB 段文件，满时自动切换，文件名 `<timeline>_<segno>`

---

### Task W6.2：WAL 归档

**Files:**
- Create: `engineering/src/db/storage/wal/wal_archive.c`
- Test: `engineering/test/db/storage/wal_archive_test.cpp`

**目标：** `archive_command` 配置，段切换时执行归档

---

### Task W6.3：PITR 恢复

**Files:**
- Modify: `engineering/src/db/storage/wal/wal.c`
- Test: `engineering/test/db/storage/pitr_test.cpp`

**目标：** 从全量备份恢复，回放归档日志到目标时间点

---

### Task W6.4：表达式索引

**Files:**
- Modify: `engineering/src/db/storage/access/btree/btreeam.c`
- Test: `engineering/test/db/storage/expr_index_test.cpp`

**目标：** `CREATE INDEX ON t (lower(name))`，插入时对表达式求值

---

### Task W6.5：部分索引

**Files:**
- Modify: `engineering/src/db/storage/access/btree/btreeam.c`
- Test: `engineering/test/db/storage/partial_index_test.cpp`

**目标：** `CREATE INDEX ON t (col) WHERE col > 0`，插入时检查谓词

---

### Task W6.6：视图实现

**Files:**
- Modify: `engineering/src/db/storage/catalog/catalog.c`
- Modify: `engineering/src/db/sql/planner.c`
- Test: `engineering/test/db/sql/view_test.cpp`

**目标：** `CREATE VIEW` 存储查询定义，查询时展开为子查询

---

### Task W6.7：物化视图

**Files:**
- Modify: `engineering/src/db/storage/catalog/catalog.c`
- Test: `engineering/test/db/sql/matview_test.cpp`

**目标：** `CREATE MATERIALIZED VIEW` + `REFRESH`

---

### Task W6.8：规则系统

**Files:**
- Create: `engineering/src/db/sql/rewriter.c`
- Create: `engineering/include/db/sql/rewriter.h`
- Test: `engineering/test/db/sql/rewrite_test.cpp`

**目标：** `CREATE RULE`，ON SELECT/INSERT/UPDATE/DELETE 查询重写

---

### Task W6.9：继承表

**Files:**
- Modify: `engineering/src/db/storage/catalog/catalog.c`
- Test: `engineering/test/db/storage/inherit_test.cpp`

**目标：** `CREATE TABLE t (...) INHERITS (parent)`，查询父表包含子表

---

## 总计

| Wave | 任务数 | 新增文件 | 修改文件 | 周期 |
|------|--------|---------|---------|------|
| Wave 1 | 7 | 5 | 8 | 3-4 周 |
| Wave 2 | 7 | 3 | 5 | 4-5 周 |
| Wave 3 | 8 | 8 | 8 | 4-5 周 |
| Wave 4 | 5 | 3 | 5 | 4-5 周 |
| Wave 5 | 3 | 0 | 3 | 4-5 周 |
| Wave 6 | 9 | 3 | 6 | 4-5 周 |
| **合计** | **39** | **22** | **35** | **23-29 周** |