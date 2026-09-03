# NOTES.md — rcu 工程对照

## RCU 无锁同步在数据库中的应用

### 背景

RCU（Read-Copy-Update）是 Linux 内核广泛使用的无锁同步机制，允许读操作完全不获取锁。数据库中的元数据缓存（Catalog）、查询计划缓存等读多写少场景是 RCU 的典型应用。

### 工程对照：数据库 Catalog 缓存

在我们的 `engineering/src/db/storage/` 模块中，Catalog 缓存是 RCU 的理想应用场景：

```c
// catalog_rcu.c — 基于 RCU 的 Catalog 缓存
typedef struct {
    Oid rel_oid;           // 表 OID
    char relname[64];      // 表名
    TupleDesc *desc;       // 表结构
    int ncolumns;          // 列数
    struct timespec ctime; // 创建时间
} CatalogEntry;

typedef struct {
    CatalogEntry *entries[CATALOG_MAX];  // RCU 保护的指针数组
    int count;
} CatalogCache;

CatalogCache *g_catalog = NULL;  // RCU 保护的全局指针

/* 读侧：不需要锁！ */
CatalogEntry* catalog_lookup(Oid oid) {
    // 即使写者正在更新，读者也不会被阻塞
    CatalogCache *snapshot = __atomic_load_n(&g_catalog, __ATOMIC_ACQUIRE);

    for (int i = 0; i < snapshot->count; i++) {
        if (snapshot->entries[i]->rel_oid == oid) {
            return snapshot->entries[i];  // 直接返回，无需释放
        }
    }
    return NULL;
}

/* 写侧：COW + 等待宽限期 */
void catalog_insert(CatalogEntry *entry) {
    // 步骤1: 拷贝旧缓存
    CatalogCache *old = g_catalog;
    CatalogCache *new = malloc(sizeof(CatalogCache));

    // 步骤2: 拷贝 + 新项
    memcpy(new, old, sizeof(CatalogCache));
    new->entries[new->count++] = entry;  // 新增

    // 步骤3: 原子更新指针（读者从此刻起看到新版本）
    __atomic_store_n(&g_catalog, new, __ATOMIC_RELEASE);

    // 步骤4: 等待宽限期（所有在途读者都已离开）
    synchronize_rcu();  // 内核提供的 RCU API

    // 步骤5: 安全释放旧版本
    free(old);
}
```

### RCU 特性与数据库对比

| RCU 特性 | 数据库场景 |
|----------|-----------|
| 读无锁 | 查询时读取表定义 |
| COW 写 | DDL 操作（CREATE/ALTER TABLE）|
| 宽限期 | 确保所有飞行查询看到一致的元数据 |
| 无 ABA 问题 | 版本号 + 指针组合 |

### 数据库 RCU 适用场景

1. **Catalog 缓存**：表结构、索引定义读多写少
2. **GUC 配置**：运行时参数读取零开销
3. **查询计划缓存**：预编译语句的哈希表查找
4. **锁管理器的快速路径**：lock-free 哈希表
5. **监控指标**：累计统计（计数器数组）

### RCU 与其他同步机制对比

```c
// 性能对比: 100万次读操作
// 互斥锁   : 200ms  (每次加锁解锁)
// 读写锁   : 100ms  (读者获取读锁)
// RCU      : 5ms    (无开销，仅内存屏障)
// 原子变量 : 50ms   (CAS 竞争)

// 写操作成本
// 互斥锁   : 500ns  (一次锁获取)
// 读写锁   : 600ns  (写锁更贵)
// RCU      : 5000ns (COW + 宽限期，但写极少)
```

### 最佳实践

1. **粒度**：在合适的粒度使用（如整个 Catalog，而非单条记录）
2. **写频率**：写频率应远低于读频率（1:1000+）
3. **宽限期**：`synchronize_rcu()` 是阻塞调用，合理使用
4. **内存回收**：注意旧版本的内存释放时机

### 性能影响

- 读侧：零开销（仅 acquire/release 内存屏障）
- 写侧：COW 拷贝开销 + 宽限期等待（毫秒级）
- 总收益：读多写少场景可 10-50x 吞吐提升

### 扩展思考

- Linux 内核 RCU 实现：`rcu_read_lock/rcu_read_unlock`
- 用户空间 RCU（URCU）：`cds_list_for_each_entry_rcu`
- PostgreSQL 使用 RCU 风格的 LWLock 优化
- 现代数据库引擎（如 WiredTiger/MongoDB）的 RCU 使用
