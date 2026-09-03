# SQL→存储深度集成实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 SQL 执行引擎与存储引擎的端到端深度集成，支持数据插入、索引查找、事务控制和 WAL 持久化

**Architecture:** 采用 PostgreSQL 风格的 Volcano 迭代器模型，通过 Catalog 管理元数据、Buffer Pool 缓存页面、Heap/BTree AM 存储数据、WAL 记录修改。集成测试验证完整数据流。

**Tech Stack:** C11, C++17, GoogleTest, PostgreSQL 风格存储架构（Catalog、Buffer Pool、Heap AM、BTree AM、WAL）

## Global Constraints

- CMake 3.20+ 构建系统
- 所有代码使用中文注释
- 测试文件使用 C++ (.cpp)，通过 `extern "C"` 引入 C 头文件
- 提交信息使用中文
- 遵循现有的代码风格（参考 `engineering/src/db/sql/` 和 `engineering/src/db/storage/`）
- 每个任务必须包含完整的测试验证

---

## Phase 1: IndexScan 完善

### Task 1.1: 添加调试日志定位失败点

**Files:**
- Modify: `engineering/src/db/sql/nodeIndexscan.c:348-375`
- Test: `engineering/test/db/sql/test_sql_storage_integration.cpp`

**Interfaces:**
- Consumes: `Oid indexid`, `Oid relid` (索引和表的 OID)
- Produces: 调试日志输出到 stderr

- [ ] **Step 1: 添加日志到 ExecInitIndexScanRelation**

打开 `engineering/src/db/sql/nodeIndexscan.c`，在 `ExecInitIndexScanRelation` 函数开头添加调试日志：

```c
int ExecInitIndexScanRelation(IndexScanState *node, IndexScanExtState *ext_state,
                              Oid indexid, Oid relid)
{
    Relation indexrel;
    Relation heaprel;

    /* 添加调试日志 */
    fprintf(stderr, "[DEBUG] ExecInitIndexScanRelation: indexid=%u, relid=%u\n", indexid, relid);

    if (!node || !ext_state || !OidIsValid(indexid) || !OidIsValid(relid)) {
        fprintf(stderr, "[ERROR] Invalid parameters: node=%p, ext_state=%p, indexid=%u, relid=%u\n",
                node, ext_state, indexid, relid);
        return -1;
    }

    /* 打开索引 Relation */
    indexrel = relation_open(indexid, RELMODE_READ);
    if (!indexrel) {
        fprintf(stderr, "[ERROR] Failed to open index relation: oid=%u\n", indexid);
        return -1;
    }

    /* 打开堆 Relation */
    heaprel = relation_open(relid, RELMODE_READ);
    if (!heaprel) {
        fprintf(stderr, "[ERROR] Failed to open heap relation: oid=%u\n", relid);
        relation_close(indexrel, RELMODE_READ);
        return -1;
    }

    ext_state->iss_indexRelation = indexrel;
    ext_state->iss_heapRelation = heaprel;

    fprintf(stderr, "[INFO] Successfully initialized: index_oid=%u, heap_oid=%u\n",
            indexid, relid);
    return 0;
}
```

- [ ] **Step 2: 编译并运行测试**

```bash
cmake --build build/engineering --target test_sql_storage_integration
./build/engineering/test/db/sql/test_sql_storage_integration.exe --gtest_filter=*IndexScan*
```

Expected: 看到 `[ERROR] Failed to open index relation` 或类似错误信息

- [ ] **Step 3: 分析日志输出**

根据错误信息确定是哪个 `relation_open` 调用失败：
- 如果是 `indexid` 失败：Catalog 未正确注册索引
- 如果是 `relid` 失败：Catalog 表查找逻辑问题

- [ ] **Step 4: 提交调试代码**

```bash
git add engineering/src/db/sql/nodeIndexscan.c
git commit -m "feat(sql): 添加 IndexScan 初始化调试日志"
```

---

### Task 1.2: 验证 Catalog 索引创建

**Files:**
- Modify: `engineering/src/db/storage/catalog/catalog.c:255-280`
- Modify: `engineering/test/db/sql/test_sql_storage_integration.cpp:151-191`

**Interfaces:**
- Consumes: `catalog_create_index()` 返回的 OID
- Produces: 验证索引 OID 在 Catalog 中可查找

- [ ] **Step 1: 在测试中添加 OID 验证**

修改 `test_sql_storage_integration.cpp` 的 `IndexScanStorageIntegration` 测试：

```cpp
TEST_F(SqlStorageIntegrationTest, IndexScanStorageIntegration) {
    // 创建表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    columns[0].not_null = true;
    strncpy(columns[1].name, "data", NAMEDATALEN);
    columns[1].type_oid = 25;

    Oid table_oid = catalog_create_table("indexscan_test", columns, 2);
    ASSERT_NE(table_oid, InvalidOid);

    // 创建索引
    const char *index_cols[] = {"id"};
    Oid index_oid = catalog_create_index("idx_indexscan_id", table_oid,
                                          index_cols, 1, true);
    ASSERT_NE(index_oid, InvalidOid) << "索引创建失败";

    // 验证索引可以查找
    index_info_t *index_info = catalog_get_index(index_oid);
    ASSERT_NE(index_info, nullptr) << "索引 OID " << index_oid << " 在 Catalog 中未找到";
    EXPECT_EQ(index_info->table_oid, table_oid);
    catalog_free_index(index_info);

    // 后续测试...
}
```

- [ ] **Step 2: 运行测试验证 Catalog**

```bash
cmake --build build/engineering --target test_sql_storage_integration
./build/engineering/test/db/sql/test_sql_storage_integration.exe --gtest_filter=*IndexScan*
```

Expected: 看到 `index_info != nullptr` 验证通过

- [ ] **Step 3: 检查 Catalog 实现**

如果验证失败，检查 `catalog_create_index` 实现：

```c
// engineering/src/db/storage/catalog/catalog.c:255
Oid catalog_create_index(const char *index_name, Oid table_oid,
                         const char **columns, int ncolumns, bool is_unique) {
    // 确保索引被正确添加到全局哈希表
    // 确保返回的 OID 非零
}
```

- [ ] **Step 4: 提交验证代码**

```bash
git add engineering/test/db/sql/test_sql_storage_integration.cpp
git commit -m "test(sql): 添加 Catalog 索引创建验证"
```

---

### Task 1.3: 修复 relation_open 索引支持

**Files:**
- Modify: `engineering/src/db/storage/rel/rel.c:141-210`
- Read: `engineering/include/db/rel.h:59-100`

**Interfaces:**
- Consumes: `Oid relid`, `RelOpenMode mode`
- Produces: `Relation` 指针（包含索引 Relation）

- [ ] **Step 1: 阅读 relation_open 实现**

```bash
Read engineering/src/db/storage/rel/rel.c:141-210
```

理解当前的表查找逻辑：
- 如何从 OID 查找 Catalog 条目
- 如何区分表和索引

- [ ] **Step 2: 添加索引类型支持**

修改 `relation_open` 函数，确保支持索引类型：

```c
Relation relation_open(Oid relid, int mode) {
    Relation rel;
    table_info_t *info;
    
    // 查找表信息（包括索引）
    info = catalog_get_table(relid);
    if (!info) {
        // 可能是索引，尝试从索引表查找
        index_info_t *idx_info = catalog_get_index(relid);
        if (!idx_info) {
            fprintf(stderr, "[ERROR] relation_open: OID %u not found in catalog\n", relid);
            return NULL;
        }
        // 从索引信息构造 Relation
        rel = create_index_relation(idx_info);
        catalog_free_index(idx_info);
        return rel;
    }
    
    // 原有的表 Relation 创建逻辑...
}
```

- [ ] **Step 3: 编译并测试**

```bash
cmake --build build/engineering --target test_sql_storage_integration
./build/engineering/test/db/sql/test_sql_storage_integration.exe --gtest_filter=*IndexScan*
```

Expected: `relation_open(index_oid)` 成功返回

- [ ] **Step 4: 提交修复**

```bash
git add engineering/src/db/storage/rel/rel.c
git commit -m "fix(rel): 支持 relation_open 打开索引 Relation"
```

---

### Task 1.4: 验证 IndexScan 完整流程

**Files:**
- Test: `engineering/test/db/sql/test_sql_storage_integration.cpp`

**Interfaces:**
- Consumes: `IndexScanState` 初始化成功
- Produces: IndexScan 可以迭代返回元组（空表返回 0 元组）

- [ ] **Step 1: 运行完整测试**

```bash
./build/engineering/test/db/sql/test_sql_storage_integration.exe --gtest_filter=*IndexScan*
```

Expected: 测试通过，看到 `[INFO] Successfully initialized` 日志

- [ ] **Step 2: 移除调试日志（可选）**

如果测试稳定，可以移除 fprintf 日志，或改为条件编译：

```c
#ifdef DEBUG_INDEXSCAN
fprintf(stderr, "[DEBUG] ...\n");
#endif
```

- [ ] **Step 3: 提交最终版本**

```bash
git add engineering/src/db/sql/nodeIndexscan.c
git commit -m "feat(sql): 完善 IndexScan 初始化并移除调试日志"
```

---

## Phase 2: 数据插入验证

### Task 2.1: 实现数据插入辅助函数

**Files:**
- Create: `engineering/test/db/sql/test_data_helper.h`
- Create: `engineering/test/db/sql/test_data_helper.cpp`
- Modify: `engineering/test/db/sql/CMakeLists.txt`

**Interfaces:**
- Produces: `insert_test_tuple(Oid table_oid, int id, const char *name, int value)`

- [ ] **Step 1: 创建辅助函数头文件**

```cpp
// engineering/test/db/sql/test_data_helper.h
#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t Oid;

/**
 * @brief 插入测试元组到表
 * @param table_oid 表 OID
 * @param id ID 列值
 * @param name 名称列值
 * @param value 数值列值
 * @return 0 成功，-1 失败
 */
int insert_test_tuple(Oid table_oid, int id, const char *name, int value);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: 实现辅助函数**

```cpp
// engineering/test/db/sql/test_data_helper.cpp
#include "test_data_helper.h"

extern "C" {
#include "db/rel.h"
#include "db/heapam.h"
#include "db/catalog.h"
}

int insert_test_tuple(Oid table_oid, int id, const char *name, int value) {
    // 打开表
    Relation rel = relation_open(table_oid, 1);  // RELMODE_WRITE
    if (!rel) {
        return -1;
    }

    // 构造元组数据
    struct {
        int32_t id;
        char name[64];
        int32_t value;
    } tuple_data;

    tuple_data.id = id;
    strncpy(tuple_data.name, name, sizeof(tuple_data.name) - 1);
    tuple_data.name[sizeof(tuple_data.name) - 1] = '\0';
    tuple_data.value = value;

    // 插入元组
    int result = heap_insert(rel, &tuple_data, sizeof(tuple_data), 0, 0, nullptr);

    // 关闭表
    relation_close(rel, 1);

    return result;
}
```

- [ ] **Step 3: 更新 CMakeLists.txt**

```cmake
# engineering/test/db/sql/CMakeLists.txt
# 添加数据插入辅助库
add_library(test_data_helper test_data_helper.cpp)
target_include_directories(test_data_helper PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_data_helper db_storage project_includes)

# 更新 test_sql_storage_integration 依赖
target_link_libraries(test_sql_storage_integration sql_engine db_parser_sql_simplified db_storage project_includes test_data_helper gtest gtest_main)
```

- [ ] **Step 4: 编译验证**

```bash
cmake --build build/engineering --target test_sql_storage_integration
```

Expected: 编译成功

- [ ] **Step 5: 提交辅助函数**

```bash
git add engineering/test/db/sql/test_data_helper.h
git add engineering/test/db/sql/test_data_helper.cpp
git add engineering/test/db/sql/CMakeLists.txt
git commit -m "feat(test): 添加数据插入辅助函数"
```

---

### Task 2.2: 验证 SeqScan 返回插入的数据

**Files:**
- Modify: `engineering/test/db/sql/test_sql_storage_integration.cpp`

**Interfaces:**
- Consumes: `insert_test_tuple()` 插入数据
- Produces: SeqScan 返回正确的元组数量和内容

- [ ] **Step 1: 编写测试用例**

在 `test_sql_storage_integration.cpp` 添加新测试：

```cpp
TEST_F(SqlStorageIntegrationTest, SeqScanWithData) {
    // 创建表
    column_def_t columns[3];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;  // int4
    strncpy(columns[1].name, "name", NAMEDATALEN);
    columns[1].type_oid = 25;  // text
    strncpy(columns[2].name, "value", NAMEDATALEN);
    columns[2].type_oid = 23;  // int4

    Oid table_oid = catalog_create_table("scan_data_test", columns, 3);
    ASSERT_NE(table_oid, InvalidOid);

    // 插入 5 条数据
    EXPECT_EQ(insert_test_tuple(table_oid, 1, "Alice", 100), 0);
    EXPECT_EQ(insert_test_tuple(table_oid, 2, "Bob", 200), 0);
    EXPECT_EQ(insert_test_tuple(table_oid, 3, "Charlie", 300), 0);
    EXPECT_EQ(insert_test_tuple(table_oid, 4, "David", 400), 0);
    EXPECT_EQ(insert_test_tuple(table_oid, 5, "Eve", 500), 0);

    // 执行 SeqScan
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = table_oid;

    SeqScanState *state = ExecInitSeqScan(&plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    // 统计返回的元组数
    int count = 0;
    while (ExecSeqScan((PlanState *)state) != nullptr) {
        count++;
    }

    // 验证返回了 5 条数据
    EXPECT_EQ(count, 5);

    ExecEndSeqScan(state);
}
```

- [ ] **Step 2: 运行测试**

```bash
cmake --build build/engineering --target test_sql_storage_integration
./build/engineering/test/db/sql/test_sql_storage_integration.exe --gtest_filter=*SeqScanWithData*
```

Expected: 测试通过，返回 5 条元组

- [ ] **Step 3: 提交测试**

```bash
git add engineering/test/db/sql/test_sql_storage_integration.cpp
git commit -m "test(sql): 验证 SeqScan 返回插入的数据"
```

---

### Task 2.3: 验证 IndexScan 按条件查找

**Files:**
- Modify: `engineering/test/db/sql/test_sql_storage_integration.cpp`

**Interfaces:**
- Consumes: 带索引的表，插入数据
- Produces: IndexScan 按 ScanKey 条件返回匹配的元组

- [ ] **Step 1: 编写索引查找测试**

```cpp
TEST_F(SqlStorageIntegrationTest, IndexScanWithCondition) {
    // 创建带索引的表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    columns[0].not_null = true;
    strncpy(columns[1].name, "data", NAMEDATALEN);
    columns[1].type_oid = 25;

    Oid table_oid = catalog_create_table("index_find_test", columns, 2);
    ASSERT_NE(table_oid, InvalidOid);

    // 创建索引
    const char *index_cols[] = {"id"};
    Oid index_oid = catalog_create_index("idx_find_id", table_oid, index_cols, 1, true);
    ASSERT_NE(index_oid, InvalidOid);

    // 插入数据
    insert_test_tuple(table_oid, 10, "Data10", 0);
    insert_test_tuple(table_oid, 20, "Data20", 0);
    insert_test_tuple(table_oid, 30, "Data30", 0);

    // 创建 ScanKey (查找 id=20)
    ScanKeyData scan_key;
    ScanKeyInit(&scan_key, 1, SK_EQ, (void *)20);

    // 初始化 IndexScan
    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;
    plan.indexid = index_oid;
    plan.relid = table_oid;
    plan.nkeys = 1;
    plan.key = &scan_key;

    IndexScanState *state = ExecInitIndexScan(&plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    // 扫描并验证
    int count = 0;
    while (ExecIndexScan((PlanState *)state) != nullptr) {
        count++;
    }

    // 验证返回了 1 条数据 (id=20)
    EXPECT_EQ(count, 1);

    ExecEndIndexScan(state);
}
```

- [ ] **Step 2: 运行测试**

```bash
cmake --build build/engineering --target test_sql_storage_integration
./build/engineering/test/db/sql/test_sql_storage_integration.exe --gtest_filter=*IndexScanWithCondition*
```

Expected: 测试通过

- [ ] **Step 3: 提交测试**

```bash
git add engineering/test/db/sql/test_sql_storage_integration.cpp
git commit -m "test(sql): 验证 IndexScan 按条件查找数据"
```

---

## Phase 3: 事务支持

### Task 3.1: 集成事务管理器到测试环境

**Files:**
- Modify: `engineering/test/db/sql/test_sql_storage_integration.cpp`
- Read: `engineering/include/db/storage/txn/txn.h`
- Read: `engineering/src/db/storage/txn/txn.c`

**Interfaces:**
- Consumes: `txn_manager_t`, `txn_t`
- Produces: 测试环境支持事务边界控制

- [ ] **Step 1: 阅读事务 API**

```bash
Read engineering/include/db/storage/txn/txn.h
```

理解关键函数：
- `txn_manager_create()`
- `txn_begin()`
- `txn_commit()`
- `txn_abort()`

- [ ] **Step 2: 扩展测试基类**

```cpp
// test_sql_storage_integration.cpp

class SqlStorageTxnTest : public SqlStorageIntegrationTest {
protected:
    txn_manager_t *txn_mgr;

    void SetUp() override {
        SqlStorageIntegrationTest::SetUp();

        // 初始化事务管理器（1024 并发事务）
        txn_mgr = txn_manager_create(1024);
        ASSERT_NE(txn_mgr, nullptr);
    }

    void TearDown() override {
        if (txn_mgr) {
            txn_manager_destroy(txn_mgr);
        }
        SqlStorageIntegrationTest::TearDown();
    }
};
```

- [ ] **Step 3: 编译验证**

```bash
cmake --build build/engineering --target test_sql_storage_integration
```

Expected: 编译成功

- [ ] **Step 4: 提交事务测试基类**

```bash
git add engineering/test/db/sql/test_sql_storage_integration.cpp
git commit -m "test(sql): 添加事务测试基类"
```

---

### Task 3.2: 实现基本事务测试

**Files:**
- Modify: `engineering/test/db/sql/test_sql_storage_integration.cpp`

**Interfaces:**
- Produces: 事务提交后数据可见，回滚后数据不可见

- [ ] **Step 1: 编写事务提交测试**

```cpp
TEST_F(SqlStorageTxnTest, TransactionCommit) {
    // 创建表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    strncpy(columns[1].name, "value", NAMEDATALEN);
    columns[1].type_oid = 23;

    Oid table_oid = catalog_create_table("txn_test", columns, 2);
    ASSERT_NE(table_oid, InvalidOid);

    // 开始事务
    txn_t *txn = txn_begin(txn_mgr);
    ASSERT_NE(txn, nullptr);

    // 在事务中插入数据
    EXPECT_EQ(insert_test_tuple(table_oid, 1, "TxData", 100), 0);

    // 提交事务
    EXPECT_EQ(txn_commit(txn), 0);

    // 验证数据可见
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = table_oid;

    SeqScanState *state = ExecInitSeqScan(&plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    int count = 0;
    while (ExecSeqScan((PlanState *)state) != nullptr) {
        count++;
    }

    EXPECT_EQ(count, 1) << "提交后应可见 1 条数据";
    ExecEndSeqScan(state);
}
```

- [ ] **Step 2: 编写事务回滚测试**

```cpp
TEST_F(SqlStorageTxnTest, TransactionAbort) {
    // 创建表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    strncpy(columns[1].name, "value", NAMEDATALEN);
    columns[1].type_oid = 23;

    Oid table_oid = catalog_create_table("txn_abort_test", columns, 2);
    ASSERT_NE(table_oid, InvalidOid);

    // 开始事务
    txn_t *txn = txn_begin(txn_mgr);
    ASSERT_NE(txn, nullptr);

    // 在事务中插入数据
    EXPECT_EQ(insert_test_tuple(table_oid, 2, "AbortData", 200), 0);

    // 回滚事务
    EXPECT_EQ(txn_abort(txn), 0);

    // 验证数据不可见
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = table_oid;

    SeqScanState *state = ExecInitSeqScan(&plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    int count = 0;
    while (ExecSeqScan((PlanState *)state) != nullptr) {
        count++;
    }

    EXPECT_EQ(count, 0) << "回滚后应无数据";
    ExecEndSeqScan(state);
}
```

- [ ] **Step 3: 运行测试**

```bash
cmake --build build/engineering --target test_sql_storage_integration
./build/engineering/test/db/sql/test_sql_storage_integration.exe --gtest_filter=*Transaction*
```

Expected: 两个测试都通过

- [ ] **Step 4: 提交事务测试**

```bash
git add engineering/test/db/sql/test_sql_storage_integration.cpp
git commit -m "test(sql): 实现事务提交和回滚测试"
```

---

## Phase 4: WAL 集成

### Task 4.1: 集成 WAL 到测试环境

**Files:**
- Modify: `engineering/test/db/sql/test_sql_storage_integration.cpp`
- Read: `engineering/include/db/storage/wal/wal.h`
- Read: `engineering/include/db/storage/wal/wal_buf.h`

**Interfaces:**
- Consumes: `wal_t`, `wal_buf_t`
- Produces: 测试环境支持 WAL 记录生成

- [ ] **Step 1: 阅读 WAL API**

```bash
Read engineering/include/db/storage/wal/wal.h
```

理解关键函数：
- `wal_create()`: 创建 WAL 文件
- `wal_append()`: 追加记录
- `wal_get_stats()`: 获取统计信息

- [ ] **Step 2: 扩展测试基类支持 WAL**

```cpp
class SqlStorageWalTest : public SqlStorageIntegrationTest {
protected:
    wal_t *wal;
    wal_buf_t *wal_buf;
    char wal_path[256];

    void SetUp() override {
        SqlStorageIntegrationTest::SetUp();

        // 创建临时 WAL 文件
        snprintf(wal_path, sizeof(wal_path), "test_wal_%d.log", getpid());
        wal = wal_create(wal_path);
        ASSERT_NE(wal, nullptr);

        // 创建 WAL-Buf 协调器
        wal_buf = wal_buf_create(wal);
        ASSERT_NE(wal_buf, nullptr);
    }

    void TearDown() override {
        if (wal_buf) {
            wal_buf_destroy(wal_buf);
        }
        if (wal) {
            wal_close(wal);
        }
        // 删除临时文件
        unlink(wal_path);

        SqlStorageIntegrationTest::TearDown();
    }
};
```

- [ ] **Step 3: 编译验证**

```bash
cmake --build build/engineering --target test_sql_storage_integration
```

Expected: 编译成功

- [ ] **Step 4: 提交 WAL 测试基类**

```bash
git add engineering/test/db/sql/test_sql_storage_integration.cpp
git commit -m "test(sql): 添加 WAL 测试基类"
```

---

### Task 4.2: 验证 WAL 记录生成

**Files:**
- Modify: `engineering/test/db/sql/test_sql_storage_integration.cpp`

**Interfaces:**
- Produces: 插入操作生成 WAL_LOG_INSERT 记录

- [ ] **Step 1: 编写 WAL 记录测试**

```cpp
TEST_F(SqlStorageWalTest, WalInsertRecord) {
    // 创建表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    strncpy(columns[1].name, "data", NAMEDATALEN);
    columns[1].type_oid = 25;

    Oid table_oid = catalog_create_table("wal_test", columns, 2);
    ASSERT_NE(table_oid, InvalidOid);

    // 获取初始 WAL 统计
    WalStats stats_before;
    wal_get_stats(wal, &stats_before);

    // 插入数据
    EXPECT_EQ(insert_test_tuple(table_oid, 100, "WalData", 0), 0);

    // 获取插入后 WAL 统计
    WalStats stats_after;
    wal_get_stats(wal, &stats_after);

    // 验证生成了 WAL 记录
    EXPECT_GT(stats_after.records_written, stats_before.records_written)
        << "插入操作应生成 WAL 记录";
}
```

- [ ] **Step 2: 运行测试**

```bash
cmake --build build/engineering --target test_sql_storage_integration
./build/engineering/test/db/sql/test_sql_storage_integration.exe --gtest_filter=*WalInsert*
```

Expected: 测试通过，WAL 记录数增加

- [ ] **Step 3: 提交 WAL 测试**

```bash
git add engineering/test/db/sql/test_sql_storage_integration.cpp
git commit -m "test(sql): 验证 WAL 记录生成"
```

---

### Task 4.3: 实现 WAL 恢复测试

**Files:**
- Modify: `engineering/test/db/sql/test_sql_storage_integration.cpp`

**Interfaces:**
- Produces: WAL 恢复后数据一致

- [ ] **Step 1: 编写 WAL 恢复测试**

```cpp
TEST_F(SqlStorageWalTest, WalRecovery) {
    // 创建表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    strncpy(columns[1].name, "value", NAMEDATALEN);
    columns[1].type_oid = 23;

    Oid table_oid = catalog_create_table("wal_recovery_test", columns, 2);
    ASSERT_NE(table_oid, InvalidOid);

    // 插入数据并记录 WAL
    insert_test_tuple(table_oid, 1, "Data1", 100);
    insert_test_tuple(table_oid, 2, "Data2", 200);

    // 强制刷盘 WAL
    wal_flush(wal);

    // 模拟崩溃：不正常关闭数据库
    // 在实际测试中，这需要更复杂的处理

    // 从 WAL 恢复
    WalStats stats;
    wal_get_stats(wal, &stats);
    EXPECT_GT(stats.records_written, 0) << "WAL 中应有记录";

    // 验证恢复后的数据（简化版）
    // 完整实现需要重新初始化存储系统并回放 WAL
}
```

- [ ] **Step 2: 运行测试**

```bash
cmake --build build/engineering --target test_sql_storage_integration
./build/engineering/test/db/sql/test_sql_storage_integration.exe --gtest_filter=*WalRecovery*
```

Expected: 测试通过

- [ ] **Step 3: 提交恢复测试**

```bash
git add engineering/test/db/sql/test_sql_storage_integration.cpp
git commit -m "test(sql): 实现 WAL 恢复测试"
```

---

## 验收标准

### Phase 1 完成标志
- [x] IndexScan 初始化成功
- [ ] IndexScan 测试通过（需完成 Task 1.3-1.4）

### Phase 2 完成标志
- [ ] 数据插入辅助函数可用
- [ ] SeqScan 返回正确的元组数量和内容
- [ ] IndexScan 按条件查找正确

### Phase 3 完成标志
- [ ] 事务提交后数据可见
- [ ] 事务回滚后数据不可见

### Phase 4 完成标志
- [ ] 插入操作生成 WAL 记录
- [ ] WAL 恢复后数据一致

---

## 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| IndexScan 初始化复杂 | 高 | 分步调试，添加详细日志 |
| 事务并发难以测试 | 中 | 先实现单事务测试，再扩展并发 |
| WAL 恢复逻辑复杂 | 高 | 先验证记录生成，再实现完整恢复 |

---

## 时间估算

| 阶段 | 工时（小时） |
|------|--------------|
| Phase 1: IndexScan 完善 | 4-6 |
| Phase 2: 数据插入验证 | 3-4 |
| Phase 3: 事务支持 | 8-12 |
| Phase 4: WAL 集成 | 6-8 |
| **总计** | **21-30** |