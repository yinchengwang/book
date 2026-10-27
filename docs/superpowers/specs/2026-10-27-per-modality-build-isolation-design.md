# 按模态独立构建（方案 B）设计

> 状态：设计稿，待用户批准
> 日期：2026-10-27
> 范围：`StillEvolving/projects/engine`
> 所有"现状"数字均为本次实测，命令与口径见 §1.6

---

## 0. 一句话

把 engine 从「一个单体静态库 + 14 个近乎无效的编译开关」改成「每个模态一个独立静态库 + 单一 profile 入口」，
使得 `-DMMDB_MODALITY=kv` 只编译 kv 及其真实依赖，并用每模态一个冒烟可执行文件机械地证明「这个模态单独可用」。

---

## 1. 现状取证

### 1.1 开关只覆盖了 13.6% 的编译单元

| 指标 | 实测 |
|---|---|
| `src/db` 下 `.c` 总数 | **487** |
| 其中位于可开关的模态目录（`storage/{kv,vector,graph,doc,ts,spatial,yang,stream,columnar,st,rdf,sparse}`） | **66（13.6%）** |
| 无条件编译（`src/db/CMakeLists.txt` 里没有 `if(MMDB_ENABLE_*)` 包裹） | **421（86.4%）** |

`MMDB_ENABLE_*` 在源码里的实际使用次数：`RELATIONAL` 7、`KV` 16、其余模态各 **2** 次
（即 `src/db/core/engine_registry.c` 里一个 `extern` 声明 + 一个注册块）。
**宏控制的是"要不要 `add_subdirectory` 这个目录"，不是"要不要编译这段代码"。**

`MMDB_ENABLE_DISTRIBUTED` 在源码中 **0 次**使用。

### 1.2 最大的单块未被守卫：向量索引

| 项 | 值 |
|---|---|
| `src/db/index/vector_index` | **101 个 `.c` / 37,800 行** |
| 守卫 | **无**（`src/db/index/CMakeLists.txt` 里 `add_subdirectory(vector_index)` 无条件） |

即 `-DMMDB_ENABLE_VECTOR=OFF` 时这 101 个文件照编。

### 1.3 构建产物被测试可执行文件主导

| 项 | 值 |
|---|---|
| 测试可执行文件 | **165 个 / 2.2 GB** |
| 静态库 | 101 个 / 149 MB |
| 构建树总量 | 2.8 GB |

每个测试 exe 都通过 `add_project_test(... db_storage ...)` 静态链接全套库，
因此 165 份几乎相同的镜像。

### 1.4 依赖环：3 个文件让每个模态都依赖全部模态

`src/db/core/` 里有 3 个文件越界引用了具体引擎的私有头：

| 文件 | 行数 | 越界引用 |
|---|---|---|
| `core/func_registry.c` | 405 | `storage/doc/doc_vector.h`、`storage/ts/ts_sql_functions.h`、`graph/graph_sql_functions.h`、`storage/spatial/spatial_geo.h`、`storage/yang/yang_tree.h` |
| `core/vector_query.c` | 1631 | `index/vector_index/{hnsw/faiss_hnsw,ivf/IndexIVF,reranker/reranker,BM25/bm25}.h` |
| `core/vector_exec.c` | 925 | `db/sql/sql_executor.h` |

由于所有模态都依赖 `core`，而 `core` 又依赖全部引擎，**传递闭包退化成全集**：

| 模态 | 当前闭包（模块数） |
|---|---|
| kv / vector / spatial / yang / doc / ts / st | **16** |
| graph | **17** |

**摘掉这 3 个文件的出边后重算**（这是方案 B 的核心收益，已用脚本模拟验证）：

| 模态 | 当前 | 摘除后 | 消失的模块 |
|---|---|---|---|
| kv | 16 | **1** | 其余全部 |
| yang | 16 | **2** | 其余全部 |
| graph | 17 | **4** | 其余全部 |
| spatial | 16 | **4** | 其余全部 |
| st | 16 | **5** | 其余全部 |
| doc | 16 | **6** | 其余全部 |
| vector | 16 | **7** | 其余全部 |
| ts | 16 | **8** | 其余全部 |

### 1.5 其他既有事实（影响范围判断）

1. **`doc`（7,038 行）和 `ts`（5,603 行）根本没被 `add_subdirectory`。**
   `src/db/storage/CMakeLists.txt:85-93` 只打印 "code exists but has build issues, not enabling"。
   这两个模态当前不参与任何构建，也从未通过编译。
2. **`db_core` ↔ `db_storage` 循环 PUBLIC 链接**（`src/db/CMakeLists.txt:46-48` 与
   `storage/CMakeLists.txt:233`）。CMake 允许，但会破坏库边界，是拆库的直接障碍。
3. **`storage/CMakeLists.txt:248-258` 的 else 分支**用 `add_library(db_storage STATIC IMPORTED)`
   指向自己的产物 `libdb_storage.a`，会静默取上一次构建的陈旧库。
4. **`index/` 下 162 个 `.c`**，其中 `vector_index` 101 个；`vector_index` 内部：
   `faiss_hnsw` 11、`diskann` 17、`ivf` 7、`BM25` 8、`delete` 6、`gpu` 6、`streaming` 5、`reranker` 5，
   另有约 21 个单文件 ANN 实现（annoy/scann/lsh/nsw/ssg/bq/opq/rq/lvq/kd_tree/ball_tree/spectral_hash/itq/…）。

### 1.6 取证口径

```bash
# 编译单元计数
find src/db -name "*.c" | wc -l                                    # 487
find src/db/storage/{kv,vector,graph,doc,ts,spatial,yang,stream,columnar,st,rdf,sparse} -name "*.c" | wc -l   # 66
# 依赖闭包：scripts/dep_audit.py（本次新增，见 §7 交付物）
# 产物：build/verify-debt 下 find -name "*.exe" / "*.a" + du
```

---

## 2. 目标

1. **G1** 给定 `-DMMDB_MODALITY=<name>`，编译单元数只包含该模态自身 + 它真实需要的公共代码。
2. **G2** 每个模态产出一个冒烟可执行文件 `mm_<modality>_smoke`，只链接该模态的库，
   链接成功 + 运行通过 = 「单独可用」的机械证明。
3. **G3** 全模态构建（`MMDB_MODALITY=full`）的功能与今天等价，测试失败集合不超出基线。
4. **G4** 公共代码只有一份。任何为了裁剪而复制代码的做法都被禁止。
5. **G5** 构建时间与产物可量化对比，给出前后数字。

## 3. 非目标与反目标

**非目标**
- 不重构任何模态的内部实现（不碰算法、不改数据结构）。
- 不改公开 API 语义（`storage_ops_t`、`vdb_api.h`、`mm_storage.h` 的函数签名不变）。
- 不做运行时插件（`.so`/`.dll` 动态加载）。宏隔离天然为它铺路，但本轮不做。
- 不修复 doc/ts 的功能缺陷，只把它们**拉进构建**（见 §6 开放决策 D3）。

**反目标（做到这些就算失败）**
- **复制公共代码到各模态**以换取"只编自己的"——修一个 bug 要改 N 处。
- **宏组合爆炸**：保留 14 个可任意组合的布尔量而不加约束，产生
  `VECTOR=OFF, SPARSE=ON` 这类非法组合。
- **用 `#ifdef` 包裹每个调用点**：给 400+ 文件加条件编译，可读性和可维护性一起崩。

---

## 4. 方案选择

| 方案 | 内容 | 结论 |
|---|---|---|
| **A** 只补守卫 + 按目录拆库 | 把无条件构建的子系统挂到需要它的模态下，拆 `db_storage` | 只能把单模态从 431 降到约 150；**没有机制证明独立性**，且不解决 core 反向依赖 |
| **B** A + 公共层收敛 + 每模态独立 target | 在 A 基础上抽出最小公共层、去环、单入口 profile、每模态冒烟 | **已选**。唯一同时满足 G1/G2/G4 的方案 |
| **C** 每模态一个独立 CMake 工程 | 各自顶层工程 + main，共享代码以 INTERFACE 引入 | 构建入口变 N 个、CI 矩阵翻倍、公共代码改一行验证 N 处，与 G4 冲突 |

选 B 的理由：A 缺 G2（没有东西能证明"单独可用"），C 把复用扔掉。B 的
`mm_<modality>_smoke` 是唯一可自动验证的独立性证据；`MMDB_MODALITY` 单入口消灭非法组合。

---

## 5. 设计

### 5.1 四层，依赖只许向下

```
L4  服务层      api / protocol / netconf / cli / ecosystem / tools
L3  查询层      parser / optimizer / rewrite / validator / executor      （按 RELATIONAL / GRAPH 分片）
L2  模态引擎    kv | vector | graph | doc | ts | spatial | yang | sparse | stream | columnar | st | rdf
L1  存储设施    buffer / page / lock / catalog / wal / rel / blob / access / mm
L0  公共基础    mm_base：errors / log / memctx / rwlock / platform_compat / utils
```

**铁律**：只允许 `L(n) → L(n-1)`。同层之间只允许 §5.4 白名单里的边。

### 5.2 去环：注册表反转（本设计的核心动作）

把「公共层知道所有引擎」改成「引擎向公共层注册自己」。

1. `storage_ops_t`、`DataModel`、`storage_register_engine` / `storage_get_engine`
   全部下沉到 **L1 的 `mm_storage_api`**，该库不 include 任何引擎头。
2. 每个引擎导出一个注册函数：`int mm_<modality>_register(void)`，
   内部调用 `storage_register_engine(MODEL_X, &xxx_ops)`。
3. 新增**构建期生成文件** `mm_builtin_engines.c`：由 `configure_file` 按当前启用的模态生成
   函数 `mm_register_builtin_engines(void)`，函数体是若干个 `mm_<modality>_register()` 调用。
   关掉某个模态，这段代码根本不存在，也不需要 `#ifdef`。
4. **生成的注册文件在每次构建中都存在，不使用弱符号。**
   `mm_storage_api` 只声明 `void mm_register_builtin_engines(void);`，定义由生成文件提供。
   即使某个 profile 一个模态都没开，生成文件也产出一个空的函数体。
   于是：
   - `mm_storage_init()` 继续调用 `mm_register_builtin_engines()`，**公开 API 语义不变**（满足非目标）；
   - 生成的 TU 是**按 profile 定制的**：kv profile 里它只调 `mm_kv_register()`，
     所以依赖图是 `mm_storage_api → mm_builtin_engines(kv) → mm_kv`，**没有跨模态边**；
   - 不需要 `__attribute__((weak))`，因此 MSVC 上同样成立（本仓对 MSVC 加了 `/W4 /WX`，
     弱符号本来也不是可移植方案）。
5. `core/func_registry.c` 同样拆成 `mm_<modality>_register_sql_functions()`，由同一个生成文件调用。
   `core` 不再 include 任何引擎私有头。
6. **两个越界文件的归属（已核实，与初稿不同）：**
   - `core/vector_query.c`（1631 行）→ **搬到 `storage/vector/`**。它 include 了
     `index/vector_index/{hnsw,ivf,reranker,BM25}`，是向量专属的查询执行。
   - `core/vector_exec.c`（925 行）→ **搬到 `vectorized/`，不是向量模态**。
     它导出的是 `vector_block_*` / `vector_batch_*` 这类通用向量化原语，
     `executor/{framework,operators,parallel}` 下 **10+ 个文件**在用（filter/hashagg/hashjoin/
     project/seqscan/exchange/…）。把它塞进 `mm_vector` 会造成 relational → vector 的反向依赖。

**验收这条设计成立的判据**：
```bash
grep -rn 'storage/\(kv\|vector\|graph\|doc\|ts\|spatial\|yang\)/\|index/vector_index\|db/sql/' src/db/core/
# 必须为空
```

### 5.3 单一入口 `MMDB_MODALITY`

新增 `cmake/ModalityProfiles.cmake`，每个 profile 显式列出它打开的模态集合，
并在配置期做**闭包校验**：profile 声明的模态集合必须对其依赖闭包封闭，
否则 `message(FATAL_ERROR)`。

```cmake
mmdb_profile(kv         MODALITIES kv)
mmdb_profile(vector     MODALITIES vector)
mmdb_profile(graph      MODALITIES graph kv)          # graph 依赖 kv
mmdb_profile(relational MODALITIES relational)
mmdb_profile(full       MODALITIES relational kv graph vector spatial tree sparse
                                  doc ts stream columnar st rdf)                    # 全部 12 个模态
mmdb_profile(legacy     MODALITIES relational kv graph vector spatial tree sparse)  # = 今天能构建的集合
```

- `-DMMDB_MODALITY=vector` 是推荐入口，自动推导 14 个 `MMDB_ENABLE_*`。
- **`full` = 全部 12 个模态**（用户裁决 D3）。`legacy` 保留为"今天能构建的集合"，
  **只用于回归基线比对**——doc/ts/stream/columnar/st/rdf 从未参与构建，没有历史基线可比。
- 细粒度 `-DMMDB_ENABLE_*` 保留为高级用法，但一旦检测到手动设置就 `FATAL_ERROR`
  并提示「请改用 `-DMMDB_MODALITY`，或显式加 `-DMMDB_ALLOW_MANUAL_SWITCHES=ON` 承担后果」。
  手动模式跳过闭包校验并打印警告。
- **缓存粘性**：CMake cache 会让上次的 `-D` 粘住，因此 profile 写入一个内部变量
  `MMDB_PROFILE_APPLIED=<name>`；若它与本次 `MMDB_MODALITY` 不一致，直接报错要求换构建目录。
  （换 profile 本来就该用新构建目录，文档里写明。）

### 5.4 同层白名单（显式，短）

| 边 | 理由 |
|---|---|
| `graph → kv` | 图引擎复用 KV 的存储原语（`storage/graph → storage/kv`，已存在） |
| `st → spatial` | 时空复用空间几何类型 |
| `vector → index/vector_index` | 向量引擎需要索引实现 |
| `spatial → index`（框架） | 空间索引选择器 |
| `doc → index/vector_index`（仅 core） | 文档向量检索 |

任何**新增**的同层边必须在 `docs/arch/db/modality-build.md` 登记并说明理由，否则评审不通过。

### 5.5 库粒度

每个模态一个静态库，只含自己目录的源文件：

```
L0  mm_base
L1  mm_storage_api  mm_buffer  mm_page  mm_lock  mm_catalog  mm_wal  mm_rel  mm_blob  mm_access  mm_vectorized
L2  mm_kv  mm_vector  mm_graph  mm_doc  mm_ts  mm_spatial  mm_yang  mm_sparse  mm_stream  mm_columnar  mm_st  mm_rdf
L3  mm_relational   （sql + parser/sql + optimizer + rewrite + validator + executor + rel + access）
    mm_executor_rag （从 db_executor 拆出，VECTOR 守卫；现在它 include 了 vector_engine.h，
                     是把 relational 拖向 vector 的唯一原因）
```

`vector_index` **整体作为一个库 `mm_vector_index`，不再细分**（用户裁决 D1）。

理由：所有 ANN 索引（faiss_hnsw / ivf / diskann / nsw / annoy / scann / lsh / ssg / …）
都是**向量模态独有的代码**，构建 vector 时把它们全部编进来，符合"只编自己的 + 公共的"这个目标。
把 ANN 拆成可选子库是我初稿多加的一层，它带来的不是隔离而是"用户拿不到某些索引"，已删除。

附带记录（供后续按需优化，不进本轮）：
- `vector_index_selector.c` 对 HNSW/IVF-PQ **只做代价估算**（`estimate_*` 静态函数），
  没有对索引实现的硬符号引用。
- `tiered_index.c`、`streaming/streaming_index.c`、`delete/vector_index_delete_api.c` 硬引用 `diskann_*`；
  `access/am_hnsw.c`、`access/vector_am.c`、`core/vector_query.c` 硬引用 `nsw_*`。
- `annoy`、`scann`、`lsh`、`ssg`、`spectral_hash`、`itq` 在自身目录外零调用者。
  → 若将来要裁，这 6 个是唯一能安全去掉的。

### 5.6 每模态冒烟目标

```cmake
mm_smoke(kv)       # 只链 mm_base + mm_storage_api + mm_builtin_engines + mm_kv + 它声明的 L1 子集
mm_smoke(vector)   # 只链 … + mm_vector + mm_vector_index
...
```

每个 `mm_<modality>_smoke.c` 只做：初始化 → 建一个模型 → 写一条 → 读回 → 断言 → 关闭。
它存在的唯一意义是**链接期证明**：如果这个模态偷偷依赖了别的模态，链接会失败。

### 5.7 测试归属

现有 165 个测试 exe 每个都链 `db_storage`（全套）。改为：
- 每个测试目录在 CMake 里声明所属模态（`set(MM_TEST_MODALITY vector)`）；
- `add_project_test` 只链接该模态的库 + 公共层；
- 跨模态集成测试单独归 `full` profile，只在全量构建时启用。

预期收益：单模态构建的 exe 数量与体积按模态数下降，而不是恒定 165 个 / 2.2 GB。

### 5.8 L4 在 profile 中的归属

| profile | 产物 |
|---|---|
| 单模态（`kv` / `vector` / `graph` / …） | 静态库 + `mm_<modality>_smoke`。**不出 server / CLI** |
| `relational` | 静态库 + smoke + `db_cli`（SQL shell）+ `db_server` |
| `full` | 全部 L4：api / protocol / netconf / cli / ecosystem / tools |

「单模态可用」的定义就是：**库能链、冒烟能跑**，不包括服务端。服务端是 L4 的事，与模态无关。

### 5.9 预期数字

计数口径：**L0 + L1 子集 + 该模态闭包内全部 `.c` + 生成文件 1 个；不含测试。**

| 构建 | 现在 | 目标（估算） |
|---|---|---|
| `kv` | ≈431 | **45–60** |
| `graph` | ≈431 | **60–80** |
| `vector`（含全部 ANN） | ≈440 | **150–170** |
| `relational` | ≈460 | **110–130** |
| `legacy`（今天能构建的集合） | 459 | ≈459（不退化） |
| `full`（12 个模态） | 无法构建 | **487** |

`vector` 的目标看起来没降多少，因为它独占 `vector_index` 的 101 个文件——
而这 101 个文件确实全是向量独有代码（用户裁决 D1），本来就应该编。
它省掉的是 `sql`(36) + `executor`(28) + `parser`(9) + `tools`(16) + `distributed`(14) + `protocol`(4) 等。

**这些数字是估算，不作为验收判据。** 真正的门禁在 Task 0：改造前先为每个 profile
量一次「改造后应该达到的编译单元数」，实测值写入 `docs/arch/db/modality-build.md`，
后续以实测值为准（门禁 = 实测值 × 1.2）。

### 5.10 把 5 个从未构建的模态拉进来（D3）

`doc`、`ts`、`stream`、`columnar`、`st`、`rdf` 当前**全部没有 `add_subdirectory`**。
本轮要让 `full` 含全部 12 个模态，必须先让它们编得过。

**已实测**（`gcc -std=c11 -fsyntax-only`，逐个文件，见下表）：

| 模态 | 编译单元 | 错误数 | 根因（已归类） |
|---|---|---|---|
| `doc` | 11 | **2** | `doc_nested.c` 用 `node->value.num_children`，但该 union 无此成员 |
| `ts` | 11 | **22** | `TsColumnarCompressor.raw_capacity` / `DeltaEncoder.capacity` / `BitPackingEncoder.capacity` 头与实现漂移；`CdcConsumer` 结构体在两头重复定义 |
| `stream` | 1 | **0** | 无 |
| `columnar` | 1 | **2** | `columnar_serialize_table` 静态/非静态声明冲突 |
| `st` | 1 | **10** | `mkdir` 两参调用、`bbox_t` / `point_t` 重复 typedef、`st_engine_aggregate_time` 签名不一致 |
| `rdf` | 3 | **23** | 用了 `WIN32_FIND_DATAW` / `HANDLE` / `cFileName` 但未 include `<windows.h>`；`mkdir` 未声明 |
| **合计** | **28** | **59** | **约 14 个根因，集中在 5 个文件** |

结论：这不是"重写"级别的活，是**头文件与实现漂移 + Windows 可移植性**的修修补补。
`st` 与 `rdf` 的错误几乎全是 Windows 特有（`mkdir` 两参、`WIN32_FIND_DATAW`），
说明它们在 Linux 上大概率本来就编得过——**修的时候要两边都验证**。

顺序：`doc` → `columnar` → `ts` → `st` → `rdf`（按错误数从少到多，`stream` 已 0 错误可直接接）。
每个模态修完后立刻接入 `mm_<modality>_smoke`，避免"编过了但跑不起来"。

---

## 6. 开放决策

### 6.1 已由设计者裁决（builder 可直接按此执行，不必再问）

| # | 问题 | 裁决 |
|---|---|---|
| D2 | 是否保留 14 个细粒度 `MMDB_ENABLE_*` | 保留，但必须显式加 `-DMMDB_ALLOW_MANUAL_SWITCHES=ON`，否则 `FATAL_ERROR` |
| D4 | 165 个测试 exe 是否本轮重排 | **是**。否则"减少编译产物"这个目标落空——产物 2.2 GB 里 96% 是这 165 个 exe |
| D5 | `db_core ↔ db_storage` 循环依赖 | **是**，作为独立第一步提交，可单独回滚 |
| D6 | `vector_exec.c` 归属 | `vectorized/`（通用向量化原语，10+ executor 文件在用），**不是**向量模态 |
| D7 | `vector_query.c` 归属 | `storage/vector/`（向量专属） |
| D8 | `executor/rag` 归属 | 从 `db_executor` 拆成 `mm_executor_rag`，**VECTOR 守卫**。它是 relational 被拖向 vector 的唯一路径 |
| D9 | 注册表如何避免 L1→L3 反向边 | 生成文件在每次构建中都存在（按 profile 定制），**不用弱符号**——MSVC 无可移植的弱函数符号（见 §5.2 第 4 条） |
| D10 | 裁剪 ANN 会不会让选择器链接失败 | 不会（选择器只做代价估算）。但**本轮不裁**，见 D1 |
| D11 | 换 profile 时 cache 粘住 | `MMDB_PROFILE_APPLIED` 内部变量比对，不一致直接报错 |
| **D1** | `vector_index` 带哪些 ANN | **全部带**（用户裁决）。所有 ANN 都是向量独有代码，构建 vector 就该全编；不引入 `MMDB_VECTOR_INDEXES` 子开关 |
| **D3** | 未构建的 5 个模态是否本轮纳入 | **全部纳入**（用户裁决）。`full` = 12 个模态；实测仅 59 个错误 / 约 14 个根因，见 §5.10 |

### 6.2 仍需用户拍板

无。D1/D3 已裁决，其余 12 项由设计者裁决，builder 可直接按 §6.1 执行。

---

## 7. 交付物

1. `scripts/dep_audit.py`——模块级 include 依赖审计（本次已写成原型，需正式化）。
2. `cmake/ModalityProfiles.cmake`——profile 定义 + 闭包校验。
3. `cmake/gen_builtin_engines.cmake` + `mm_builtin_engines.c.in`——注册表生成。
4. 每模态 `CMakeLists.txt` 的库定义与依赖声明。
5. 每模态 `mm_<modality>_smoke.c` + 冒烟 target。
6. `docs/arch/db/modality-build.md`——依赖白名单、新增边登记处、**实测基线数字**。
7. `scripts/measure_build.sh`——统计给定 profile 的编译单元数与产物体积（可重复）。
8. **基线快照** `docs/arch/db/build-baseline-2026-10-27.md`——改造前的 ctest 失败用例名集合
   （不是失败数）。仓库里没有现成基线，必须先采。

## 8. 验收判据

```bash
# 0) 采基线（改造前，必须做）
#    legacy = 今天能构建的集合（relational kv graph vector spatial tree sparse）
cmake -B build/base -S engine -G Ninja -DMMDB_MODALITY=legacy -DBUILD_TESTING=ON
ninja -C build/base -k 0 && ctest --test-dir build/base -j8 --output-junit base.xml
# 记录「失败用例名集合」到 docs/arch/db/build-baseline-2026-10-27.md
# 跑两次取交集，flaky 用例单独标注

# 1) 单模态编译单元数（门禁值由 Task 0 实测确定，见 §5.9）
cmake -B build/kv -S engine -G Ninja -DMMDB_MODALITY=kv
./scripts/measure_build.sh build/kv        # 打印编译单元数 / .a 总量 / exe 数量

# 2) 独立性证明
ninja -C build/kv mm_kv_smoke && ./build/kv/mm_kv_smoke

# 3) 去环
grep -rn 'storage/\(kv\|vector\|graph\|doc\|ts\|spatial\|yang\)/\|index/vector_index\|db/sql/' src/db/core/   # 空

# 4) 全量不退化
cmake -B build/full -S engine -G Ninja -DMMDB_MODALITY=full -DBUILD_TESTING=ON
ninja -C build/full -k 0 && ctest --test-dir build/full -j8 --output-junit full.xml
# 失败用例名集合 ⊆ 基线集合；新增失败必须逐条归因
```

**注意**：该仓 ctest 失败数在完全相同前提下会在 61–68 之间波动（memctx UAF 误报、NSW 超时、
`test_spatial_engine` 并行 SEGFAULT），**绝对值不可比**，只能比较失败用例名集合。
基线本身也要跑两次取交集，把 flaky 用例单独标注，否则"新增失败"会被噪声淹没。

---

## 9. 风险

| 风险 | 影响 | 应对 |
|---|---|---|
| 拆库后大面积链接期缺符号 | 高 | 这正是价值所在——它把隐性依赖显式化。按模态逐个拆，每个模态拆完立刻跑冒烟 + 该模态测试 |
| `db_core ↔ db_storage` 循环打断会连带大量 include 改动 | 中 | 先做 D5，单独一个提交，可独立回滚 |
| `func_registry.c` 拆分改变 SQL 函数注册时序 | 中 | 保留原注册顺序；补一个"所有内置函数可见"的回归测试 |
| 生成文件必须随每次构建产出，否则 `mm_storage_init` 链接失败 | 中 | 生成文件是 `add_library(mm_builtin_engines ...)` 的固定源，**不放在可选分支里**；空 profile 也生成空函数体。**注意：本仓对 MSVC 加了 `/W4 /WX`，新增告警会变错误** |
| 5 个未构建模态拉进构建（D3） | 中 | **已实测**：59 个错误 / 约 14 个根因，集中在 5 个文件（§5.10）。不是重写级工作量 |
| `st` / `rdf` 的报错几乎全是 Windows 特有（`mkdir` 两参、`WIN32_FIND_DATAW`、`HANDLE`） | 中 | 修的时候 Linux 与 Windows **两边都要验**——Linux 上可能本来就编得过，只改 Windows 分支容易在 Linux 上引入回归 |
| 测试重排后某些测试静默不跑 | 中 | 对比重排前后 ctest 用例总数，必须相等 |
| 165 个 exe 重排会一次性改动大量 CMakeLists | 中 | 按目录分批，每批跑一次 ctest 用例数比对 |

---

## 10. 留给 builder 决定的

- 拆库的**实施顺序**（建议：去环 → 公共层 → 逐模态拆 → 测试重排）。
- `mm_base` 的具体成员边界（哪些 `.c` 进 L0，哪些留 L1）。
- 冒烟测试的具体断言内容。
- `measure_build.sh` 的实现方式。

---

## 11. Builder check 记录（2026-10-27）

派了一个未参与讨论的 subagent 以"要照此排计划的人"的身份评审本文档，提出 9 个问题。
处理结果：

| # | 问题 | 处置 |
|---|---|---|
| 1 | 生成的注册函数由谁调用？会不会重建循环 | **已裁决 D9**：生成文件每次构建都产出、按 profile 定制，`mm_storage_init` 语义不变；不用弱符号（MSVC 不可移植） |
| 2 | `vector_exec.c` 不是向量专属，搬进 `mm_vector` 会造成 L2→L3 反向边 | **已核实并改正 D6**。初稿此处是错的，已改为 `vectorized/` |
| 3 | 裁 ANN 后选择器会不会链接失败 | **已核实 D10**：选择器只做代价估算，无硬符号引用。但用户裁决 D1 = 全部 ANN 都带，故本轮不裁 |
| 4 | 闭包与编译单元的计数口径 | **已明确**：L0 + L1 子集 + 模态闭包 + 生成文件，不含测试（§5.9） |
| 5 | relational 的真实闭包 | **已裁决 D8**：`executor/rag` 拆出并 VECTOR 守卫，否则 relational 被拖向 vector |
| 6 | `full` 含 doc/ts 与 G3 冲突 | **已解决**：用户裁决 D3 = 全部纳入。`full` 改为 12 模态，另设 `legacy` profile 承载"今天能构建的集合"用于回归比对 |
| 7 | 单模态构建还产不产服务端 | **已明确**：不产（§5.8） |
| 8 | 如何区分用户手动设了 `MMDB_ENABLE_*` | **已裁决 D11**：`MMDB_PROFILE_APPLIED` 比对 + 显式开关 `MMDB_ALLOW_MANUAL_SWITCHES` |
| 9 | 基线失败集从哪来 | **已补**：基线采集列为交付物 #8 + §8 步骤 0 |
