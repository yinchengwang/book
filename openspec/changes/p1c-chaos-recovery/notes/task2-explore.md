# Task 2 探查报告 — chaos 现状 + 间隔参数化路径

> 探查者：P1-C Task 2 探查 implementer
> 分支：`feature/p1c-chaos-recovery`（HEAD `8dcd1e1e9`）
> 上游：P1-B 已合入 main `03eb3f91e`
> 范围：定位后续 Task 3-5 实现 `chaos_e2e 5 用例 + vdb_chaos 扩展 2-3` 所需的全部事实
> 根因：`vector_api_create` 仍硬编码 `interval_ms=5000`，且 `vector_api_set_checkpoint_interval()` setter 不存在

---

## 1. 现状盘点 — vdb_chaos 6 用例的覆盖范围与空白矩阵

### 1.1 测试套件位置

`engineering/test/db/integration/vdb_chaos_test.cpp`（533 行）使用 gtest fixture
`VDBChaosTest`，目录 `./test-results/vdb_chaos`（受 `_mkdir(test_data_dir)` 与
`fs::remove_all` 管理）；`SetUp` 调用 `vector_api_create(test_data_dir)`（**未传
interval_ms** → 默认走 P1-B 5s），`TearDown` 调用 `vector_api_destroy`。

### 1.2 6 用例 × 5 维度覆盖矩阵

| # | 测试 | 场景 | 模式 | kill-during-write | partial-write | replay 中 crash | 并发写 + crash | 短 interval |
|---|------|------|------|------|------|------|------|------|
| 1 | `WriteKillRecovery` | 100 save 后再 insert 50 → 不 save destroy | save+insert+kill | ✓ | ✗ | ✗ | ✗ | ✗ |
| 2 | `IndexRebuildAfterKill` | 10 批 × 100=1000 → 不 save destroy | save+insert+kill | ✓ | ✗ | ✗ | ✗ | ✗ |
| 3 | `DiskFullSimulation` | 一次性大 batch 10000（无 destroy） | insert | ✗ | ✗ | ✗ | ✗ | ✗ |
| 4 | `MultipleCrashRecovery` | 3 轮 insert+kill+reload | save+insert+kill | ✓ | ✗ | ✗ | ✗ | ✗ |
| 5 | `ConcurrentWriteWithCrash` | 4 线程各 50 → 100ms 后 destroy | save+insert+kill | ✗ | ✗ | ✗ | ✓（部分） | ✗ |
| 6 | `SaveLoadCorrectness` | 500 + save + destroy + reload | save+insert+kill | ✗ | ✗ | ✗ | ✗ | ✗ |

### 1.3 空白矩阵（强 → 弱）

| 缺口 | 现状 | 影响 |
|------|------|------|
| **kill-during-checkpoint** | 6 用例均在 save/insert 完成后 destroy，未在 `vector_wal_flush/checkpoint/truncate` 中途 kill | 验证不到 checkpoint 内部状态撕裂 |
| **kill-during-replay** | `load → 第二次 destroy` 路径不存在 | 验证不到 replay 幂等性 + 段边界 |
| **partial-write recovery** | 完全不模拟 WAL 末尾字节截断/破坏 | 验证不到 CRC 校验 + 头部 `last_checkpoint` 容错 |
| **短 interval** | interval_ms 编译时硬编码 5000 | 5s 周期下 chaos 触发概率接近 0，stress 不可达 |
| **并发写 + save + crash** | `ConcurrentWriteWithCrash` 仅 4 写 + 100ms sleep，无后台 save | 验证不到后台 checkpoint 与 save 并发的副本撕裂 |

### 1.4 fixture 公共设施

- `make_create_params(name, dim, idx_type, metric)` —— 构造 `VectorCreateParams`
- `make_insert_params(coll, n, ids, vecs, meta, meta_sizes)` —— 包装
- `make_search_params(...)` —— 包装查询
- `generate_vectors(n, dim)` / `free_vectors(...)` —— 随机向量生成
- `cleanup_test_dir()` —— 删除 + 重建目录
- `test_data_dir = "./test-results/vdb_chaos"`（全 suite 共用，P1-C 扩展需谨慎共享）

---

## 2. vector_api_create 内部流程（P1-B 已集成 WAL）

### 2.1 当前 vector_api_create（vector_api.c:249-295）逐行注释

```text
249 VectorAPI *vector_api_create(const char *data_dir) {
250     VectorAPI *api = (VectorAPI *)calloc(1, sizeof(*api));   // 分配 api 句柄（含 wal=NULL / checkpoint_mgr=NULL）
251     if (!api) return NULL;
253     if (data_dir) {
254         strncpy(api->data_dir, data_dir, sizeof(api->data_dir) - 1);  // 拷贝 data_dir 路径
255     } else {
256         strcpy(api->data_dir, "./vector_data");                       // 默认数据目录
257     }
259     api->capacity = 16;                                            // 集合数组初始容量
260     api->collections = (VectorCollectionImpl **)calloc(...);        // 分配 16 个 collection 槽
261     if (!api->collections) { free(api); return NULL; }
266     vector_query_init();                                           // 初始化查询执行器（全局一次）
269     // —————— P1-B Task 4：打开/创建 WAL ——————
272     api->wal = vector_wal_open(api->data_dir, 0);                   // 拼 {data_dir}/vector_wal_0.log
273     if (!api->wal) {
274         LOG_WARN("WAL 打开失败，vector_api 将在无 WAL 模式下运行...");  // 软失败，无 WAL 仍可用
275     }
277     // —————— P1-B Task 7：启动后台 CheckpointManager ——————
280     if (api->wal) {                                                 // 仅在 WAL 开启时跑后台
281         api->checkpoint_mgr = checkpoint_manager_create(api->wal, 5000);  // ←【硬编码 5s】
282         if (api->checkpoint_mgr) {
283             if (checkpoint_manager_start(api->checkpoint_mgr) != 0) {
284                 LOG_WARN("CheckpointManager 启动失败");
285                 checkpoint_manager_destroy(api->checkpoint_mgr);
286                 api->checkpoint_mgr = NULL;
287             }
288         } else {
289             LOG_WARN("CheckpointManager 创建失败");
290         }
291     }
293     LOG_INFO("VectorAPI 创建成功: data_dir=%s", api->data_dir);
294     return api;
295 }
```

### 2.2 参数化路径识别

| 字段 | 类型 | 当前值 | 暴露方式 |
|------|------|--------|----------|
| `data_dir` | `const char*` | 由调用方传 | 已是函数参数 |
| `interval_ms` | `uint32_t` | **硬编码 5000** | 需新增 `vector_api_create_with_interval(data_dir, interval_ms)` |
| `checkpoint_interval_ms`（结构体字段） | `uint32_t` | **不存在** | 需在 `VectorAPI_s` 加字段 |

### 2.3 结论

`checkpoint_manager_create(api->wal, 5000)` 是 **唯一的参数化切入点**：
把字面量 `5000` 替换为 `interval_ms` 形参即可，函数内部调用完全不必改
（`checkpoint_manager_create` 已经接受 `interval_ms`）。

---

## 3. vector_api_destroy 销毁顺序（vector_api.c:297-322）

### 3.1 当前顺序与不变式

```text
297 void vector_api_destroy(VectorAPI *api) {
298     if (!api) return;
302     if (api->checkpoint_mgr) {
303         checkpoint_manager_stop(api->checkpoint_mgr);     // (1) 先停后台线程（broadcast cond + join）
304         checkpoint_manager_destroy(api->checkpoint_mgr);  // (2) 销毁 mgr（lock/cond destroy + free）
305         api->checkpoint_mgr = NULL;
306     }
309     if (api->wal) {
310         vector_wal_close(api->wal);                       // (3) 关闭 WAL（必在 collection 销毁前）
311         api->wal = NULL;
312     }
314     for (int32_t i = 0; i < api->n_collections; i++) {
315         collection_destroy(api->collections[i]);          // (4) 释放集合
316     }
317     free(api->collections);
319     vector_query_shutdown();
320     free(api);
321 }
```

### 3.2 不变式

- **(1)→(3)**：`mgr` 持有 `wal` 指针，必须先 stop mgr 再关 wal；
- **(3)→(4)**：collection 内 `query_plan` 可能引用 wal，wal 必须早关；
- `checkpoint_manager_destroy` 内部已是 stop+free 原子，但 vector_api_destroy 再次 stop 是幂等的（`atomic_load(&mgr->started)` 检查）。

### 3.3 加 setter 后是否需要 mutex

**结论：setter 与 insert/save/search 不需要锁**，原因：

1. `vector_api_set_checkpoint_interval()` 只触碰 `api->checkpoint_mgr`（独立指针），
   setter 自身内部用 mutex 保护 stop→destroy→create→start 的原子过渡；
2. `insert` / `save` / `search` 操作的是 `coll->lock`（mmdb_rwlock_t）和 `wal->mutex`
   （vector_wal 内部），与 `api->checkpoint_mgr` 没有共享状态；
3. 唯一需要保护的是 setter 自身两次进入的竞争（并发调 setter）。

### 3.4 推荐 destroy 顺序（不变）

Task 4 实现后 `vector_api_destroy` 维持上述顺序，只需在 setter 路径上额外加 mutex。

---

## 4. CheckpointManager 间隔参数化路径

### 4.1 当前实现已就绪

`checkpoint_manager_create(wal, interval_ms)`（checkpoint_manager.c:100-128）：
```c
mgr->interval_ms = interval_ms;
ms_to_timespec(interval_ms, &mgr->interval_timespec);   // 缓存给 cond_timedwait
```
后台线程 `checkpoint_thread_main`（line 54-94）每次循环 `deadline += interval_timespec`，
因此 **同一 mgr 内部 interval_ms 不能动态改**（设计选择：避免热改越界）。

### 4.2 委托模式（推荐）

新加 `vector_api_create_with_interval(const char *data_dir, uint32_t interval_ms)`：

```c
VectorAPI *vector_api_create(const char *data_dir) {
    return vector_api_create_with_interval(data_dir, 5000);  // 委托，P1-B 行为完全不变
}

VectorAPI *vector_api_create_with_interval(const char *data_dir, uint32_t interval_ms) {
    // 1. 验证 interval_ms（P1-C 约定 0 = 禁用 mgr）
    // 2. 现有的 vector_api_create 主体
    // 3. 把 line 281 的字面量 5000 替换为局部变量 interval_ms
}
```

**优点**：
- 旧 5 个测试套件（vdb_chaos / vdb_checkpoint / vdb_wal_recovery / vdb_e2e / vdb_stress）
  完全不需改；
- chaos_e2e 短 interval 用例直接调 `vector_api_create_with_interval(dir, 100)`；
- 公共 API 一处实现，无重复代码。

### 4.3 接口签名（待 Task 4 实现）

```c
/* vector_api.h 头文件 */
VectorAPI *vector_api_create_with_interval(const char *data_dir, uint32_t interval_ms);
int vector_api_set_checkpoint_interval(VectorAPI *api, uint32_t interval_ms);
```

### 4.4 命名约定

- P1-B 默认值（5000）已在 `vector_api_create` 中固化；**不允许 0 作为默认**；
- `interval_ms == 0` 在 `vector_api_create_with_interval` 语义为"禁用后台 CheckpointManager"，
  仍可手动 save，但 chaos 测试规避此路径（保持 mgr 始终运行便于 kill-during-checkpoint）。

---

## 5. setter 线程安全方案

### 5.1 API 契约

```c
int vector_api_set_checkpoint_interval(VectorAPI *api, uint32_t interval_ms);
```

**行为**：
1. 校验 `api != NULL && interval_ms >= 0`；
2. stop 当前 mgr（pthread_join 已存在）；
3. destroy 当前 mgr；
4. 用新 `interval_ms` create 新 mgr；
5. start 新 mgr；
6. 返回 0；任一步失败返回 -1 并 LOG_WARN。

### 5.2 mutex 保护

#### 5.2.1 锁类型：pthread_mutex_t 直采

`mmdb_lock.h` 当前只暴露 `mmdb_rwlock_t`（SRWLOCK / pthread_rwlock_t），**没有 plain mutex**。
要在 `VectorAPI_s` 加 `pthread_mutex_t mgr_lock`（无需走 mmdb_lock 抽象 ——
读多写少场景无需 rwlock，简单 mutex 即可）。

#### 5.2.2 锁顺序

```text
[setter thread]                       [insert / save / search thread]
  lock(api->mgr_lock)                   ┐
    stop(mgr)                           │  mgr 指针切换期间
    destroy(mgr)                        │  其他线程不触碰 mgr
    mgr = checkpoint_manager_create()   │
    start(mgr)                          │
  unlock(api->mgr_lock)                 ┘
```

**关键**：insert / save / search 路径 **完全不需要获取 mgr_lock**，原因：
- insert 直接操作 `coll->lock` 与 `wal->mutex`，不读 `api->checkpoint_mgr`；
- save 通过 `vector_wal_checkpoint(wal, MANUAL)` 内部 mutex 化；
- search 只读 `coll->lock` 与 `api->collections[]`（不变式：集合数量 → collection 引用，未改）。

#### 5.2.3 唯一竞争点

setter 之间并发调 → `pthread_mutex_lock(&api->mgr_lock)` 串行化；与 destroy 竞争
→ destroy 时不持锁（`destroy` 在 suite 结尾独占调用，未与 setter 并发，可接受）。
若项目需要 destroy-与-setter 并发安全，destroy 入口同样加 `mgr_lock`。

### 5.3 VectorAPI_s 字段扩展

```c
struct VectorAPI_s {
    char data_dir[512];
    VectorCollectionImpl **collections;
    int32_t n_collections;
    int32_t capacity;
    char error_msg[256];
    vector_wal_t *wal;
    CheckpointManager *checkpoint_mgr;
    uint32_t checkpoint_interval_ms;       // ← 新增：默认 5000
    pthread_mutex_t mgr_lock;              // ← 新增：保护 set_checkpoint_interval
};
```

构造函数加 `pthread_mutex_init(&api->mgr_lock, NULL)`，析构加
`pthread_mutex_destroy(&api->mgr_lock)`。

---

## 6. chaos_e2e 5 用例技术细节（设计草案）

> 文件：`engineering/test/db/integration/chaos_e2e_test.cpp`
> fixture：`ChaosE2ETest`（与 VDBChaosTest 同款 tools，独立 test_data_dir）

### 6.1 `ChaosE2ETest::KillDuringCheckpoint`

```text
目的：kill 必须撞上 flush + checkpoint + truncate 三步合一窗口
interval_ms=100   ——— 5s 周期下窗口几乎为 0
循环 10 次取最坏 ——— 概率性测试取最小通过 size
步骤：
  1. vector_api_create_with_interval(dir, 100)
  2. create_collection("ckpt_kill")
  3. insert 1000（触发多次 checkpoint）
  4. for cycle=0..9:
       a. 立即 vector_api_destroy(api)（可能撞 flush 窗口）
       b. api = vector_api_create_with_interval(dir, 100)
       c. ASSERT_EQ load(api) == OK
       d. size = vector_api_size(api, "ckpt_kill")
       e. EXPECT_GE(size, 1) && EXPECT_LE(size, 1000)
       f. 各 cycle 内统计 size，构成"最坏情况记录"
  5. 最终期望：无 corrupt / 无负数 / 任何 cycle 后 reload 仍能进
依赖：vector_api_set_checkpoint_interval 已可热调（若需要再降低至 50ms）
```

### 6.2 `ChaosE2ETest::KillDuringReplay`

```text
目的：replay 中途 kill，验证再次 create 时幂等续接
步骤：
  1. phase1 dir=dir_a: create(5s) + insert(10000) + save + destroy
     ——— 创建 1w 条 WAL 记录
  2. phase2 dir=dir_b: vector_api_create_with_interval(dir_b, 5000)
     2a. 把 dir_a 的 vector_wal_0.log 拷贝到 dir_b（或同 dir）
     2b. 调用 vector_api_load() （会触发 replay）
     2c. 在 Sleep(5ms) 后立即 vector_api_destroy ——— 强行撞 replay 中
  3. phase3: 重新 create + load，期望 size = 10000（完整 replay 续接）
  4. 最后 save + 再次 reload，期望 size 不变
依赖：vector_api_wal_replay 的 idempotent 路径（P1-B Task 6 已实现）
```

### 6.3 `ChaosE2ETest::PartialWriteRecovery`

```text
目的：手动破坏 WAL 末尾 16 字节，验证 replay CRC 容错
步骤：
  1. create_with_interval(dir, 5000) + create_collection + insert(500) + save
     ——— 触发 WAL checkpoint + truncate → WAL 留有头部 + 后续 records
  2. 打开 vector_wal_0.log（fopen wb+），fseek 至末尾 - 16，fwrite 0xFF×16
     ——— 模拟 partial write（断电 / OS crash 末尾 16 字节未对齐 record header）
  3. destroy + create + load
  4. 期望：size 在某次 insert 的整数倍附近，replay 中 CRC 失败被跳过
       不抛异常，无 corrupt，无负数
变体：ftruncate 末尾 32 / 64 字节（> 单 record header 17 + 数据 + CRC 4）
测试只 truncate 最后 16 字节（< 一个完整 record header）即可
```

### 6.4 `ChaosE2ETest::ShortIntervalStress`

```text
目的：100ms 周期 + 200 次 insert+kill 循环，验证零丢失
步骤：
  1. for round=0..199:
       a. api = vector_api_create_with_interval(dir, 100)
       b. create_collection（首次） / 不创建（后续轮）
       c. insert(1)（每次仅 1 条，让 checkpoint 与 insert 频繁交错）
       d. destroy(api)
       e. api = vector_api_create(dir)
       f. load(api)
       g. size = vector_api_size(api)
       h. EXPECT_EQ(size, round + 1)  // 每轮 +1
  2. 最终 save + 再次 reload
  3. 期望：size == 200，零丢失
注：因复用同名 collection，vector_api_size 累计
```

### 6.5 `ChaosE2ETest::ConcurrentSaveCrash`

```text
目的：4 写 + 1 save + 1 destroyer 200ms 周期，验证并发交织不撕裂
步骤：
  1. create_with_interval(dir, 5000) + create_collection
  2. 启动 3 类线程：
     - 4 writer 线程：每秒 insert(50)，持续 10s
     - 1 saver 线程：每 800ms vector_api_save
     - 1 destroyer 线程：每 200ms 检查 atomic<bool> destroy_now，触发 destroy+recreate
  3. 主线程：sleep 10s → atomic<bool> stop = true
  4. join all
  5. 最终 destroy + 重新 create + load
  6. 期望：size 一致（不超预期 / 不为负），不出现 corrupt
挑战：setter 路径的并发模型尚未支持，destroyer 用"全 destroy 后 recreate"代替
```

---

## 7. vdb_chaos 扩展 2-3 用例草案

> 文件：`engineering/test/db/integration/vdb_chaos_test.cpp`（追加，不改原 6 用例）

### 7.1 `ShortIntervalMultipleKillRecovery`

```text
目的：100ms × 5 轮 crash-recover，每轮 insert→kill→recover，验证全 round 数据保留
步骤：
  1. for round=0..4:
       a. api = vector_api_create_with_interval(dir, 100)
       b. create_collection（首次）/ skip（后续）
       c. insert(round + 1)  // 第 1 轮 1 条，第 2 轮 2 条...
       d. destroy
       e. api = vector_api_create(dir) + load
       f. size = vector_api_size(api)
       g. EXPECT_EQ(size, sum(1..round+1))  // 1+2+3+4+5 = 15
  2. 最终 save + 再 reload
  3. 期望：size == 15，零丢失
```

### 7.2 `ShortIntervalConcurrentCrashSave`

```text
目的：100ms interval + 4 写 + 1 save（200ms 周期）+ 周期 destroy
步骤：
  1. api = vector_api_create_with_interval(dir, 100)
  2. create_collection
  3. 启动线程：
     - 4 writer：每秒 insert(20)，20s
     - 1 saver：每 200ms save
     - 1 destroyer：每 500ms destroy+recreate（强制 reload）
  4. 主线程 sleep 20s → stop 信号
  5. join all
  6. 最终 destroy + create + load
  7. 期望：size 在下限（被 destroy 的部分丢失 ≤ 期望）
```

### 7.3 可选 `IntervalChangeStress`

```text
目的：运行时调 vector_api_set_checkpoint_interval()，验证 setter 原子性
步骤：
  1. create_with_interval(dir, 5000) + create_collection + insert(100)
  2. setter: vector_api_set_checkpoint_interval(api, 100)
  3. 立即 insert(100)（撞 setter 尚未完成 start 的窗口）
  4. destroy + reload
  5. 期望：size == 200，无 corrupt
挑战：setter 期间 insert 撞 stop→destroy→create→start 窗口
```

---

## 8. 风险与开放问题

| 风险 | 描述 | 缓解 |
|------|------|------|
| **短 interval IO 风暴** | 100ms 周期 + 大量 insert → flush+checkpoint+truncate 频繁 → CI 跑时间变长（vdb_stress 已 5/5 用 < 5s，新 chaos 5 用例可能用 10-30s） | 默认 5s 不变，仅 chaos 测试用 100ms；ChaosE2E 5 用例总时间控制在 60s 以内；CI 跑 chaos 套件设超时门槛 |
| **kill-during-checkpoint 竞态** | CheckpointManager 三步合一（flush/checkpoint/truncate），destroy 必须撞窗口，否则测试平凡通过 | 循环 10 次取最坏，记录每轮 size 留 audit log；100ms 周期下窗口比 5s 大 50 倍 |
| **partial write 误判** | 末尾 16 字节 truncate 可能落在有效 record 中间 → CRC 失败 → 这条 record 之后的 records 也全部丢失（因 `fseek` 跳到 `last_checkpoint`，后续是顺序读） | 测试只 truncate 最后 16 字节（< 一个 record header 17 + data + CRC 4），验证丢失 ≤ 1 条；变体 truncate 32/64 字节纳入矩阵 |
| **setter 线程安全** | setter 必须 stop→destroy→create→start 原子过渡 | VectorAPI_s 加 `pthread_mutex_t mgr_lock`；insert/save/search 不持锁 |
| **跨平台 mkdir 问题** | P1-B 教训：Windows `mkdir -p` 不支持 → warmup save `_mkdir` 创建目录（vdb_checkpoint_test.cpp:336-341 模式） | chaos_e2e_test.cpp 同样在 SetUp 用 `vector_api_save(warmup)` + `_mkdir` 双重保险；用 `_getpid` 唯一化 tmp 目录 |
| **mmdb_lock.h 无 plain mutex** | 仅暴露 mmdb_rwlock_t（rwlock 抽象），setter 需要 plain mutex | setter 内部用 `pthread_mutex_t`（跨平台，Windows 走 PTHREAD 兼容库已可用 — vector_api.c 已 `#include <pthread.h>`） |
| **Linux / Windows pthread 差异** | SetUpSuite 用 `_mkdir` vs `mkdir(0755)`，Sleep 用 Windows `Sleep(ms)` vs POSIX `usleep` | 沿用 vdb_chaos_test.cpp:24-34 既有 `#ifdef _WIN32` 模式；chaos_e2e 同样 |
| **chaos 触发确定性** | 概率性测试在 CI 不稳定时可能假阴性 | 把 "循环 N 次取最坏" 显式声明：循环 10 次、记录每次 size、EXPECT 各次均 ≥ 0，不强制 100% 通过破坏 |
| **`vector_api_create_with_interval` 0 语义** | interval_ms=0 是否禁用 mgr | 决策为"禁用后台 CheckpointManager"（仍可手动 save），chaos 测试规避此路径 |
| **预存 fixture 复用** | vdb_chaos 与 chaos_e2e test_data_dir 不共享避免相互干扰 | chaos_e2e_test.cpp 用独立目录 `./test-results/chaos_e2e` 与 `getpid()` |

### 8.1 开放问题清单

1. **`vector_api_set_checkpoint_interval` 是否要阻塞到旧 mgr 完全 stop？**
   - 当前设计：`pthread_join` 阻塞 → 必须完整等；
   - 异步变体：fire-and-forget，但需保证旧 mgr 句柄不被并发访问。
   - **建议**：同步阻塞，setter 简洁。

2. **`interval_ms < 100` 是否自动 WARN？**
   - 设计已写"< 100ms → 警告"但未实现；
   - chaos 5 用例无需更短；Task 4 可仅 WARN，不阻断。

3. **destroy 期间 setter 行为？**
   - destroy 入口不持 `mgr_lock`；
   - 若 setter 与 destroy 并发：setter 可能拿已 freed 的 mgr 指针 → UAF；
   - **建议**：destroy 入口持 `mgr_lock`，与 setter 串行化。

4. **`CheckpointManager::interval_timespec` 是否需可热改？**
   - 当前设计：必须 stop+create + start 重置；
   - 热改需要额外 mutex 保护 `interval_timespec` 与背景线程 `cond_timedwait` 中的 deadline 计算；
   - **建议**：保持 stop+create 模式，简单且易测试。

5. **chaos_e2e 是否需要整合到 ctest `all_tests`？**
   - vdb_chaos / vdb_checkpoint 已在 all_tests 内，chaos_e2e 需独立 target；
   - Task 3 末尾注册 `add_project_test(chaos_e2e_test VAR)`。

---

## 9. 实施清单（建议 Task 3-5 直接引用）

### Task 3

- [ ] 新建 `engineering/test/db/integration/chaos_e2e_test.cpp`
- [ ] fixture `ChaosE2ETest`（独立 test_data_dir=`./test-results/chaos_e2e`）
- [ ] 5 个 TEST_F 红测试，编译通过但运行失败（因 `vector_api_create_with_interval` 未实现）
- [ ] `engineering/test/db/integration/CMakeLists.txt` 注册 `chaos_e2e_test`

### Task 4

- [ ] `vector_api.h` 增加 `vector_api_create_with_interval` + `vector_api_set_checkpoint_interval` 声明
- [ ] `vector_api.c` 拆分 `vector_api_create` → 委托 `vector_api_create_with_interval(data_dir, 5000)`
- [ ] `VectorAPI_s` 加 `uint32_t checkpoint_interval_ms` + `pthread_mutex_t mgr_lock`
- [ ] `vector_api_create_with_interval` 内 `if (interval_ms > 0) checkpoint_manager_create(api->wal, interval_ms)`
- [ ] setter 实现：`lock(mgr_lock) → stop → destroy → create(interval_ms) → start → unlock`
- [ ] destroy 入口 `pthread_mutex_lock(&api->mgr_lock)` 避免 UAF

### Task 5

- [ ] chaos_e2e 5 用例 全绿
- [ ] vdb_chaos 追加 2-3 用例（`ShortIntervalMultipleKillRecovery` / `ShortIntervalConcurrentCrashSave`）
- [ ] 验证 vdb_checkpoint / vdb_wal_recovery / vdb_e2e / vdb_stress 全部回归不破

---

## 10. 关键代码契约（备忘）

1. **`checkpoint_manager_create(wal, interval_ms)`** 已是参数化（P1-B 完成），Task 4 不动
2. **`interval_ms == 0` 语义**：禁用后台 CheckpointManager，仅手动 save（Task 4 决策）
3. **destroy 不变式**：先 stop mgr → destroy mgr → close wal → free collection
4. **`mmdb_lock.h` 无 plain mutex**：setter 用 `pthread_mutex_t`（直采 OS 原语）
5. **`vector_api_create_with_interval` 委托模式**：旧 5 套件测试不需改，回归零成本
6. **`vector_api_save` 内部已含 checkpoint + truncate**（P1-B Task 6），chaos 路径覆盖完整
7. **WAL 文件路径**：`{data_dir}/vector_wal_0.log`，partial-write 测试按此路径找到文件
8. **partial write 16 字节**：`fseek(-16, SEEK_END)` + `fwrite(0xFF×16)` + destroy + create + load
9. **Windows mkdir 兼容**：沿用 vdb_checkpoint_test SetUp 模式 + `_getpid`
10. **chaos_e2e 与 vdb_chaos test_data_dir 隔离**：避免并发跑测试时 `remove_all` 误删

---

## 11. 关联清单

- P1-B 完成 `p1b-checkpoint-manager`（feature HEAD `03eb3f91e` 已合入 main）
- P1-C 当前 HEAD `8dcd1e1e9`（变更骨架已建）
- gap-analysis-roadmap Task 2.2.2（p1c-chaos-recovery 对应实现项）
- 下一阶段 P1-D：vector_wal_replay 重构（接受通用 record 回调），不在 P1-C 范围
- 下一阶段 P1-E：跨平台 mkdir 根治（独立 OPSX）

探查结束。等待 Task 3 实现。
