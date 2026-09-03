# 完备内存上下文管理系统设计

> **定位**：PostgreSQL 级别完整，不是“最小可用”。
>
> **当前状态**：项目已有 `engineering/include/db/sql/memctx.h` 与 `engineering/src/db/sql/memctx.c`，但只覆盖 SQL 执行器临时内存，尚未形成真正完整的全局内存生命周期管理。

---

## 一、设计目标

### 1.1 目标

建立一套真正完整的内存上下文管理系统，满足：

1. PostgreSQL 级别语义完整性；
2. 全项目统一的内存所有权模型；
3. SDK / DB / SQL 三条链路全部接入；
4. 明确的生命周期层级；
5. 可观测、可诊断、可限额、可审计；
6. 一次性全量迁移的可行路径。

### 1.2 非目标

1. 不引入独立 bump / pool / tcmalloc 多实现路线；
2. 不把 `pfree` 设计成完全无代价的真物理释放；
3. 不追求跨连接共享上下文；
4. 不改动已有公开 `mmdb_*` 函数签名。

---

## 二、现有问题

当前实现存在以下缺口：

1. **只覆盖 SQL Executor**，SDK / vector / text / graph / timeseries / RAG / aggregation 等模块仍大量使用 `malloc/calloc/realloc/free`。
2. **缺少真正的上下文切换**，业务代码需到处显式传递 `MemoryContext`。
3. **`pfree()` 是空操作**，不能检测非法指针、重复释放、跨上下文释放。
4. **缺少资源析构机制**，无法安全管理 `sqlite3_stmt`、文件句柄、HNSW 索引、roaring bitmap 等非内存资源。
5. **缺少限额、峰值、OOM、统计**，`mem_allocated` 不能反映当前真实占用。
6. **`mmdb_t` 没有根内存上下文**，未形成每连接独立上下文树。
7. **线程归属无检测**，上下文可能被错误跨线程访问。
8. **子/父上下文删除契约不完整**，错误路径缺少统一清理模型。

---

## 三、架构选择

### 3.1 方案 A：SQL-only 扩展

只完善 `memctx.c`，SDK / DB 继续使用 `malloc/free`。

- 优点：改动小
- 缺点：无法实现“真正完整的管理系统”

**结论：不采用**

### 3.2 方案 B：完全采用 PostgreSQL 原生 API

直接把所有业务代码统一成 PG 原生 MemoryContext 语义。

- 优点：语义一致
- 缺点：公开 SDK 接口被内部实现污染，迁移风险高，独立模块/测试/后台任务接入不便

**结论：不直接采用**

### 3.3 方案 C：PG 核心 + 项目兼容层 + 全量迁移

1. 在 `engineering/include/db/sql/memctx.h` / `engineering/src/db/sql/memctx.c` 建立 PostgreSQL 核心能力；
2. 新增 SDK 项目兼容层 `mmdb_mem_*`；
3. 一次性全量迁移所有业务代码到 MemoryContext；
4. 保留公开 `mmdb_*` 接口不变。

- 优点：完整性最高，迁移后架构最清晰，既有 SQL 路径与 SDK 路径可统一演进
- 缺点：工作量大，需要一次性全量切换

**结论：推荐采用**

---

## 四、总体架构

```text
业务 / API 层
mmdb_open / collection / vector / text / graph / query / rag
        |
项目内存兼容层
mmdb_mem_alloc / mmdb_mem_calloc / mmdb_mem_realloc / mmdb_mem_free / mmdb_memctx_create / mmdb_memctx_delete
        |
PostgreSQL 风格 MemoryContext 核心层
CurrentMemoryContext / SwitchTo / Reset / Delete / 资源析构 / 统计 / 限额
        |
AllocSet 分配器
普通块 + 大对象块 + 指数扩张 + 对齐 + 统计头
        |
系统分配器
malloc / free
```

---

## 五、生命周期模型

每个 `mmdb_t` 拥有独立上下文树：

```text
DatabaseContext
├── ConnectionContext
│   ├── RequestContext
│   │   ├── QueryContext
│   │   │   ├── PlanContext
│   │   │   ├── ExecutionContext
│   │   │   ├── ResultContext
│   │   │   └── TemporaryContext
│   │   └── TransactionContext
│   ├── CollectionContext
│   │   ├── VectorIndexContext
│   │   ├── TextIndexContext
│   │   └── GraphContext
│   └── CacheContext
└── BackgroundContext
```

### 5.1 生命周期语义

| 上下文 | 创建时机 | Reset 时机 | Delete 时机 |
|---|---|---|---|
| DatabaseContext | `mmdb_open` | 不主动 Reset | `mmdb_close` |
| ConnectionContext | 连接初始化 | 连接复用/会话清理时 | 连接关闭 |
| RequestContext | 每个请求开始 | 请求结束 | 请求结束 |
| QueryContext | SQL / SDK 查询开始 | 查询结束 | 查询结束 |
| TransactionContext | 事务开始 | 回滚或提交后 | 事务结束 |
| ResultContext | 结果构建前 | 新结果覆盖前 | 请求结束 |
| CollectionContext | 打开 collection | collection 重载 | collection 关闭 |
| CacheContext | 缓存创建 | 缓存淘汰时 | 数据库关闭时 |
| TemporaryContext | 临时操作前 | 操作结束 | 父上下文删除 |

### 5.2 关键原则

1. **短生命周期对象分配到短生命周期上下文。**
2. **跨请求对象禁止分配到 RequestContext。**
3. **索引、缓存、collection 元数据分配到长期上下文。**
4. **查询结果只能引用生命周期更长的对象，或复制到 ResultContext。**
5. **错误路径统一依赖上下文释放，不逐项手动 `free()`。**

---

## 六、核心数据结构

### 6.1 MemoryContext 扩展结构

```c
typedef struct MemoryContextData {
    NodeTag type;

    MemoryContext parent;
    MemoryContext firstchild;
    MemoryContext prevchild;
    MemoryContext nextchild;

    const MemoryContextMethods *methods;
    char *name;

    /* 统计 */
    Size current_bytes;
    Size peak_bytes;
    Size total_allocated;
    Size total_freed;
    Size allocation_count;
    Size free_count;

    /* 限额 */
    Size max_bytes;

    /* 校验与生命周期 */
    uint64_t generation;
    uint32_t flags;

    /* 资源析构 */
    struct MemoryResource *resources;

    /* OOM 策略 */
    void (*on_oom)(MemoryContext context, Size requested, void *arg);
    void *on_oom_arg;

    /* 状态 */
    bool is_reset;
    bool is_deleted;

    /* 线程归属 */
    bool is_thread_owner;
    uint64_t owner_thread_id;
} MemoryContextData;
```

### 6.2 Allocation Header

每次用户分配前插入隐藏头部：

```c
typedef struct MemoryAllocationHeader {
    uint64_t magic;
    Size requested_size;
    Size allocated_size;
    MemoryContext owner;
    uint64_t generation;
    uint32_t flags;
} MemoryAllocationHeader;
```

用户指针布局：

```text
[MemoryAllocationHeader][对齐后的用户数据]
```

作用：

1. 检测非法指针；
2. 检测是否属于当前上下文；
3. 检测是否重复释放；
4. 支持统计与诊断；
5. 支持 generation 检查。

### 6.3 大对象块

```c
typedef struct AllocSetLargeBlock {
    Size size;
    Size requested;
    struct AllocSetLargeBlock *next;
} AllocSetLargeBlock;
```

当请求大小满足：

```text
size > max_block_size / 2
```

则分配为独立大对象块，不挤占普通块。

---

## 七、完整 API

### 7.1 核心上下文 API

```c
MemoryContext MemoryContextCreate(
    MemoryContext parent,
    const char *name,
    Size min_context_size,
    Size init_block_size,
    Size max_block_size,
    Size max_bytes);

void MemoryContextReset(MemoryContext context);
void MemoryContextDelete(MemoryContext context);
void MemoryContextResetChildren(MemoryContext context);
```

### 7.2 上下文切换

```c
MemoryContext MemoryContextCurrent(void);
MemoryContext MemoryContextSwitchTo(MemoryContext context);
```

使用模式：

```c
MemoryContext old = MemoryContextSwitchTo(query_context);
void *node = palloc(sizeof(ParseNode));
MemoryContextSwitchTo(old);
```

必须支持嵌套恢复。

### 7.3 分配 API

#### PG 风格

```c
void *MemoryContextAlloc(MemoryContext context, Size size);
void *MemoryContextAllocZero(MemoryContext context, Size size);
void *MemoryContextRealloc(MemoryContext context, void *pointer, Size size);
void MemoryContextFree(MemoryContext context, void *pointer);
```

#### 当前上下文快捷 API

```c
void *palloc(Size size);
void *palloc0(Size size);
void *repalloc(void *pointer, Size size);
void pfree(void *pointer);
```

#### 项目兼容层

```c
mmdb_memctx_t *mmdb_memctx_create(mmdb_memctx_t *parent, const char *name, size_t max_bytes);
void *mmdb_mem_alloc(mmdb_memctx_t *ctx, size_t size);
void *mmdb_mem_calloc(mmdb_memctx_t *ctx, size_t count, size_t size);
void *mmdb_mem_realloc(mmdb_memctx_t *ctx, void *ptr, size_t size);
char *mmdb_mem_strdup(mmdb_memctx_t *ctx, const char *value);
void mmdb_mem_free(mmdb_memctx_t *ctx, void *ptr);
void mmdb_memctx_reset(mmdb_memctx_t *ctx);
void mmdb_memctx_delete(mmdb_memctx_t *ctx);
```

### 7.4 资源析构

```c
int mmdb_mem_register_resource(
    mmdb_memctx_t *ctx,
    void *resource,
    void (*destructor)(void *resource, void *arg),
    void *arg,
    const char *name);

int mmdb_mem_unregister_resource(mmdb_memctx_t *ctx, void *resource);
```

销毁顺序：

1. 当前上下文资源按 LIFO 顺序执行析构；
2. 再递归处理子上下文；
3. 最后释放 AllocSet 块。

### 7.5 Reset/Delete 语义

#### Reset

1. 执行资源析构；
2. 递归 Reset 子上下文；
3. 保留上下文对象；
4. 保留首块；
5. 释放扩展块；
6. `generation += 1`；
7. `current_bytes = 0`。

#### Delete

1. 执行资源析构；
2. 递归 Delete 子上下文；
3. 释放所有块；
4. 从父链表移除；
5. 释放上下文本体；
6. 后续访问必须被检测为非法。

---

## 八、pfree 语义

### 8.1 默认模式：逻辑释放

`pfree(pointer)` 执行：

1. 检查 `magic`；
2. 检查 `owner`；
3. 检查 `generation`；
4. 标记 `MemoryAllocationHeader` 为已释放；
5. 更新统计；
6. 不立即归还物理块。

这保留了 AllocSet 的高效性，同时提供完整诊断能力。

### 8.2 可选严格模式

编译或运行时开关：

```text
MMDB_MEMCTX_STRICT_FREE
```

严格模式下：

1. 小对象可进入尺寸级空闲链表；
2. 大对象立即释放；
3. 对 `double free` 记录错误并拒绝；
4. 适合调试和验证阶段。

第一版优先实现统计、检测、诊断，不把复杂 freelist 作为必须。

---

## 九、统计与监控

### 9.1 统计结构

```c
typedef struct MemoryContextStats {
    Size current_bytes;
    Size peak_bytes;
    Size total_allocated;
    Size total_freed;
    Size allocation_count;
    Size free_count;
    Size reset_count;
    Size oom_count;
    Size invalid_free_count;
    Size double_free_count;
    Size resource_count;
    Size child_count;
} MemoryContextStats;
```

### 9.2 API

```c
int MemoryContextGetStats(MemoryContext context, MemoryContextStats *stats);
void MemoryContextStatsPrint(
    MemoryContext context,
    void (*writer)(const char *line, void *arg),
    void *arg);
Size MemoryContextGetUsed(MemoryContext context);
Size MemoryContextGetPeak(MemoryContext context);
```

### 9.3 监控指标建议

```text
mmdb_memory_current_bytes
mmdb_memory_peak_bytes
mmdb_memory_allocations_total
mmdb_memory_resets_total
mmdb_memory_oom_total
mmdb_memory_invalid_free_total
mmdb_memory_context_count
```

---

## 十、线程与连接隔离

### 10.1 规则

1. 每个 `mmdb_t` 拥有独立 `DatabaseContext`；
2. 每个请求线程只能使用自己的 `ConnectionContext` / `RequestContext`；
3. 一个 `MemoryContext` 不允许被其他线程访问；
4. 跨线程传递结果必须复制到目标线程上下文；
5. 上下文本身不加入全局热路径锁。

### 10.2 线程归属检查

创建时记录：

```text
owner_thread_id
```

每次操作可检测：

```text
context->owner_thread_id == mmdb_current_thread_id()
```

允许的例外：

1. 只读统计；
2. 明确标记为共享的未来 `SharedMemoryContext`。

当前阶段不实现共享上下文。

---

## 十一、mmdb_t 接入

### 11.1 扩展字段

在 `mmdb_s` 中追加：

```c
struct mmdb_s {
    /* 原有字段位置不动 */

    MemoryContext memory_context;       /* DatabaseContext */
    MemoryContext connection_context;   /* ConnectionContext */
    MemoryContext cache_context;        /* CacheContext */
};
```

### 11.2 mmdb_open 初始化顺序

1. 分配临时 `mmdb_t` 结构；
2. 创建 `DatabaseContext`；
3. 创建 `ConnectionContext`；
4. 创建 `CacheContext`；
5. 把数据库路径、错误信息、collection 数组迁移到对应上下文；
6. 初始化 SQLite 与锁；
7. 注册 SQLite 句柄析构回调；
8. 任意失败时删除根上下文，统一回收。

### 11.3 mmdb_close 关闭顺序

1. 阻止新请求；
2. 等待活跃请求结束；
3. 关闭 / 注销 collection；
4. 销毁索引与模型资源；
5. 关闭 SQLite；
6. 删除 ConnectionContext；
7. 删除 CacheContext；
8. 删除 DatabaseContext；
9. 释放 `mmdb_t` 本体。

---

## 十二、请求级接入

### 12.1 请求守卫

```c
typedef struct mmdb_request_scope {
    mmdb_t *db;
    MemoryContext context;
    MemoryContext previous;
    int active;
} mmdb_request_scope_t;
```

### 12.2 API

```c
int mmdb_request_begin(mmdb_t *db, const char *name, mmdb_request_scope_t *scope);
void mmdb_request_end(mmdb_request_scope_t *scope);
```

### 12.3 语义

`mmdb_request_end()` 必须：

1. 恢复 previous context；
2. 执行资源析构；
3. 删除请求上下文；
4. 清理 scope；
5. 即使业务失败也必须执行。

---

## 十三、全量迁移策略

### 13.1 分类

#### A. 请求临时对象

迁移到 `RequestContext` / `QueryContext`：

- 查询结果中间数组
- SQL AST
- 过滤器解析对象
- 临时距离缓冲区
- JSONPath 节点
- 聚合中间状态

#### B. 事务对象

迁移到 `TransactionContext`：

- MVCC 快照
- 事务状态
- undo/redo 辅助结构
- 事务级临时索引

#### C. Collection 长期对象

迁移到 `CollectionContext`：

- collection 名称
- schema
- HNSW 索引
- 文本索引
- graph 元数据
- 向量 ID 映射

#### D. 数据库级长期对象

迁移到 `DatabaseContext` / `CacheContext`：

- collection 缓存
- prepared statement 缓存
- 统计信息
- 配置快照

#### E. 不由上下文直接管理，但需挂资源析构

- SQLite 句柄
- 文件描述符
- mmap
- 外部模型
- 线程 / 条件变量
- 第三方库对象

### 13.2 迁移规则

原代码：

```c
char *name = strdup(input);
if (!name) return MMDB_ERR_NOMEM;
```

迁移后：

```c
char *name = mmdb_mem_strdup(request_context, input);
if (!name) return MMDB_ERR_NOMEM;
```

原代码：

```c
sqlite3_stmt *stmt = mmdb_sqlite_prepare(...);
// 多处 sqlite3_finalize(stmt)
```

迁移后：

```c
sqlite3_stmt *stmt = mmdb_sqlite_prepare(...);
mmdb_mem_register_resource(request_context, stmt, mmdb_destroy_sqlite_stmt, NULL, "sqlite-stmt");
```

### 13.3 迁移范围

所有业务模块：

1. `sdk/core`
2. `sdk/vectors`
3. `sdk/text`
4. `sdk/graph`
5. `sdk/timeseries`
6. `sdk/aggregation`
7. `sdk/extra`
8. `db/api`
9. `db/sql/executor`
10. `db/replication`
11. `db/concurrency`
12. `kbase`

每个模块迁移后必须：

1. 建立明确父上下文；
2. 删除模块内直接 `malloc/free`；
3. 统一错误出口；
4. 增加至少一条错误路径测试；
5. 通过模块测试后再迁移下一模块。

### 13.4 允许保留直接 malloc 的位置

1. `memctx.c` 底层 AllocSet 实现；
2. 第三方库适配边界；
3. 明确标注的外部资源释放点；
4. 专门的测试验证代码。

业务模块不再保留直接 `malloc/calloc/realloc/free`。

---

## 十四、调试与诊断

### 14.1 调试开关

```c
#define MMDB_MEMORY_DEBUG 1
```

### 14.2 调试能力

1. `magic` 校验；
2. 释放后填充 `0xDD`；
3. 新分配填充 `0xCD`；
4. 删除上下文后 poison；
5. guard 字节；
6. 指针归属校验；
7. `generation` 校验；
8. 当前线程校验；
9. 上下文树转储；
10. 未释放资源报告。

### 14.3 Release 行为

Release 模式关闭部分 guard，但仍保留：

1. 限额；
2. 统计；
3. 资源析构；
4. 基本 owner 校验；
5. OOM 统计。

---

## 十五、错误处理

### 15.1 内存系统错误码

```c
typedef enum MemoryContextError {
    MMDB_MEMCTX_OK = 0,
    MMDB_MEMCTX_INVALID_CONTEXT,
    MMDB_MEMCTX_INVALID_POINTER,
    MMDB_MEMCTX_CROSS_CONTEXT_FREE,
    MMDB_MEMCTX_DOUBLE_FREE,
    MMDB_MEMCTX_LIMIT_EXCEEDED,
    MMDB_MEMCTX_OVERFLOW,
    MMDB_MEMCTX_OOM,
    MMDB_MEMCTX_WRONG_THREAD,
    MMDB_MEMCTX_ALREADY_DELETED
} MemoryContextError;
```

### 15.2 SDK 映射

| MemoryContext 错误 | SDK 错误 |
|---|---|
| OOM / Limit Exceeded | `MMDB_ERR_NOMEM` |
| Invalid Pointer / Invalid Context / Wrong Thread / Overflow | `MMDB_ERR_INVALID` |

### 15.3 设计约束

1. 内存系统不能直接 `abort()`；
2. 所有错误必须可恢复；
3. 分配失败统一返回 `NULL`；
4. 上层负责映射为项目错误码。

---

## 十六、测试设计

### 16.1 单元测试

文件：

```text
engineering/test/db/sql/memctx_test.cpp
```

覆盖：

1. 根上下文创建；
2. 多级父子上下文；
3. 不同尺寸分配；
4. 8 / 16 字节对齐；
5. 零初始化；
6. 普通块增长；
7. 大对象独立分配；
8. Reset 后首块复用；
9. Delete 递归释放；
10. 父子链表维护；
11. realloc 扩容 / 缩容；
12. pfree 统计；
13. 重复释放；
14. 跨上下文释放；
15. 无效指针释放；
16. OOM 限额；
17. 分配整数溢出；
18. 资源析构顺序；
19. 上下文切换嵌套；
20. 当前线程校验；
21. 峰值统计；
22. generation 变化。

### 16.2 SDK 集成测试

文件：

```text
engineering/test/sdk/memory/mmdb_memctx_integration_test.cpp
```

覆盖：

1. `mmdb_open/mmdb_close` 无泄漏；
2. vector insert/search/delete；
3. text insert/search；
4. graph vertex/edge；
5. timeseries 写入与聚合；
6. RAG 检索；
7. 事务提交；
8. 事务回滚；
9. 查询失败错误路径；
10. SQLite statement 自动析构；
11. collection 重复打开 / 关闭；
12. 多请求连续复用；
13. 结果集在请求结束前可用。

### 16.3 全量迁移验证

```bash
rg "\b(malloc|calloc|realloc|free|strdup)\s*\(" engineering/src
```

允许出现的位置仅限：

1. `memctx.c`
2. 第三方库适配边界
3. 明确外部资源释放点
4. 测试代码

业务模块不允许继续直接调用系统分配器。

### 16.4 构建与运行验证

```bash
cmake -B build/engineering -S engineering -DBUILD_TESTING=ON -DMMDB_MEMORY_DEBUG=ON
cmake --build build/engineering --parallel 4
ctest --test-dir build/engineering -R "memctx|memory|sdk|vector|text|graph|timeseries" --output-on-failure
```

配合：

1. AddressSanitizer；
2. UndefinedBehaviorSanitizer；
3. Windows Application Verifier；
4. 长时间压力测试；
5. 多线程请求隔离测试。

---

## 十七、实施阶段

### 阶段 1：核心能力补全

文件：

- `engineering/include/db/sql/memctx.h`
- `engineering/src/db/sql/memctx.c`

完成：

1. `CurrentMemoryContext` / `SwitchTo`
2. `realloc`
3. allocation header
4. 资源析构
5. 内存限额
6. 统计
7. 大对象块
8. 错误码
9. 线程归属
10. Reset / Delete 生命周期保护

### 阶段 2：项目兼容层

新增：

- `engineering/include/sdk/impl/mmdb_memctx.h`
- `engineering/src/sdk/core/mmdb_memctx.c`

完成：

1. SDK 类型包装
2. 溢出检查
3. `calloc/realloc/strdup`
4. 资源注册
5. 错误映射
6. 请求上下文守卫

### 阶段 3：数据库根上下文

修改：

- `engineering/include/sdk/impl/mmdb_internal.h`
- `engineering/src/sdk/core/mmdb.c`
- `engineering/src/sdk/core/result.c`

完成：

1. `mmdb_t` 根上下文
2. collection 上下文
3. `open/close` 生命周期
4. 错误信息迁移
5. collection 缓存迁移

### 阶段 4：SQL Executor 迁移

修改：

- `engineering/src/db/sql/executor.c`
- `engineering/src/db/sql/expr.c`
- 其他 SQL 模块

完成：

1. 统一 `MemoryContextSwitchTo`
2. 去除重复手工释放
3. TupleTableSlot 等对象接入上下文
4. 资源回调接入

### 阶段 5：SDK 全量迁移

按模块顺序：

1. `sdk/core`
2. `sdk/vectors`
3. `sdk/text`
4. `sdk/graph`
5. `sdk/timeseries`
6. `sdk/aggregation`
7. `sdk/extra`
8. `db/api`
9. `db/sql/executor`
10. `db/replication`
11. `db/concurrency`
12. `kbase`

每个模块：

1. 确定父上下文
2. 删除直接 malloc/free
3. 统一错误出口
4. 增加错误路径测试
5. 完成模块测试

### 阶段 6：全量扫描与清理

1. 扫描所有业务源文件
2. 清除残留直接分配
3. 检查 realloc 与所有权
4. 检查跨请求指针
5. 检查上下文删除顺序
6. 执行完整回归测试
7. 输出内存管理报告

---

## 十八、验收标准

### 18.1 功能验收

- [ ] 支持父子 MemoryContext 树
- [ ] 支持上下文切换与恢复
- [ ] 支持普通块和大对象块
- [ ] 支持 alloc/zero/realloc/free
- [ ] 支持 Reset/Delete
- [ ] 支持资源析构
- [ ] 支持内存限制
- [ ] 支持统计与峰值
- [ ] 支持 OOM 回调
- [ ] 支持非法指针检测
- [ ] 支持 double-free 检测
- [ ] 支持跨上下文释放检测
- [ ] 支持线程归属检测
- [ ] 支持调试转储

### 18.2 迁移验收

- [ ] `mmdb_t` 拥有独立根上下文
- [ ] 每个请求拥有独立 RequestContext
- [ ] SQL Executor 全部使用 MemoryContext
- [ ] SDK/DB 业务代码不再直接使用 `malloc/calloc/realloc/free`
- [ ] HNSW、文本索引、graph、timeseries、RAG 接入正确生命周期
- [ ] SQLite statement 等外部资源具备自动析构回调
- [ ] 所有错误路径不泄漏

### 18.3 并发验收

- [ ] 每连接上下文不跨线程共享
- [ ] 跨线程使用可被检测
- [ ] 并发查询不共享可变上下文
- [ ] 统计读取不破坏业务分配路径

### 18.4 性能验收

- [ ] 小对象分配不退化为每次系统调用
- [ ] 查询上下文 Reset 可复用首块
- [ ] 长请求没有持续块泄漏
- [ ] 内存统计开销可接受
- [ ] Release 模式不引入明显查询延迟回归

---

## 十九、结论

当前系统**并非完全没有内存上下文**，而是已经有一个只覆盖 SQL 执行器的初版：

```text
SQL Executor：已有 AllocSet MemoryContext
SDK / DB 其他模块：仍以 malloc/free 为主
```

要达到“真正完整的管理系统”，需要演进为：

```text
每连接独立根上下文
  -> 请求/事务/查询/结果/collection 分层上下文
    -> AllocSet 块分配器
      -> 资源析构、限额、统计、OOM、调试、线程归属检查
        -> SDK/DB 全量迁移
```

推荐方案为 **PG 核心 + 项目兼容层 + 一次性全量迁移**。

这是目前唯一能满足“真正完整”要求的路线。
