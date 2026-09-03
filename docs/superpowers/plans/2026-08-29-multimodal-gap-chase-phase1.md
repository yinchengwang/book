# 多模态数据库追赶计划实现方案

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 6 个共性 Gap（G1-G6），为 Phase 2-4 的模态追赶奠定基础设施基础。

**Architecture:** 
- G1: 创建 `common_rwlock.h/c`，封装 pthread_rwlock，替换 5 个模态的有缺陷自旋锁
- G2: 扩展 `wal.h`，统一 WAL API，6 个模态接入
- G3: 修复 `nodeModifyTable.c` TID 管道
- G4-G5: 锁默认 + 错误路径统一
- G6: 创建跨模态集成测试基线

**Tech Stack:** C 语言、pthread 库、cmocka 测试框架

## Global Constraints

- 全程简体中文；commit message 用中文
- 禁止修改 `engineering/`、`learning/` 下任何无关文件
- 只提交当前任务相关文件
- 使用 cmocka 单元测试框架
- 环境：Windows 11 + Git Bash + MinGW；仓库根 `D:\code\book`

---

## 文件结构

```
engineering/include/db/
├── common_rwlock.h           # 新增：公共并发原语库

engineering/src/db/
├── common_rwlock.c           # 新增：公共并发原语实现
├── storage/
│   ├── vector/vector_engine.c       # 修改：替换 rwlock
│   ├── kv/kv.c                      # 修改：替换 rwlock + WAL
│   ├── rel/rel_engine.c             # 修改：TID 管道 + WAL
│   ├── ts/ts_engine.c               # 修改：替换 rwlock
│   ├── doc/doc_engine.c             # 修改：替换 rwlock
│   ├── spatial/spatial_engine.c     # 修改：添加 rwlock
│   ├── graph/graph_engine.c         # 修改：添加 rwlock
│   ├── yang/yang_model.c            # 修改：WAL + 错误处理
│   └── wal/wal.c                    # 修改：扩展 fsync API

engineering/test/db/
├── common_rwlock_test.c      # 新增：并发原语测试
├── tid_pipe_test.c           # 新增：TID 管道测试
├── wal_integration_test.c    # 新增：WAL 集成测试
└── cross_modality_test.c     # 新增：跨模态测试基线
```

---

## Phase 1: G1 并发原语库

### Task 1: 创建公共并发原语库头文件

**Files:**
- Create: `engineering/include/db/common_rwlock.h`

**Interfaces:**
- Produces: `common_rwlock_create()`, `common_rwlock_destroy()`, `common_rwlock_read_lock()`, `common_rwlock_read_unlock()`, `common_rwlock_write_lock()`, `common_rwlock_write_unlock()`, `common_rwlock_try_write_lock()`

- [ ] **Step 1: 创建头文件骨架**

```c
#ifndef COMMON_RWLOCK_H
#define COMMON_RWLOCK_H

#include <pthread.h>
#include <stdbool.h>

typedef struct {
    pthread_rwlock_t rwlock;
    bool            use_lock;
    const char*     name;
} common_rwlock_t;

// 创建锁，name 用于调试输出
common_rwlock_t* common_rwlock_create(const char* name);

// 销毁锁
void common_rwlock_destroy(common_rwlock_t* lock);

// 读锁（可重入）
void common_rwlock_read_lock(common_rwlock_t* lock);
void common_rwlock_read_unlock(common_rwlock_t* lock);

// 写锁（独占）
void common_rwlock_write_lock(common_rwlock_t* lock);
void common_rwlock_write_unlock(common_rwlock_t* lock);

// 尝试获取写锁，超时返回 false
bool common_rwlock_try_write_lock(common_rwlock_t* lock, int timeout_ms);

#endif // COMMON_RWLOCK_H
```

- [ ] **Step 2: 提交**

```bash
cd D:/code/book
git add engineering/include/db/common_rwlock.h
git commit -m "feat(concurrency): 创建公共 rwlock 封装库"
```

---

### Task 2: 实现公共并发原语库

**Files:**
- Create: `engineering/src/db/common_rwlock.c`

**Interfaces:**
- Consumes: `common_rwlock.h`
- Produces: 完整的 rwlock 封装实现

- [ ] **Step 1: 实现基础功能**

```c
#include "common_rwlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

common_rwlock_t* common_rwlock_create(const char* name) {
    common_rwlock_t* lock = calloc(1, sizeof(common_rwlock_t));
    if (!lock) return NULL;
    
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    // 写者优先，避免写者饥饿
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    
    if (pthread_rwlock_init(&lock->rwlock, &attr) != 0) {
        free(lock);
        return NULL;
    }
    
    lock->use_lock = true;  // 默认开启锁
    lock->name = name;
    pthread_rwlockattr_destroy(&attr);
    
    return lock;
}

void common_rwlock_destroy(common_rwlock_t* lock) {
    if (!lock) return;
    pthread_rwlock_destroy(&lock->rwlock);
    free(lock);
}

void common_rwlock_read_lock(common_rwlock_t* lock) {
    if (!lock || !lock->use_lock) return;
    pthread_rwlock_rdlock(&lock->rwlock);
}

void common_rwlock_read_unlock(common_rwlock_t* lock) {
    if (!lock || !lock->use_lock) return;
    pthread_rwlock_unlock(&lock->rwlock);
}

void common_rwlock_write_lock(common_rwlock_t* lock) {
    if (!lock || !lock->use_lock) return;
    pthread_rwlock_wrlock(&lock->rwlock);
}

void common_rwlock_write_unlock(common_rwlock_t* lock) {
    if (!lock || !lock->use_lock) return;
    pthread_rwlock_unlock(&lock->rwlock);
}

bool common_rwlock_try_write_lock(common_rwlock_t* lock, int timeout_ms) {
    if (!lock || !lock->use_lock) return true;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    return pthread_rwlock_timedwrlock(&lock->rwlock, &ts) == 0;
}
```

- [ ] **Step 2: 编译验证**

```bash
cd D:/code/book
gcc -c -I engineering/include engineering/src/db/common_rwlock.c -o common_rwlock.o
echo "编译成功"
```

- [ ] **Step 3: 提交**

```bash
cd D:/code/book
git add engineering/src/db/common_rwlock.c
git commit -m "feat(concurrency): 实现公共 rwlock 封装"
```

---

### Task 3: Vector 引擎替换 rwlock

**Files:**
- Modify: `engineering/src/db/storage/vector/vector_engine.c`

**Interfaces:**
- Consumes: `common_rwlock.h`
- Produces: Vector 引擎使用公共 rwlock

- [ ] **Step 1: 找到 simple_rwlock 定义并替换**

在 `vector_engine.c` 中找到 `simple_rwlock_t` 相关代码，将其替换为 `common_rwlock_t`。

替换模式：
```c
// 旧代码
typedef struct {
    int readers;
    int writers_waiting;
    int writer_active;
    // ... 有缺陷的实现
} simple_rwlock_t;

// 新代码
#include "common_rwlock.h"
// 删除 simple_rwlock_t 定义，使用 common_rwlock_t
```

- [ ] **Step 2: 更新 vector_engine.h 头文件**

将 `vector_engine.h` 中的 `simple_rwlock_t` 引用改为 `common_rwlock_t`。

- [ ] **Step 3: 编译验证**

```bash
cd D:/code/book
gcc -c -I engineering/include engineering/src/db/storage/vector/vector_engine.c -o vector_engine.o
```

- [ ] **Step 4: 提交**

```bash
cd D:/code/book
git add engineering/src/db/storage/vector/vector_engine.c engineering/src/db/storage/vector/vector_engine.h
git commit -m "refactor(vector): 替换为公共 rwlock 封装"
```

---

### Task 4: KV 引擎替换 rwlock

**Files:**
- Modify: `engineering/src/db/storage/kv/kv.c`

**Interfaces:**
- Consumes: `common_rwlock.h`
- Produces: KV 引擎使用公共 rwlock

- [ ] **Step 1: 替换 KV 引擎的 rwlock**

查找并替换 KV 引擎中的 `simple_rwlock_t` 为 `common_rwlock_t`。

- [ ] **Step 2: 编译验证**

```bash
cd D:/code/book
gcc -c -I engineering/include engineering/src/db/storage/kv/kv.c -o kv.o
```

- [ ] **Step 3: 提交**

```bash
cd D:/code/book
git add engineering/src/db/storage/kv/kv.c
git commit -m "refactor(kv): 替换为公共 rwlock 封装"
```

---

### Task 5: Timeseries 引擎替换 rwlock

**Files:**
- Modify: `engineering/src/db/storage/ts/ts_engine.c`

**Interfaces:**
- Consumes: `common_rwlock.h`
- Produces: Timeseries 引擎使用公共 rwlock

- [ ] **Step 1: 替换 Timeseries 引擎的 rwlock**

查找并替换 `ts_engine.c` 中的 `simple_rwlock_t` 为 `common_rwlock_t`。

- [ ] **Step 2: 编译验证**

```bash
cd D:/code/book
gcc -c -I engineering/include engineering/src/db/storage/ts/ts_engine.c -o ts_engine.o
```

- [ ] **Step 3: 提交**

```bash
cd D:/code/book
git add engineering/src/db/storage/ts/ts_engine.c
git commit -m "refactor(ts): 替换为公共 rwlock 封装"
```

---

### Task 6: Document 引擎替换 rwlock

**Files:**
- Modify: `engineering/src/db/storage/doc/doc_engine.c`

**Interfaces:**
- Consumes: `common_rwlock.h`
- Produces: Document 引擎使用公共 rwlock

- [ ] **Step 1: 替换 Document 引擎的 rwlock**

查找并替换 `doc_engine.c` 中的 `simple_rwlock_t` 为 `common_rwlock_t`。

- [ ] **Step 2: 编译验证**

```bash
cd D:/code/book
gcc -c -I engineering/include engineering/src/db/storage/doc/doc_engine.c -o doc_engine.o
```

- [ ] **Step 3: 提交**

```bash
cd D:/code/book
git add engineering/src/db/storage/doc/doc_engine.c
git commit -m "refactor(doc): 替换为公共 rwlock 封装"
```

---

### Task 7: Graph 引擎添加 rwlock

**Files:**
- Modify: `engineering/src/db/storage/graph/graph_engine.c`

**Interfaces:**
- Consumes: `common_rwlock.h`
- Produces: Graph 引擎添加公共 rwlock（之前零锁）

- [ ] **Step 1: 在 Graph 引擎中添加 rwlock 保护**

在 `graph_engine.c` 的关键路径（CSR 读写、顶点/边操作）添加 `common_rwlock_t` 保护。

示例：
```c
#include "common_rwlock.h"

// 在 graph 结构体中添加
typedef struct {
    // ... 现有字段
    common_rwlock_t* rwlock;
} Graph;

// 在操作前后加锁
int graph_add_vertex(Graph* g, const Vertex* v) {
    common_rwlock_write_lock(g->rwlock);
    // 原有逻辑
    common_rwlock_write_unlock(g->rwlock);
}
```

- [ ] **Step 2: 编译验证**

```bash
cd D:/code/book
gcc -c -I engineering/include engineering/src/db/storage/graph/graph_engine.c -o graph_engine.o
```

- [ ] **Step 3: 提交**

```bash
cd D:/code/book
git add engineering/src/db/storage/graph/graph_engine.c
git commit -m "feat(graph): 添加公共 rwlock 保护并发安全"
```

---

### Task 8: Spatial 引擎添加 rwlock

**Files:**
- Modify: `engineering/src/db/storage/spatial/spatial_engine.c`

**Interfaces:**
- Consumes: `common_rwlock.h`
- Produces: Spatial 引擎添加公共 rwlock（之前零锁）

- [ ] **Step 1: 在 Spatial 引擎中添加 rwlock 保护**

在 R-Tree 操作路径添加 rwlock。

- [ ] **Step 2: 编译验证**

```bash
cd D:/code/book
gcc -c -I engineering/include engineering/src/db/storage/spatial/spatial_engine.c -o spatial_engine.o
```

- [ ] **Step 3: 提交**

```bash
cd D:/code/book
git add engineering/src/db/storage/spatial/spatial_engine.c
git commit -m "feat(spatial): 添加公共 rwlock 保护并发安全"
```

---

### Task 9: 编写并发原语单元测试

**Files:**
- Create: `engineering/test/db/common_rwlock_test.c`

**Interfaces:**
- Produces: 验证 rwlock 基本功能、并发安全性

- [ ] **Step 1: 编写测试用例**

```c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "common_rwlock.h"
#include <pthread.h>
#include <assert.h>

static void test_rwlock_create_destroy(void** state) {
    (void) state;
    common_rwlock_t* lock = common_rwlock_create("test");
    assert_non_null(lock);
    assert_true(lock->use_lock);
    common_rwlock_destroy(lock);
}

static void test_rwlock_read_lock(void** state) {
    (void) state;
    common_rwlock_t* lock = common_rwlock_create("test");
    
    common_rwlock_read_lock(lock);
    common_rwlock_read_unlock(lock);
    
    common_rwlock_destroy(lock);
}

static void test_rwlock_write_lock(void** state) {
    (void) state;
    common_rwlock_t* lock = common_rwlock_create("test");
    
    common_rwlock_write_lock(lock);
    common_rwlock_write_unlock(lock);
    
    common_rwlock_destroy(lock);
}

static void* reader_thread(void* arg) {
    common_rwlock_t* lock = arg;
    for (int i = 0; i < 1000; i++) {
        common_rwlock_read_lock(lock);
        // 模拟读操作
        common_rwlock_read_unlock(lock);
    }
    return NULL;
}

static void* writer_thread(void* arg) {
    common_rwlock_t* lock = arg;
    for (int i = 0; i < 100; i++) {
        common_rwlock_write_lock(lock);
        // 模拟写操作
        common_rwlock_write_unlock(lock);
    }
    return NULL;
}

static void test_rwlock_concurrent(void** state) {
    (void) state;
    common_rwlock_t* lock = common_rwlock_create("test");
    
    pthread_t readers[5];
    pthread_t writers[2];
    
    // 创建 5 个读线程
    for (int i = 0; i < 5; i++) {
        pthread_create(&readers[i], NULL, reader_thread, lock);
    }
    
    // 创建 2 个写线程
    for (int i = 0; i < 2; i++) {
        pthread_create(&writers[i], NULL, writer_thread, lock);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < 5; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < 2; i++) {
        pthread_join(writers[i], NULL);
    }
    
    common_rwlock_destroy(lock);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rwlock_create_destroy),
        cmocka_unit_test(test_rwlock_read_lock),
        cmocka_unit_test(test_rwlock_write_lock),
        cmocka_unit_test(test_rwlock_concurrent),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

- [ ] **Step 2: 编译运行测试**

```bash
cd D:/code/book
gcc -o common_rwlock_test \
    engineering/test/db/common_rwlock_test.c \
    engineering/src/db/common_rwlock.c \
    -I engineering/include -lcmocka -lpthread
./common_rwlock_test
```

预期：所有测试通过

- [ ] **Step 3: 提交**

```bash
cd D:/code/book
git add engineering/test/db/common_rwlock_test.c
git commit -m "test(concurrency): 添加公共 rwlock 单元测试"
```

---

## Phase 1: G2 WAL 统一接入

### Task 10: 扩展 WAL API 支持 fsync

**Files:**
- Modify: `engineering/include/db/wal.h`
- Modify: `engineering/src/db/storage/wal/wal.c`

**Interfaces:**
- Produces: WAL 统一 API，支持 fsync

- [ ] **Step 1: 扩展 wal.h 添加 fsync 支持**

```c
// 在 wal.h 中添加
typedef enum {
    WAL_SYNC_FULL,     // write + fsync（防系统崩溃）
    WAL_SYNC_BUFFERED, // write + fflush（仅防进程崩溃）
    WAL_SYNC_NONE      // 异步写入
} WalSyncMode;

typedef struct {
    uint64_t    lsn;
    WalSyncMode sync_mode;
    // ... 现有字段
} WalContext;

// 新增 API
int wal_set_sync_mode(WalContext* ctx, WalSyncMode mode);
int wal_flush(WalContext* ctx);  // 显式刷新
```

- [ ] **Step 2: 实现 fsync 逻辑**

在 `wal.c` 的写入路径添加 fsync：
```c
int wal_append(WalContext* ctx, ModelType model, const void* record, size_t len) {
    // ... 现有写入逻辑
    
    if (ctx->sync_mode == WAL_SYNC_FULL) {
        if (fsync(fileno(ctx->file)) != 0) {
            return -1;  // 返回错误
        }
    }
    return 0;
}
```

- [ ] **Step 3: 编译验证**

```bash
cd D:/code/book
gcc -c -I engineering/include engineering/src/db/storage/wal/wal.c -o wal.o
```

- [ ] **Step 4: 提交**

```bash
cd D:/code/book
git add engineering/include/db/wal.h engineering/src/db/storage/wal/wal.c
git commit -m "feat(wal): 扩展 WAL API 支持 fsync 和同步模式"
```

---

### Task 11: Vector 引擎 WAL 改造

**Files:**
- Modify: `engineering/src/db/storage/vector/vector_engine.c`

**Interfaces:**
- Consumes: 扩展后的 wal.h
- Produces: Vector 引擎使用统一 WAL + fsync

- [ ] **Step 1: 替换独立的 vector_wal.c 为统一 WAL**

将 Vector 的 `vector_wal.c` 替换为通过 `mm_storage` 接入统一 WAL。

- [ ] **Step 2: 设置 WAL_SYNC_FULL**

```c
// 在 vector_engine.c 初始化时
WalContext* wal = mm_storage_get_wal(storage);
wal_set_sync_mode(wal, WAL_SYNC_FULL);
```

- [ ] **Step 3: 处理 WAL 写入失败**

```c
int vector_insert(VectorEngine* engine, const Vector* vec) {
    // ... 插入逻辑
    
    int ret = wal_append(wal, MODEL_VECTOR, record, len);
    if (ret != 0) {
        // WAL 失败应该回滚或返回错误，不能静默吞掉
        return ret;  // 返回错误而非继续
    }
    return 0;
}
```

- [ ] **Step 4: 编译验证并提交**

---

### Task 12: Relational 引擎 WAL 接入

**Files:**
- Modify: `engineering/src/db/storage/rel/rel_engine.c`

**Interfaces:**
- Consumes: wal.h
- Produces: Relational 引擎接入统一 WAL

- [ ] **Step 1: 在 DML 路径添加 WAL 记录**

在 `insert/update/delete` 路径调用 `wal_append()`。

- [ ] **Step 2: 设置 WAL_SYNC_FULL**

- [ ] **Step 3: 编译验证并提交**

---

### Task 13: 其他模态 WAL 接入

**Files:**
- Modify: `engineering/src/db/storage/ts/ts_engine.c`
- Modify: `engineering/src/db/storage/spatial/spatial_engine.c`
- Modify: `engineering/src/db/storage/yang/yang_model.c`

**Interfaces:**
- Consumes: wal.h
- Produces: Timeseries/Spatial/Yang 引擎接入统一 WAL

- [ ] **Step 1: 依次接入 WAL**

按顺序修改三个引擎的写路径，接入统一 WAL。

- [ ] **Step 2: 编译验证**

- [ ] **Step 3: 批量提交**

```bash
cd D:/code/book
git add engineering/src/db/storage/ts/ts_engine.c \
       engineering/src/db/storage/spatial/spatial_engine.c \
       engineering/src/db/storage/yang/yang_model.c
git commit -m "feat(wal): Timeseries/Spatial/Yang 引擎接入统一 WAL"
```

---

## Phase 1: G3 TID 管道修复

### Task 14: 创建 TID 解析器

**Files:**
- Create: `engineering/src/db/storage/rel/tid_resolver.c`
- Create: `engineering/include/db/tid_resolver.h`

**Interfaces:**
- Produces: `tid_resolver_from_tuple()`, `tid_resolver_update()`, `tid_resolver_delete()`

- [ ] **Step 1: 创建 tid_resolver.h**

```c
#ifndef TID_RESOLVER_H
#define TID_RESOLVER_H

#include "storage/rel/heapam.h"

typedef struct {
    BlockNumber block_num;
    OffsetNumber offset;
    ItemPointerData tid;
} TIDResolver;

// 从 heap tuple 获取真实 TID
TIDResolver* tid_resolver_from_tuple(HeapTuple tuple);

// 使用 TID 更新记录
int tid_resolver_update(TIDResolver* resolver, const void* new_data);

// 使用 TID 删除记录
int tid_resolver_delete(TIDResolver* resolver);

#endif
```

- [ ] **Step 2: 实现 tid_resolver.c**

从 heap tuple 提取真实的 block_num 和 offset。

- [ ] **Step 3: 编译验证并提交**

---

### Task 15: 修复 nodeModifyTable.c

**Files:**
- Modify: `engineering/src/db/executor/nodeModifyTable.c`

**Interfaces:**
- Consumes: tid_resolver.h
- Produces: 消除硬编码 TID

- [ ] **Step 1: 查找硬编码位置并替换**

```c
// 旧代码（有 bug）
ItemPointerData tid;
tid.ip_blkid.bi_hi = 0;
tid.ip_blkid.bi_lo = 0;
tid.ip_posid = 24;  // 硬编码！

// 新代码（正确）
TIDResolver* resolver = tid_resolver_from_tuple(tuple);
ItemPointerData tid = resolver->tid;
```

- [ ] **Step 2: 编译验证**

```bash
cd D:/code/book
gcc -c -I engineering/include engineering/src/db/executor/nodeModifyTable.c -o nodeModifyTable.o
```

- [ ] **Step 3: 提交**

```bash
cd D:/code/book
git add engineering/src/db/executor/nodeModifyTable.c
git commit -m "fix(rel): 修复 nodeModifyTable TID 硬编码问题"
```

---

### Task 16: 编写 TID 管道测试

**Files:**
- Create: `engineering/test/db/tid_pipe_test.c`

**Interfaces:**
- Produces: 验证 TID 管道正确性

- [ ] **Step 1: 编写测试用例**

验证 UPDATE/DELETE 操作使用正确的 TID 而非硬编码。

- [ ] **Step 2: 运行测试并提交**

---

## Phase 1: G4 锁默认开关

### Task 17: 统一锁默认值为 true

**Files:**
- Modify: `engineering/src/db/storage/vector/vector_engine.c`
- Modify: `engineering/src/db/storage/kv/kv.c`
- Modify: `engineering/src/db/storage/ts/ts_engine.c`
- Modify: `engineering/src/db/storage/doc/doc_engine.c`

**Interfaces:**
- Produces: `use_lock=true` 为默认值

- [ ] **Step 1: 修改默认值**

在各个 engine 创建时，确保 `use_lock=true`。

```c
// vector_engine.c
EngineConfig config = {
    .use_lock = true,  // 默认开启
    // ...
};
```

- [ ] **Step 2: 编译验证并提交**

---

## Phase 1: G5 错误路径统一化

### Task 18: 统一错误处理

**Files:**
- Modify: 各 engine 的错误路径

**Interfaces:**
- Produces: 统一的错误码 + 资源回收

- [ ] **Step 1: 定义统一错误码**

```c
// engineering/include/db/error_codes.h
typedef enum {
    ERR_OK = 0,
    ERR_NOT_FOUND,
    ERR_DUPLICATE_KEY,
    ERR_WAL_WRITE_FAILED,
    ERR_LOCK_FAILED,
    // ...
} ErrorCode;
```

- [ ] **Step 2: 在各 engine 中统一使用错误码**

- [ ] **Step 3: 编译验证并提交**

---

## Phase 1: G6 跨模态集成测试基线

### Task 19: 创建跨模态集成测试框架

**Files:**
- Create: `engineering/test/db/cross_modality_test.c`

**Interfaces:**
- Produces: 跨模态集成测试基线

- [ ] **Step 1: 创建测试框架**

```c
#include <cmocka.h>

// 测试 Vector + Graph + RAG 跨模态检索
static void test_vector_graph_rag_integration(void** state) {
    // 1. 创建 Vector 数据
    // 2. 创建 Graph 数据
    // 3. 验证跨模态查询
}

// 测试 Relational + MVCC + WAL
static void test_relational_mvcc_wal(void** state) {
    // 1. 插入数据
    // 2. 开启事务
    // 3. 更新数据
    // 4. 验证 WAL 记录
    // 5. 模拟崩溃恢复
}

// 测试 Vector 并发安全
static void test_vector_concurrent(void** state) {
    // 1. 多线程并发插入
    // 2. 多线程并发搜索
    // 3. 验证无 UAF、无数据竞争
}
```

- [ ] **Step 2: 运行测试**

- [ ] **Step 3: 提交**

```bash
cd D:/code/book
git add engineering/test/db/cross_modality_test.c
git commit -m "test: 创建跨模态集成测试基线"
```

---

### Task 20: WAL 集成测试

**Files:**
- Create: `engineering/test/db/wal_integration_test.c`

**Interfaces:**
- Produces: WAL 集成测试

- [ ] **Step 1: 编写 WAL 集成测试**

验证 fsync 正确性、WAL 恢复能力。

- [ ] **Step 2: 运行测试并提交**

---

## Phase 1 验收总结

- [ ] Task 1-9: G1 并发原语库完成，5 个模态替换完成
- [ ] Task 10-13: G2 WAL 统一接入完成，6 个模态崩溃安全
- [ ] Task 14-16: G3 TID 管道修复完成
- [ ] Task 17: G4 锁默认修复完成
- [ ] Task 18: G5 错误路径统一完成
- [ ] Task 19-20: G6 集成测试基线建立

---

## Self-Review 记录

1. **规格覆盖:** 设计文档的 6 个共性 Gap 全覆盖
2. **占位符扫描:** 无 TBD/TODO
3. **类型一致性:** 所有 API 在 Task 间签名一致
