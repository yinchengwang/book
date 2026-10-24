# 构建债修复记录

> 记录阶段二之后 `engine/` 的构建债修复：根因、判据、验证方法。
> 修复以「串行构建」为唯一判据（并行下 ninja 的失败传播会让失败集合失真，
> 原因见 [`path-mapping.md`](path-mapping.md) §8.1）。

## 零、基线

修复前在 `3ec45cea6` 上做**全新**串行构建（`ninja -j1 -k 0`，冷缓存）：

```
FAILED 集合（10 个）：
  guc.exe                              raft_test.exe
  hnsw_recall_test.exe                 sift_recall_test.exe
  integration_test.exe                 test/db/core/engine_registry_test.exe
  test/db/security/security_test.exe   test_sharding.exe
  test_vector_index.exe                test_yang.exe
```

> 注：增量构建下 FAILED 集合会显著变小（只有 1 个）。因为某个库先失败后，
> 依赖它的目标被 ninja **跳过**（不计入 FAILED），冷缓存全量构建才能暴露全部问题。
> **验收必须用全新构建树 + 串行。**

按目标归类的缺失符号：

| FAILED 目标 | 缺失符号数 | 符号类别 |
|---|---|---|
| `guc.exe` | 13 | `*_engine_get_ops`（6）+ `mm_*`（5）+ `storage_*`（3） |
| `test_yang.exe` | 18 | `yang_data_*` / `yang_datastore_*` / `yang_model_*` / `yang_schema_*` |
| `test_sharding.exe` | 14 | `kv_*` |
| `raft_test.exe` | 14 | `kv_*` |
| `test/db/core/engine_registry_test.exe` | 1 | `spatial_engine_get_ops` |
| `sift_recall_test.exe` | — | `multiple definition`（17 个符号，见第五节） |
| `integration_test.exe` | 13 | 同 `guc.exe` |
| `test_vector_index.exe` / `security_test.exe` | — | 级联（上游库缺失） |

---

## 一、db_utils —— 空库被无条件链接

**根因**：`src/db/utils/` 目录**只有 `CMakeLists.txt`，没有任何 `.c` 源文件**。
其 `add_library(db_utils STATIC ${DB_UTILS_SOURCE_FILES})` 被
`if(DB_UTILS_SOURCE_FILES)` 包裹，条件永不成立 → **目标根本不存在**。

而三处无条件链接它：
- `src/db/distributed/shard/CMakeLists.txt`
- `test/db/index/integration/CMakeLists.txt`
- `test/db/index/sift_test/CMakeLists.txt`

**修法**：按仓库既有写法（`src/db/distributed/query/CMakeLists.txt`、
`src/db/protocol/CMakeLists.txt`）改用 `if(TARGET db_utils)` 保护。
不删库、不造源文件。

> 参照：`src/db/storage/sparse/CMakeLists.txt:26-27` 早已注释掉这行，
> 注释写明「db_utils 可能为空，改为可选链接」。

---

## 二、db_security —— 目录从未被引入

**根因**：`src/db/security/` 源文件齐全（`security_manager.c` 25 KB +
`security_executor.c` 4 KB），`add_library(db_security ...)` 也在，
但 `src/db/CMakeLists.txt` 的 `add_subdirectory` 列表（30+ 项）中**独缺 security**。

**修法**：补 `add_subdirectory(security)`。

---

## 三、index_hnsw_pq —— 库名写错

**根因**：实际库名是 `hnsw_pq`（`hnsw_pq/CMakeLists.txt:5` → `add_library(hnsw_pq ...)`），
`test/db/storage/CMakeLists.txt` 却写成 `index_hnsw_pq`。

**修法**：改为 `hnsw_pq`。

---

## 四、index_ivf_pq —— 不存在的库

**根因**：`ivf_pq/CMakeLists.txt` 只有两行注释、**没有 `add_library`**，
且 `ivf_pq/` 不在 `vector_index/CMakeLists.txt` 的 `add_subdirectory` 列表中。
`ivf_pq.c` 由父目录的 `file(GLOB_RECURSE VECTOR_INDEX_SOURCES ... "*.c")`
递归收集进了 `vector_index` 库。

**判据（已用 `nm` 验证）**：

```
$ nm -g --defined-only libvector_index.a | grep ivf_pq_create
00000000000009c4 T ivf_pq_create
00000000000012f9 T ivf_pq_destroy
```

且 `libvector_index.a` 已在 `test_vector_index` 的链接闭包内
（`ninja -t commands test_vector_index` 可见）。

**修法（方案 B）**：直接删除 `index_ivf_pq` 这行链接。
不另建库 —— 另建会造成同一份 `ivf_pq.c` 同时进两个库，反而制造重复符号。

---

## 五、量化器符号重复定义（multiple definition）

**根因**：`src/db/index/vector_index/` 下有 **23 个子目录各自建了独立静态库**，
但父目录的 `file(GLOB_RECURSE ...)` 仍把它们**重复收集**进 `vector_index` 库。
其中 4 个量化器目录（`pq` / `sq` / `rq` / `lvq`）的符号与 `algo-prod`
中的同名实现冲突：

```
$ nm 跨库比对 libvector_index.a vs libalgo-prod.a → 17 个重复符号
  lvq_encode, lvq_encode_batch,
  pq_adc_distance, pq_code_size, pq_compute_distance_table,
  pq_create, pq_destroy, pq_distance_table_size,
  pq_encode, pq_encode_batch, pq_train,
  rq_encode, rq_train,
  sq_code_size, sq_encode, sq_encode_batch, sq_train
```

**修法**：在 `vector_index/CMakeLists.txt` 的 GLOB 后排除这 4 个目录，
与既有的 `diskann/` / `faiss_hnsw/` / `gpu/` 排除规则一致。

> **正则必须锚定**：`^pq/.*` 而非 `.*pq/.*`。
> 后者会误伤 `hnsw_pq/`、`ivf_pq/`、`opq/` 等目录。
> GLOB 产出的是相对路径（如 `pq/pq.c`），故 `^` 可用。

**调用方已核对**，排除后符号仍可达：
- `diskann/` 用的是 `algo-prod` 版 `pq_*`（`diskann` 链 `algo-prod`）
- `hnsw_pq/` 用的是 `pq` 库版（`hnsw_pq` 链 `pq`）
- `hnsw_sq/` 用的是自己的 `static sq_encode`（不依赖 `sq` 库）

### 5.1 附带发现：integration_test 的两处源文件冲突

基线的 `multiple definition` 共 18 处、16 个唯一符号，除上述 7 个 `pq_*` 外，
还有 8 个 `diskann_*` 与 3 处 `main` —— 成因与量化器无关，是**测试目录内的文件**：

| 文件 | 问题 |
|---|---|
| `diskann_recall_simple.cpp` | 自带 `main()`，与 `integration_test.cpp` 的 `main()` 冲突 |
| `diskann_extended_stubs.c` | 扩展 API 的桩，与 `libdiskann.a` 内的 `diskann_stubs.c` 重复定义同一批符号 |

`add_project_test` 宏用 `file(GLOB *.cpp *.c)` 收集源文件，**没有排除能力**，
故这两个文件被一并收进 `integration_test`。

**已用 `nm` 核实**（对比两个 stub 的编译产物）：

```
库级 diskann_stubs.c     : 13 个符号
测试级 diskann_extended_stubs.c : 8 个符号
测试级有、库级没有的        : （空）
库级有、测试级没有的        : diskann_fresh_count / diskann_fresh_free /
                            diskann_fresh_search / diskann_merge_context_free /
                            diskann_spann_free
```

库级桩**完全覆盖**测试级桩且多 5 个符号 → 测试级桩纯属冗余。

**修法**：`integration_test/CMakeLists.txt` 改为显式 `add_executable` + `list(FILTER ...)`
排除这两个文件；`diskann_recall_simple.cpp` 另建独立 target（保留其测试价值）。
`add_project_test` 宏本身未改动（它被 30+ 个测试目录共用，改动风险过大）。

---

## 六、存储引擎符号缺失（*_engine_get_ops / mm_* / kv_* / yang_*）

**根因**：`engine_registry.c`（位于 `src/db/core/`，属 `db_core`）引用各存储引擎的
`*_engine_get_ops` 与 `mm_storage_init` / `storage_register_engine` 等符号，
但这些符号所在的 OBJECT 库**没有被正确收集进 `db_storage`**，或
`db_core` 本身没有链接到提供它们的库。

三类具体成因：

| 符号组 | 定义位置 | 成因 |
|---|---|---|
| `spatial_engine_get_ops` | `src/db/storage/spatial/spatial_engine.c:545` | `spatial` 未列入 `DB_STORAGE_SUBMODULES` 白名单（kv/graph/yang/vector 都在，唯独漏了 spatial） |
| `yang_datastore_*` / `yang_model_*` / `yang_schema_*` / `yang_data_*`（18 个） | `src/db/yang/`（`yang_parser` OBJECT 库） | 该 OBJECT 库的 obj **无人收集**；且 `src/db/yang` 在 `src/db/CMakeLists.txt` 中排在 `storage` 之后，`storage/CMakeLists.txt` 执行时目标尚不存在 |
| `kv_*`（`kv_open` / `kv_put` / `kv_get` / `kv_close` / `kv_scan` / `kv_iter_*` / `kv_stats` / `kv_delete` / `kv_flush`） | `src/db/storage/kv/kv.c`（`storage_kv` OBJECT 库） | 已收进 `db_storage`，但 `db_core` 未链接 `db_storage` → 链接器取不到 |
| `mm_*` / `storage_*` | `src/db/storage/mm/mm_storage.c`（`storage_mm`） | 同上 |

**修法**：

1. `src/db/storage/CMakeLists.txt` —— 把 `spatial` 加入 `DB_STORAGE_SUBMODULES` 白名单。
2. `src/db/CMakeLists.txt` —— 把 `add_subdirectory(yang)` **提前**到 `add_subdirectory(storage)` 之前
   （`yang_parser` 只依赖 `project_includes`，不依赖 storage，提前是安全的），
   使 `storage/CMakeLists.txt` 执行时 `yang_parser` 目标已存在。
3. `src/db/storage/CMakeLists.txt` —— 用 `target_sources(db_storage PRIVATE $<TARGET_OBJECTS:yang_parser>)`
   追加其 obj。
   > **注意**：不能用 `list(APPEND DB_STORAGE_OBJECTS $<TARGET_OBJECTS:yang_parser>)` ——
   > `add_library` 的**源文件位置不展开** `$<TARGET_OBJECTS:...>` 表达式
   > （该表达式仅在 `target_sources` 中有效）。已实测确认。
4. `src/db/CMakeLists.txt` —— 在 `add_subdirectory(storage)` **之后**声明
   `target_link_libraries(db_core PUBLIC db_storage)`。
   > **顺序要求**：`core` 目录先于 `storage` 被处理，那时 `db_storage` 目标尚不存在，
   > `if(TARGET db_storage)` 判定为假，链接不会生效。
5. `test/db/sharding/CMakeLists.txt` —— 给 `test_sharding` 显式加 `db_storage`
   （`db_sharding` 自身只链 `project_includes`，不会传递 `db_storage`）。

**关于循环依赖**：`db_storage` 链 `db_core`（PUBLIC），现在 `db_core` 也链
`db_storage`（PUBLIC），形成静态库循环。**CMake 支持这种情况**，
会在最终链接行中重复列出相关库；实测 configure 无任何告警。

### 已排除的「假缺失」

`doc` / `ts` / `stream` / `columnar` / `st` / `rdf` 六个模态的 `*_engine_get_ops`
在所有静态库中确实**没有定义**，但这**不是缺陷**：

```
$ cat build/generated/include/db/multimodal_config.h
/* #undef MMDB_ENABLE_TIMESERIES */
/* #undef MMDB_ENABLE_DOCUMENT  */
/* #undef MMDB_ENABLE_STREAM   */
/* #undef MMDB_ENABLE_COLUMNAR */
/* #undef MMDB_ENABLE_ST        */
/* #undef MMDB_ENABLE_RDF       */
```

这六个宏默认关闭（`MMDB_ENABLE_DOCUMENT`/`TIMESERIES` 的 option 描述里
明确写着 "not ready"），`engine_registry.c` 中对应的调用被 `#ifdef` 包裹，
**代码根本不参与编译**，故不需要处理。

---

## 七、faiss_hnsw_index_entry_point —— 声明有、实现无

**根因**：`faiss_hnsw.h:108` 声明了 `int32_t faiss_hnsw_index_entry_point(const faiss_hnsw_t *)`，
但 `faiss_hnsw/` 下 16 个 `.c` 文件均无实现。仅 `test/db/index/sift_test/hnsw_recall_test.c:222`
调用它。

**修法**：在 `faiss_hnsw_stubs.c` 中补齐实现。该文件已实现同类的
`faiss_hnsw_index_size` / `faiss_hnsw_index_ntotal` / `faiss_hnsw_index_max_level`，
实现模式完全一致（`faiss_hnsw_t` 是不透明类型，真实定义在
`faiss_hnsw_internal.h`，`entry_point` 字段直接可读）：

```c
int32_t faiss_hnsw_index_entry_point(const faiss_hnsw_t *index) {
    if (!index) return -1;
    return index->entry_point;
}
```

> 这不是 stub —— 它是 `max_level` 的同构实现，返回真实的 `entry_point` 字段
> （初始 -1，首个向量插入后为 0）。

---

## 八、验证方法

**判据：全新构建树 + 串行。**

```bash
rm -rf build/debt-fix
GIT_LFS_SKIP_SMUDGE=1 cmake -B build/debt-fix -S engine -DBUILD_TESTING=ON
cd build/debt-fix && GIT_LFS_SKIP_SMUDGE=1 ninja -j1 -k 0 2>&1 | grep "^FAILED"
GIT_LFS_SKIP_SMUDGE=1 ctest --test-dir build/debt-fix
```

### 定位符号归属的三个工具

| 目的 | 命令 |
|---|---|
| 某个目标实际链接了哪些库 | `ninja -t commands <target> \| tr ' ' '\n' \| grep '\.a$'` |
| 某个符号在哪些库被定义 | `nm -g --defined-only <lib>.a \| grep " T <sym>$"` |
| 某个库实际包含哪些 obj | `ninja -t deps <lib>.a`（或读 `build.ninja` 的 build 语句） |

> 注意 `ninja -t targets all` 里的 OBJECT 库**没有** `.a` 文件，
> 需看 `CMakeFiles/<target>.dir/*.obj`；用 `lib<target>.a` 查找会误判为「不存在」。

### 判据速查

| 现象 | 判据命令 | 含义 |
|---|---|---|
| 工作副本与 HEAD 一致 | `git diff-index --name-only HEAD` | 返回 0 即一致 |
| 索引 stat 缓存假阳性 | `git status --porcelain` 有 ` M` 但 `git diff --stat` 为空 | 先 `git update-index --refresh` |
| 符号是否真缺失 | `nm -g --defined-only <lib>.a` | 比对 `git log` 更可靠 |
| 库是否真被构建 | `ninja -t targets all` | OBJECT 库看 `.dir/*.obj` |
