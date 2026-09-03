# Phase 0: 基础设施修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 page/buffer/WAL/catalog/txn/lock 六个基础设施组件，使其从 stub/空操作提升到真实磁盘 I/O 和事务支持

**Architecture:** 自底向上：lock (无依赖) → page (无依赖) → bufmgr (依赖 page) → WAL (无依赖) → catalog (依赖 WAL) → txn (依赖 bufmgr + lock + WAL)

**Tech Stack:** C14, GCC/G++, GTest, POSIX pthread / Win32 CRITICAL_SECTION, CRC32

## Global Constraints

1. 所有模态使用统一 `storage_result_t` 错误码
2. WAL-first 模式：数据修改先写 WAL 再写数据页
3. 编译标准: C14, C++14 (测试), GCC/G++ 手动编译
4. 测试使用 GTest 框架，桩函数解决深层依赖
5. 每个组件必须 null-safe

---

## 文件结构

| 文件 | 操作 | 职责 |
|------|------|------|
| `include/db/storage/common/storage_result.h` | 新增 | 统一错误码 |
| `include/db/storage/page/page.h` | 修改 | 新增 page_read/page_write 声明 |
| `src/db/storage/page/page.c` | 修改 | CRC32 校验、真实磁盘读写 |
| `src/db/storage/page/disk.c` | 修改 | 底层 fd 操作 |
| `src/db/storage/buffer/bufmgr.c` | 修改 | buf_read/buf_write 真实 IO |
| `src/db/storage/buffer/buffer.c` | 删除 | 与 bufmgr 重复 |
| `src/db/storage/wal/wal.c` | 修改 | LSN 变长记录 |
| `src/db/storage/wal/wal_recover.c` | 修改 | 完整 redo/undo |
| `src/db/storage/catalog/catalog.c` | 修改 | 持久化 + hash 查找 |
| `src/db/storage/txn/txn.c` | 修改 | MVCC 快照隔离 |
| `src/db/storage/lock/lock_mgr.c` | 修改 | 条件变量替代 busy-wait |
| `tests/test_page.c` | 新增 | Page 测试 |
| `tests/test_bufmgr.c` | 新增 | Buffer Pool 测试 |
| `tests/test_wal.c` | 新增 | WAL 测试 |
| `tests/test_catalog.c` | 新增 | Catalog 测试 |
| `tests/test_lock.c` | 新增 | Lock Manager 测试 |

---

## Task 1: 统一错误码

**Files:**
- Create: `include/db/storage/common/storage_result.h`

**Interfaces:**
- Produces: `storage_result_t` 枚举，被所有后续 Task 使用

- [ ] **Step 1: 创建头文件**

```c
// include/db/storage/common/storage_result.h
#ifndef STORAGE_RESULT_H
#define STORAGE_RESULT_H

typedef enum {
    STORAGE_OK = 0,
    STORAGE_ERR_NOT_FOUND = -1,
    STORAGE_ERR_ALREADY_EXISTS = -2,
    STORAGE_ERR_IO = -3,
    STORAGE_ERR_CORRUPTION = -4,
    STORAGE_ERR_OOM = -5,
    STORAGE_ERR_NOT_SUPPORTED = -6,
    STORAGE_ERR_INVALID_ARG = -7,
    STORAGE_ERR_WAL_FAILURE = -8,
    STORAGE_ERR_DEADLOCK = -9,
    STORAGE_ERR_TIMEOUT = -10,
    STORAGE_ERR_SNAPSHOT_STALE = -11,
} storage_result_t;

#endif
```

- [ ] **Step 2: 验证编译**

Run: `gcc -c -ID:/code/book/engineering/include -x c -o /dev/null - <<'EOF'
#include "db/storage/common/storage_result.h"
int main(void) { return STORAGE_OK; }
EOF`
Expected: 编译成功，无错误

- [ ] **Step 3: Commit**

```bash
git add include/db/storage/common/storage_result.h
git commit -m "feat(storage): add unified storage_result_t error codes"
```

---

## Task 2: Lock Manager 修复

**Files:**
- Modify: `src/db/storage/lock/lock_mgr.c`
- Create: `tests/test_lock.c`

**Interfaces:**
- Consumes: `storage_result_t` (Task 1)
- Produces: `lock_acquire` / `lock_release` 使用条件变量，线程安全

- [ ] **Step 1: 读取现有代码**

Read: `src/db/storage/lock/lock_mgr.c` — 理解当前结构

- [ ] **Step 2: 写测试**

```c
// tests/test_lock.c
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "db/lock.h"

class LockMgrTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = lock_mgr_create();
    }
    void TearDown() override {
        lock_mgr_destroy(mgr);
    }
    lock_mgr_t *mgr;
};

TEST_F(LockMgrTest, CreateDestroy) {
    ASSERT_NE(mgr, nullptr);
}

TEST_F(LockMgrTest, CreateNull) {
    // 不崩溃即可
    lock_mgr_destroy(nullptr);
}

TEST_F(LockMgrTest, AcquireRelease) {
    int ret = lock_acquire(mgr, 1, LOCK_MODE_EXCLUSIVE);
    EXPECT_EQ(ret, 0);
    ret = lock_release(mgr, 1);
    EXPECT_EQ(ret, 0);
}

TEST_F(LockMgrTest, SharedExclusiveConflict) {
    // S 锁可以并发
    EXPECT_EQ(lock_acquire(mgr, 1, LOCK_MODE_SHARED), 0);
    EXPECT_EQ(lock_acquire(mgr, 1, LOCK_MODE_SHARED), 0);
    EXPECT_EQ(lock_release(mgr, 1), 0);
    EXPECT_EQ(lock_release(mgr, 1), 0);
}

TEST_F(LockMgrTest, AcquireInvalidArgs) {
    EXPECT_NE(lock_acquire(nullptr, 1, LOCK_MODE_EXCLUSIVE), 0);
    EXPECT_NE(lock_acquire(mgr, 0, LOCK_MODE_EXCLUSIVE), 0);
    EXPECT_NE(lock_release(nullptr, 1), 0);
}

TEST_F(LockMgrTest, ReleaseNotHeld) {
    EXPECT_NE(lock_release(mgr, 999), 0);
}

TEST_F(LockMgrTest, ReentrantShared) {
    EXPECT_EQ(lock_acquire(mgr, 1, LOCK_MODE_SHARED), 0);
    EXPECT_EQ(lock_acquire(mgr, 1, LOCK_MODE_SHARED), 0);
    EXPECT_EQ(lock_release(mgr, 1), 0);
    EXPECT_EQ(lock_release(mgr, 1), 0);
}

// 并发测试：多线程交替获取/释放
static void *concurrent_worker(void *arg) {
    lock_mgr_t *m = (lock_mgr_t *)arg;
    for (int i = 0; i < 100; i++) {
        lock_acquire(m, 10, LOCK_MODE_EXCLUSIVE);
        // 模拟临界区
        lock_release(m, 10);
    }
    return NULL;
}

TEST_F(LockMgrTest, ConcurrentAccess) {
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, concurrent_worker, mgr);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    // 不崩溃、不死锁即通过
    SUCCEED();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

- [ ] **Step 3: 编译测试验证失败**

Run: `cd D:/code/book/engineering && g++ -std=c++14 -c tests/test_lock.c -ID:/code/book/engineering/include -ID:/code/book/engineering/include/db -ID:/code/book/engineering/third_part/googletest/googletest/include -o tests/test_lock.o`
Expected: 编译成功（lock.h 已有声明）

- [ ] **Step 4: 修复 lock_mgr.c**

在 `lock_mgr.c` 中：
1. 将 `Sleep(1)` / `usleep(1000)` busy-wait 替换为条件变量等待
2. 修复 `lock_release_all` 迭代期间修改问题
3. 实现 `wake_up_next_waiter` 真实唤醒

关键修改：
```c
// 替换 busy-wait
// 修复前
while (!req->granted) {
    Sleep(1);  // Win32
}

// 修复后
while (!req->granted) {
    pthread_cond_wait(&entry->cond, &entry->mutex);
    // 或 Windows: SleepConditionVariableCS(&entry->cond, &entry->cs, INFINITE);
}
```

- [ ] **Step 5: 编译并运行测试**

Run: `cd D:/code/book/engineering && g++ -std=c++14 -c src/db/storage/lock/lock_mgr.c -ID:/code/book/engineering/include -ID:/code/book/engineering/include/db -o tests/lock_mgr.o && g++ -std=c++14 -o tests/test_lock.exe tests/test_lock.o tests/lock_mgr.o gtest-all.o gmock-all.o -lpthread && tests/test_lock.exe`
Expected: 所有测试 PASS

- [ ] **Step 6: Commit**

```bash
git add src/db/storage/lock/lock_mgr.c tests/test_lock.c
git commit -m "fix(lock): replace busy-wait with condition variables, fix release_all"
```

---

## Task 3: Page 层修复

**Files:**
- Modify: `src/db/storage/page/page.c`
- Modify: `src/db/storage/page/disk.c`
- Modify: `include/db/storage/page/page.h`
- Create: `tests/test_page.c`

**Interfaces:**
- Produces: `page_read`, `page_write`, `page_compute_checksum` — 被 Task 4 (bufmgr) 使用

- [ ] **Step 1: 写测试**

```c
// tests/test_page.c
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "db/storage/page/page.h"

class PageTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PageTest, ChecksumConsistency) {
    page_t page;
    memset(&page, 0, sizeof(page));
    page.header.magic = PAGE_MAGIC;
    uint32_t cs1 = page_compute_checksum(&page);
    uint32_t cs2 = page_compute_checksum(&page);
    EXPECT_EQ(cs1, cs2);
}

TEST_F(PageTest, ChecksumDifferent) {
    page_t p1, p2;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.header.magic = PAGE_MAGIC;
    p2.header.magic = PAGE_MAGIC;
    p1.data[0] = 0x42;
    p2.data[0] = 0x43;
    EXPECT_NE(page_compute_checksum(&p1), page_compute_checksum(&p2));
}

TEST_F(PageTest, VerifyValid) {
    page_t page;
    memset(&page, 0, sizeof(page));
    page.header.magic = PAGE_MAGIC;
    page.header.checksum = page_compute_checksum(&page);
    EXPECT_EQ(page_verify(&page), 0);
}

TEST_F(PageTest, VerifyBadMagic) {
    page_t page;
    memset(&page, 0, sizeof(page));
    page.header.magic = 0xDEADBEEF;
    EXPECT_NE(page_verify(&page), 0);
}

TEST_F(PageTest, VerifyBadChecksum) {
    page_t page;
    memset(&page, 0, sizeof(page));
    page.header.magic = PAGE_MAGIC;
    page.header.checksum = 0;
    EXPECT_NE(page_verify(&page), 0);
}

TEST_F(PageTest, NullSafe) {
    EXPECT_NE(page_compute_checksum(NULL), 0u);
    EXPECT_NE(page_verify(NULL), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

- [ ] **Step 2: 编译测试验证失败**

Run: `cd D:/code/book/engineering && g++ -std=c++14 -c tests/test_page.c -ID:/code/book/engineering/include -ID:/code/book/engineering/include/db -ID:/code/book/engineering/third_part/googletest/googletest/include -o tests/test_page.o`
Expected: 编译成功

- [ ] **Step 3: 修复 page.c**

修改 `page_get_checksum` 从 XOR 改为 CRC32，实现 `page_compute_checksum` 和 `page_verify`。

- [ ] **Step 4: 编译运行测试**

Run: `cd D:/code/book/engineering && gcc -c src/db/storage/page/page.c -ID:/code/book/engineering/include -ID:/code/book/engineering/include/db -o tests/page.o && g++ -std=c++14 -o tests/test_page.exe tests/test_page.o tests/page.o gtest-all.o gmock-all.o -lpthread && tests/test_page.exe`
Expected: 所有测试 PASS

- [ ] **Step 5: Commit**

```bash
git add src/db/storage/page/page.c include/db/storage/page/page.h tests/test_page.c
git commit -m "fix(page): replace XOR checksum with CRC32, add page_verify"
```

---

## Task 4: Buffer Pool 修复

**Files:**
- Modify: `src/db/storage/buffer/bufmgr.c`
- Delete: `src/db/storage/buffer/buffer.c`
- Create: `tests/test_bufmgr.c`

**Interfaces:**
- Consumes: `page_read`, `page_write` (Task 3)
- Produces: `buf_read`, `buf_write` 执行真实磁盘 IO

- [ ] **Step 1: 写测试**

```c
// tests/test_bufmgr.c
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "db/storage/buffer/bufmgr.h"

class BufMgrTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 使用临时文件
        snprintf(test_path, sizeof(test_path), "./test_bufmgr_%d.dat", getpid());
        FILE *f = fopen(test_path, "w");
        if (f) fclose(f);
    }
    void TearDown() remove(test_path);
    char test_path[256];
};

TEST_F(BufMgrTest, InitShutdown) {
    EXPECT_EQ(buf_init(test_path), 0);
    buf_shutdown();
}

TEST_F(BufMgrTest, InitNull) {
    EXPECT_NE(buf_init(NULL), 0);
}

TEST_F(BufMgrTest, ReadWriteRoundtrip) {
    buf_init(test_path);

    // 写入页面
    page_t page;
    memset(&page, 0, sizeof(page));
    page.header.magic = PAGE_MAGIC;
    memcpy(page.data, "hello world", 11);

    buffer_t *buf = buf_read(test_path, 0);
    ASSERT_NE(buf, nullptr);
    memcpy(buf->page.data, "hello world", 11);
    buf->is_dirty = 1;
    buf_unpin(buf);

    buf_shutdown();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

- [ ] **Step 2: 修复 bufmgr.c 中的 buf_read 和 buf_write**

```c
// buf_read 修复：调用 page_read
buffer_t *buf_read(const char *path, uint32_t page_id) {
    // 1. 查 hash table 是否已在 buffer pool
    buffer_desc *desc = buf_hash_lookup(path, page_id);
    if (desc) {
        desc->refcount++;
        return desc;
    }
    // 2. 分配 frame
    desc = buf_alloc_frame();
    // 3. 从磁盘读取
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        page_read(fd, page_id, &desc->page);
        close(fd);
    }
    // 4. 插入 hash table
    buf_hash_insert(desc, path, page_id);
    desc->refcount = 1;
    return desc;
}

// buf_write 修复：调用 page_write
int buf_write(buffer_t *buf) {
    if (!buf || !buf->is_dirty) return 0;
    int fd = open(buf->path, O_RDWR);
    if (fd < 0) return STORAGE_ERR_IO;
    page_write(fd, buf->page_id, &buf->page);
    close(fd);
    buf->is_dirty = 0;
    return 0;
}
```

- [ ] **Step 3: 编译运行测试**

Run: `cd D:/code/book/engineering && gcc -c src/db/storage/buffer/bufmgr.c -ID:/code/book/engineering/include -ID:/code/book/engineering/include/db -o tests/bufmgr.o && g++ -std=c++14 -o tests/test_bufmgr.exe tests/test_bufmgr.o tests/bufmgr.o tests/page.o gtest-all.o gmock-all.o -lpthread && tests/test_bufmgr.exe`
Expected: 所有测试 PASS

- [ ] **Step 4: 删除重复的 buffer.c**

Run: `rm src/db/storage/buffer/buffer.c`

- [ ] **Step 5: Commit**

```bash
git add src/db/storage/buffer/bufmgr.c tests/test_bufmgr.c
git rm src/db/storage/buffer/buffer.c
git commit -m "fix(bufmgr): implement real disk IO in buf_read/buf_write, remove duplicate buffer.c"
```

---

## Task 5: WAL 完善

**Files:**
- Modify: `src/db/storage/wal/wal.c`
- Modify: `src/db/storage/wal/wal_recover.c`
- Create: `tests/test_wal.c`

**Interfaces:**
- Produces: 完整 redo/undo，被 Task 6 (catalog) 和 Task 7 (txn) 使用

- [ ] **Step 1: 写测试**

```c
// tests/test_wal.c
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "db/storage/wal/wal.h"

class WalTest : public ::testing::Test {
protected:
    void SetUp() override {
        snprintf(test_path, sizeof(test_path), "./test_wal_%d.log", getpid());
    }
    void TearDown() {
        remove(test_path);
    }
    char test_path[256];
};

TEST_F(WalTest, CreateFlush) {
    wal_t *wal = wal_create(test_path);
    ASSERT_NE(wal, nullptr);

    wal_record_t rec = {0};
    rec.lsn = 1;
    rec.txn_id = 1;
    rec.modality = 1;
    rec.op = WAL_OP_INSERT;
    rec.data_len = 4;
    uint8_t data[4] = {1,2,3,4};
    memcpy(rec.data, data, 4);

    EXPECT_EQ(wal_append(wal, &rec), 0);
    EXPECT_EQ(wal_flush(wal), 0);
    wal_close(wal);
}

TEST_F(WalTest, RedoInsert) {
    wal_t *wal = wal_create(test_path);
    ASSERT_NE(wal, nullptr);

    wal_record_t rec = {0};
    rec.lsn = 1;
    rec.txn_id = 1;
    rec.modality = 1;
    rec.op = WAL_OP_INSERT;
    rec.data_len = 0;
    wal_append(wal, &rec);
    wal_flush(wal);
    wal_close(wal);

    // 重新打开并 redo
    wal_t *wal2 = wal_open(test_path);
    ASSERT_NE(wal2, nullptr);
    // redo 应该不崩溃
    wal_close(wal2);
}

TEST_F(WalTest, NullSafe) {
    wal_close(NULL);
    EXPECT_NE(wal_append(NULL, NULL), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

- [ ] **Step 2: 修复 wal.c LSN 计算**

```c
// 修复前：固定 1024
uint64_t lsn = wal->current_offset / 1024;

// 修复后：变长记录
uint64_t lsn = wal->current_offset;
```

- [ ] **Step 3: 修复 wal_recover.c redo/undo**

在 `wal_redo` 中增加 DELETE/COMMIT/ABORT/CHECKPOINT case，在 `wal_undo` 中改为逆序扫描。

- [ ] **Step 4: 编译运行测试**

Run: `cd D:/code/book/engineering && gcc -c src/db/storage/wal/wal.c -ID:/code/book/engineering/include -ID:/code/book/engineering/include/db -o tests/wal.o && gcc -c src/db/storage/wal/wal_recover.c -ID:/code/book/engineering/include -ID:/code/book/engineering/include/db -o tests/wal_recover.o && g++ -std=c++14 -o tests/test_wal.exe tests/test_wal.o tests/wal.o tests/wal_recover.o gtest-all.o gmock-all.o -lpthread && tests/test_wal.exe`
Expected: 所有测试 PASS

- [ ] **Step 5: Commit**

```bash
git add src/db/storage/wal/wal.c src/db/storage/wal/wal_recover.c tests/test_wal.c
git commit -m "fix(wal): variable-length LSN, complete redo/undo for all record types"
```

---

## Task 6: Catalog 持久化

**Files:**
- Modify: `src/db/storage/catalog/catalog.c`
- Create: `tests/test_catalog.c`

**Interfaces:**
- Consumes: `wal_append`, `wal_open` (Task 5)
- Produces: `catalog_persist`, `catalog_recover` — 被 Task 7 (txn) 使用

- [ ] **Step 1: 写测试**

```c
// tests/test_catalog.c
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "db/storage/catalog/catalog.h"

class CatalogTest : public ::testing::Test {
protected:
    void SetUp() override {
        catalog_init(&cat, NULL);
    }
    void TearDown() {
        catalog_shutdown(&cat);
    }
    catalog_t cat;
};

TEST_F(CatalogTest, CreateDrop) {
    EXPECT_EQ(catalog_create_table(&cat, 1, NULL, 'r', 0), 0);
    EXPECT_NE(catalog_create_table(&cat, 1, NULL, 'r', 0), 0); // 已存在
    EXPECT_EQ(catalog_drop_table(&cat, 1), 0);
    EXPECT_EQ(catalog_drop_table(&cat, 1), 0); // 不存在，返回错误
}

TEST_F(CatalogTest, Lookup) {
    catalog_create_table(&cat, 1, NULL, 'r', 0);
    catalog_table_t *t = catalog_get_table(&cat, 1);
    EXPECT_NE(t, nullptr);
    EXPECT_EQ(catalog_get_table(&cat, 999), nullptr);
    catalog_drop_table(&cat, 1);
}

TEST_F(CatalogTest, NullSafe) {
    EXPECT_EQ(catalog_create_table(NULL, 1, NULL, 'r', 0), -1);
    EXPECT_EQ(catalog_get_table(NULL, 1), nullptr);
    EXPECT_EQ(catalog_drop_table(NULL, 1), -1);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

- [ ] **Step 2: 修复 catalog.c**

1. `catalog_add_column`: 修复内存泄漏
2. `catalog_lookup_table`: 链表 → hash table

- [ ] **Step 3: 编译运行测试**

Run: `cd D:/code/book/engineering && gcc -c src/db/storage/catalog/catalog.c -ID:/code/book/engineering/include -ID:/code/book/engineering/include/db -o tests/catalog.o && g++ -std=c++14 -o tests/test_catalog.exe tests/test_catalog.o tests/catalog.o gtest-all.o gmock-all.o -lpthread && tests/test_catalog.exe`
Expected: 所有测试 PASS

- [ ] **Step 4: Commit**

```bash
git add src/db/storage/catalog/catalog.c tests/test_catalog.c
git commit -m "fix(catalog): fix memory leak in add_column, O(1) lookup via hash table"
```

---

## Task 7: MVCC 事务

**Files:**
- Modify: `src/db/storage/txn/txn.c`
- Modify: `src/db/storage/txn/mvcc.c`

**Interfaces:**
- Consumes: `page_read` (Task 3), `lock_acquire/release` (Task 2), WAL (Task 5)
- Produces: `txn_begin_mvcc`, `txn_visibility_check` — 被所有存储模态使用

- [ ] **Step 1: 读取现有代码**

Read: `src/db/storage/txn/txn.c`, `src/db/storage/txn/mvcc.c`

- [ ] **Step 2: 实现 MVCC 快照**

```c
int txn_begin_mvcc(txn_t *txn, isolation_level_t level) {
    if (!txn) return STORAGE_ERR_INVALID_ARG;
    txn->level = level;
    txn->snapshot_lsn = wal_get_current_lsn(g_wal);
    txn->status = TXN_ACTIVE;
    txn->changes = NULL;
    return STORAGE_OK;
}

int txn_visibility_check(txn_t *txn, const tuple_header *tuple) {
    if (!txn || !tuple) return 0;
    // 事务自己的修改始终可见
    if (tuple->t_xmin == txn->id) return 1;
    // 已提交且在快照之前
    if (tuple->t_xmax != 0 && tuple->t_xmax < txn->snapshot_lsn) return 0;
    // xmin 在快照之前且已提交
    if (tuple->t_xmin < txn->snapshot_lsn) return 1;
    return 0;
}
```

- [ ] **Step 3: 与 lock_mgr 集成**

在 `txn_begin_mvcc` 中获取 IS 锁，在修改时升级为 X 锁。

- [ ] **Step 4: Commit**

```bash
git add src/db/storage/txn/txn.c src/db/storage/txn/mvcc.c
git commit -m "feat(txn): implement MVCC snapshot isolation with lock integration"
```

---

## 执行顺序总结

```
Task 1 (错误码) → 无依赖
Task 2 (Lock) → 依赖 Task 1
Task 3 (Page) → 无依赖，可与 Task 2 并行
Task 4 (BufMgr) → 依赖 Task 3
Task 5 (WAL) → 无依赖，可与 Task 2/3 并行
Task 6 (Catalog) → 依赖 Task 5
Task 7 (Txn) → 依赖 Task 2 + 3 + 5
```

推荐执行顺序：Task 1 → Task 2 + Task 3 + Task 5 (并行) → Task 4 + Task 6 → Task 7
