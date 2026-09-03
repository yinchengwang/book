# MemoryContext 最佳实践指南

> 本指南为多模态数据库 MemoryContext 内存管理系统提供使用规范、调试技巧和性能调优建议。

---

## 1. 概述

MemoryContext 是 PostgreSQL 风格的内存生命周期管理系统，核心特性：

- **层级结构**：parent → child 上下文形成树形结构
- **批量管理**：按块（Block）分配，reset/delete 统一回收
- **零碎片**：块内线性分配，8 字节对齐
- **资源析构**：支持注册析构回调，LIFO 顺序执行

```
DatabaseContext (根)
├── ConnectionContext
│   ├── RequestContext
│   │   └── QueryContext
│   └── PreparedStatementContext
└── CollectionContext
    └── IndexBuildContext
```

---

## 2. 上下文选择指南

| 场景 | 推荐上下文 | 预设配置 | 说明 |
|------|-----------|---------|------|
| 数据库全局配置 | DatabaseContext | `ALLOCSET_PRESET_DEFAULT` | 系统表缓存、GUC 参数 |
| 连接级状态 | ConnectionContext | `ALLOCSET_PRESET_DEFAULT` | prepared statement、会话变量 |
| 请求级临时分配 | RequestContext | `ALLOCSET_PRESET_SMALL高频` | SQL 执行器、临时缓冲区 |
| 单次查询执行 | QueryContext | `ALLOCSET_PRESET_SMALL高频` | 查询计划、表达式计算 |
| 集合元数据 | CollectionContext | `ALLOCSET_PRESET_DEFAULT` | 索引构建、schema 信息 |
| 大批量导入 | BulkImportContext | `ALLOCSET_PRESET_BULK` | bulk insert、数据导入 |

---

## 3. 分配模式

### 3.1 批量分配 vs 单次分配

```c
// ✅ 推荐：批量分配后统一释放
MemoryContext batch_ctx = AllocSetContextCreate(
    parent, "BatchImport", 0,
    ALLOCSET_PRESET3_INIT, ALLOCSET_PRESET3_MAX,
    ALLOCSET_PRESET_BULK);

for (int i = 0; i < 1000000; i++) {
    void *tuple = palloc(batch_ctx, tuple_size);
    // ... 处理 tuple
}
// 一次性释放所有内存
MemoryContextDelete(batch_ctx);

// ❌ 避免：频繁创建/销毁上下文
for (int i = 0; i < 1000000; i++) {
    MemoryContext temp = AllocSetContextCreate(...);
    void *tuple = palloc(temp, tuple_size);
    // ...
    MemoryContextDelete(temp);  // 开销大
}
```

### 3.2 Reset 时机选择

```c
// ✅ 查询执行器：每行 reset
TupleTableSlot *slot;
while ((slot = ExecProcNode(plan_state)) != NULL) {
    // 处理当前行
    process_tuple(slot);
    
    // reset 清理临时分配，保留上下文结构
    MemoryContextReset(query_ctx);
}

// ✅ 事务级：commit/rollback 时 reset
void transaction_commit(MemoryContext txn_ctx) {
    // 提交前处理
    flush_wal(txn_ctx);
    
    // reset 释放所有临时分配
    MemoryContextReset(txn_ctx);
}
```

### 3.3 资源析构回调

```c
// 注册文件句柄析构
FILE *fp = fopen("data.csv", "r");
mmdb_mem_register_resource(query_ctx, fp, 
    (void (*)(void*, void*))fclose, NULL, "csv_file");

// 注册锁释放
pthread_mutex_t *lock = palloc(query_ctx, sizeof(pthread_mutex_t));
pthread_mutex_init(lock, NULL);
mmdb_mem_register_resource(query_ctx, lock,
    (void (*)(void*, void*))pthread_mutex_destroy, NULL, "mutex");

// reset/delete 时自动按 LIFO 顺序调用析构函数
MemoryContextReset(query_ctx);  // 自动 fclose(fp) + pthread_mutex_destroy(lock)
```

---

## 4. 调试技巧

### 4.1 启用严格模式

在 Debug 构建中自动启用 `MMDB_MEMCTX_STRICT_FREE`，检测：

- **双重释放**：`pfree()` 同一指针两次
- **跨上下文释放**：用 A 上下文释放 B 上下文的内存
- **魔数校验失败**：内存被覆写或已释放

```c
// 编译时启用（CMakeLists.txt）
target_compile_definitions(sql_engine PRIVATE
    $<$<CONFIG:Debug>:MMDB_MEMCTX_STRICT_FREE=1>)

// 运行时输出示例
[STRICT_FREE] pfree: 双重释放 detected at 0x0000022e9f437fe8
[STRICT_FREE] pfree: 跨上下文释放 detected at 0x0000022e9f437fe8
```

### 4.2 统计信息查看

```c
// 获取上下文统计
MemoryContextStats stats;
stats = ctx->stats;  // 或通过 API 获取

printf("当前分配: %zu bytes\n", stats.current_bytes);
printf("峰值分配: %zu bytes\n", stats.peak_bytes);
printf("分配次数: %zu\n", stats.allocation_count);
printf("重置次数: %zu\n", stats.reset_count);
printf("无效释放: %zu\n", stats.invalid_free_count);
printf("双重释放: %zu\n", stats.double_free_count);
```

### 4.3 Generation 追踪

```c
// 检测 use-after-reset
uint64_t gen_before = MemoryContextGetGeneration(ctx);
MemoryContextReset(ctx);
uint64_t gen_after = MemoryContextGetGeneration(ctx);

if (gen_before == gen_after) {
    // Generation 未递增，说明 reset 未执行
    fprintf(stderr, "警告: reset 未生效\n");
}
```

### 4.4 线程归属校验

```c
// 设置线程归属
MemoryContextSetThreadOwner(ctx, mmdb_current_thread_id());

// 校验当前线程
if (!MemoryContextCheckThread(ctx)) {
    fprintf(stderr, "错误: 跨线程访问检测\n");
    abort();
}
```

---

## 5. 性能调优

### 5.1 AllocSet 预设配置选择

| 场景 | 预设 | initBlockSize | maxBlockSize | 适用模块 |
|------|------|--------------|-------------|----------|
| 通用 | `DEFAULT` | 8KB | 8KB | 系统表、元数据 |
| 小对象高频 | `SMALL高频` | 1KB | 64KB | SQL executor、request scope |
| 大对象 | `LARGE` | 64KB | 1MB | 集合创建、索引构建 |
| 批量导入 | `BULK` | 1MB | 16MB | bulk insert、数据导入 |

### 5.2 块大小调整指南

```c
// 场景 1：高频小对象（SQL 执行器）
MemoryContext exec_ctx = AllocSetContextCreate(
    parent, "Executor", 0,
    ALLOCSET_PRESET1_INIT,   // 1KB 初始
    ALLOCSET_PRESET1_MAX,    // 64KB 最大
    ALLOCSET_PRESET_SMALL高频);

// 场景 2：大对象（索引构建）
MemoryContext idx_ctx = AllocSetContextCreate(
    parent, "IndexBuild", 0,
    ALLOCSET_PRESET2_INIT,   // 64KB 初始
    ALLOCSET_PRESET2_MAX,    // 1MB 最大
    ALLOCSET_PRESET_LARGE);

// 场景 3：批量导入
MemoryContext bulk_ctx = AllocSetContextCreate(
    parent, "BulkImport", 0,
    ALLOCSET_PRESET3_INIT,   // 1MB 初始
    ALLOCSET_PRESET3_MAX,    // 16MB 最大
    ALLOCSET_PRESET_BULK);
```

### 5.3 高频分配路径优化

```c
// ✅ 预分配缓冲区
#define BATCH_SIZE 1024
void **batch = palloc(exec_ctx, sizeof(void*) * BATCH_SIZE);
for (int i = 0; i < n; i++) {
    if (i % BATCH_SIZE == 0 && i > 0) {
        // 批量处理后 reset
        process_batch(batch, BATCH_SIZE);
        MemoryContextReset(exec_ctx);
        batch = palloc(exec_ctx, sizeof(void*) * BATCH_SIZE);
    }
    batch[i % BATCH_SIZE] = palloc(exec_ctx, item_size);
}

// ✅ 使用 palloc0 避免手动清零
void *buf = palloc0(ctx, size);  // 自动清零，比 malloc+memset 快
```

---

## 6. 常见陷阱

### 6.1 Use-after-close 检测

```c
// ❌ 错误：关闭后仍访问
mmdb_close(db);
printf("context deleted: %d\n", db->memory_context->is_deleted);  // 未定义行为

// ✅ 正确：使用哨兵机制
uint64_t gen_before = mmdb_close(db);
// 或检查全局哨兵
if (g_memctx_delete_generation > gen_before) {
    printf("context deleted\n");
}
```

### 6.2 跨上下文释放

```c
// ❌ 错误：跨上下文释放
MemoryContext ctx1 = AllocSetContextCreate(...);
MemoryContext ctx2 = AllocSetContextCreate(...);
void *ptr = palloc(ctx1, 128);
pfree(ctx2, ptr);  // 检测到跨上下文释放

// ✅ 正确：在同一上下文释放
pfree(ctx1, ptr);
```

### 6.3 线程安全

```c
// ❌ 错误：多线程共享上下文
void thread_func(MemoryContext shared_ctx) {
    void *ptr = palloc(shared_ctx, 128);  // 竞态条件
}

// ✅ 正确：每线程独立上下文
void thread_func(MemoryContext parent) {
    MemoryContext thread_ctx = AllocSetContextCreate(
        parent, "ThreadCtx", 0, 0, 0, ALLOCSET_PRESET_DEFAULT);
    void *ptr = palloc(thread_ctx, 128);
    MemoryContextDelete(thread_ctx);
}
```

---

## 7. 迁移指南

### 7.1 手动 malloc/free → MemoryContext

```c
// 旧代码
void *buf = malloc(1024);
// ... 使用 buf
free(buf);

// 新代码
void *buf = palloc(ctx, 1024);
// ... 使用 buf
// 无需手动 free，reset/delete 自动回收
```

### 7.2 第三方库适配层

```c
// 封装第三方库的 malloc/free
void *custom_alloc(size_t size) {
    return palloc(CurrentMemoryContext, size);
}

void custom_free(void *ptr) {
    // AllocSet 中 pfree 为空操作，实际由 reset/delete 回收
    // 如需立即释放，注册资源析构回调
    mmdb_mem_register_resource(CurrentMemoryContext, ptr, 
        free_wrapper, NULL, "third_party_buf");
}
```

---

## 8. API 参考

### 8.1 SDK 兼容层 API

| API | 说明 |
|-----|------|
| `mmdb_mem_init()` | 初始化内存系统 |
| `mmdb_mem_cleanup()` | 清理内存系统 |
| `mmdb_mem_open(path)` | 打开内存数据库 |
| `mmdb_mem_close(db)` | 关闭内存数据库 |
| `mmdb_mem_create_context(db, name)` | 创建上下文 |
| `mmdb_mem_destroy_context(ctx)` | 销毁上下文 |
| `mmdb_mem_alloc(ctx, size)` | 分配内存 |
| `mmdb_mem_free(ctx, ptr)` | 释放内存 |
| `mmdb_mem_reset(ctx)` | 重置上下文 |
| `mmdb_mem_stats(ctx)` | 获取统计信息 |
| `mmdb_mem_register_resource(...)` | 注册资源析构 |
| `mmdb_mem_unregister_resource(...)` | 取消注册资源 |

### 8.2 MemoryContext 核心 API

| API | 说明 |
|-----|------|
| `AllocSetContextCreate(...)` | 创建 AllocSet 上下文 |
| `palloc(ctx, size)` | 分配内存 |
| `palloc0(ctx, size)` | 分配零初始化内存 |
| `pfree(ctx, ptr)` | 释放内存（严格模式） |
| `MemoryContextReset(ctx)` | 重置上下文 |
| `MemoryContextDelete(ctx)` | 删除上下文 |
| `MemoryContextSwitchTo(ctx)` | 切换当前上下文 |
| `MemoryContextGetGeneration(ctx)` | 获取 generation |
| `MemoryContextSetThreadOwner(ctx, tid)` | 设置线程归属 |
| `MemoryContextCheckThread(ctx)` | 校验线程归属 |

### 8.3 错误码参考

| 错误码 | 说明 |
|--------|------|
| `MMDB_MEMCTX_OK` | 成功 |
| `MMDB_MEMCTX_INVALID_CONTEXT` | 无效上下文 |
| `MMDB_MEMCTX_INVALID_POINTER` | 无效指针 |
| `MMDB_MEMCTX_CROSS_CONTEXT_FREE` | 跨上下文释放 |
| `MMDB_MEMCTX_DOUBLE_FREE` | 双重释放 |
| `MMDB_MEMCTX_LIMIT_EXCEEDED` | 超出限额 |
| `MMDB_MEMCTX_OVERFLOW` | 溢出 |
| `MMDB_MEMCTX_OOM` | 内存不足 |
| `MMDB_MEMCTX_WRONG_THREAD` | 线程不匹配 |
| `MMDB_MEMCTX_ALREADY_DELETED` | 已删除 |

---

## 9. 性能基准

### 9.1 AllocSet 预设性能对比

| 预设 | 10K 小对象分配 | 1K 大对象分配 | 内存效率 |
|------|---------------|--------------|---------|
| `DEFAULT` | 1.0x (基准) | 1.0x | 中等 |
| `SMALL高频` | 1.5x | 0.8x | 高（小对象） |
| `LARGE` | 0.6x | 1.8x | 高（大对象） |
| `BULK` | 0.3x | 2.5x | 最高（批量） |

### 9.2 严格模式开销

| 模式 | 分配开销 | 释放开销 | 适用场景 |
|------|---------|---------|---------|
| Release | 0% | 0% | 生产环境 |
| Debug (STRICT_FREE=1) | ~5% | ~10% | 开发调试 |

---

## 10. 故障排查

### 10.1 常见错误及解决方案

| 错误信息 | 原因 | 解决方案 |
|---------|------|---------|
| `魔数校验失败` | 内存被覆写或已释放 | 检查指针使用，启用严格模式 |
| `双重释放` | 同一指针释放两次 | 检查释放逻辑，使用哨兵机制 |
| `跨上下文释放` | 用 A 上下文释放 B 上下文的内存 | 确保在同一上下文释放 |
| `线程不匹配` | 多线程共享上下文 | 每线程独立上下文 |
| `内存不足` | 块分配失败 | 检查系统内存，调整块大小 |

### 10.2 调试步骤

1. **启用严格模式**：编译时定义 `MMDB_MEMCTX_STRICT_FREE=1`
2. **查看统计信息**：打印 `current_bytes`, `peak_bytes`, `invalid_free_count`
3. **检查 generation**：对比 reset 前后的 generation 值
4. **验证线程归属**：调用 `MemoryContextCheckThread()` 校验
5. **查看析构回调**：确认资源析构回调是否正确注册

---

## 参考资料

- [PostgreSQL MemoryContext 文档](https://www.postgresql.org/docs/current/memory-management.html)
- [设计文档](docs/memory-context-design.md)
- [迁移报告](docs/memory-context-migration-report.md)
- [API 参考](docs/memory-context-api-reference.md)
