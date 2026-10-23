# 移除 engineering/apps 设计文档

> 日期：2026-10-23
> 状态：待批准
> 决策：方案 C（直接删除 + 移除仅服务于 apps 的接口）

---

## 1. 背景与结论修正

原计划（`2026-10-22-miniprogram-native-rewrite-master-plan.md`）定的是「`engineering/apps/` 整目录删除」。
2026-10-22 执行时发现该目录含活跃 C++ 构建，收窄为只删 3 个已迁移 Web 源。
现在用户要求重新评估能否整目录删除，并选定**方案 C：直接删除 + 移除仅服务于 apps 的接口**。

### 调查过程中的一次自我修正（重要）

初次评估时我判断「删除会砍掉核心存储能力」，理由是
`src/db/storage/vector/vector_engine.c` 在持久化路径上调用了 5 个 `hnsw_persist_*` 符号，
而 `hnsw_persist.c` 位于 `apps/multimodal_rag/`。

**深入核查后该判断过重，实际风险低得多：**

| 事实 | 证据 |
|---|---|
| 向量引擎**真正的**持久化路径是自包含的 | `vector_engine_save_index()`（L1162）直接用 `fwrite` 写 HNSW，不走 `hnsw_persist_*` |
| 通用 `vector_index_save/load/is_valid` 只是对 `hnsw_persist` 的薄封装 | L2587–2710 |
| 该通用 API 的**唯一生产消费者就是 `apps/multimodal_rag` 自己** | `apps/multimodal_rag/src/index/db_vector_index.cpp:224,269` |
| 其余调用点均在不参与构建的测试文件里 | `test/db/storage/vector_index_persist_test.cpp`（未登记 CMake） |

结论：`hnsw_persist` 及其 4 个包装函数是**为 multimodal_rag 服务的**，不是引擎自身依赖。
删除它们不会削弱引擎的持久化能力 —— 引擎用自己的 `vector_engine_save_index/load_index`。

---

## 2. 目标 / 非目标 / 反目标

**目标**
- `engineering/apps/` 目录从仓库中彻底消失
- CMake 配置与全量构建通过
- 全量测试通过，且 target 数量变化可解释

**非目标**
- 不重写向量持久化能力（引擎既有路径不动）
- 不迁移 `multimodal_rag` 到 `src/`（用户选择砍掉而非搬迁）
- 不清理 `apps/` 之外的历史文档引用（历史计划按约定保持原样）

**反目标（即使"技术通过"也算失败）**
- 引擎持久化能力被削弱（`vector_engine_save_index` 必须仍然可用）
- 出现"构建通过但 target 静默消失"却未被记录的情况
- 遗留指向已删路径的有效引用

---

## 3. 约束

- 必须在删除前后各记录一次 target 清单，做差集核对（上次的教训：「不报错」≠「无影响」）
- `vector_engine.c` 用 `extern` 声明而非 include 头文件调用 `hnsw_persist_*`，因此删除时不会因头文件缺失而暴露，**必须手工核对全部调用点**
- 删除 API 而非 stub：让任何遗漏的调用点在**编译期**暴露，而非运行时静默失败

---

## 4. 方案对比

| 方案 | 做法 | 代价 | 结论 |
|---|---|---|---|
| A 只删孤儿 | 删 api-server / db_driver / workbench_server / web（约 1.9MB） | 零风险，但 apps 仍存在 | 未选 |
| B 搬迁依赖 | 把 `hnsw_persist.c/.h` 搬进 `src/`，保留全部功能 | 搬 2 文件 + 改 1 行 CMake | 未选 |
| **C 直接删 + 移除接口** | 删整个 apps，同时删除只服务于它的 `vector_index_save/load/is_valid/get_meta` | 删 4 个函数 + 4 个测试 + 两处 CMake 入口 | **已选定** |

选 C 的理由：通用 persist API 的唯一消费者就是待删的 apps 本身；引擎自有持久化路径不受影响。
因此 C 在功能上等价于 B，但省掉一次搬迁，且能顺带清掉一个已无消费者的接口。

---

## 5. 实施步骤

### S1 · 断开 core 对 apps 的依赖

**`src/db/storage/vector/CMakeLists.txt`**
- 从 `VECTOR_PERSIST_SOURCES` 移除 `${ENGINEERING_SOURCE_DIR}/apps/multimodal_rag/src/index/hnsw_persist.c`
- 从 `target_include_directories(... PUBLIC ...)` 移除 `${ENGINEERING_SOURCE_DIR}/apps/multimodal_rag/include`

**`src/db/storage/vector/vector_engine.c`**
- 删除 L187–206 区域的 `extern` 声明（5 个 `hnsw_persist_*`）与 `hnsw_persist_result_t` / `hnsw_persist_meta_t` 结构体 typedef
- 删除以下 4 个函数（唯一消费者随 apps 消失）：
  - `vector_index_save()`（L2587 起）
  - `vector_index_load()`（L2639 起）
  - `vector_index_is_valid()`（约 L2694）
  - `vector_index_get_meta()`（约 L2703）

**头文件**
- `include/db/vector_engine.h`：删除 `vector_index_save` / `vector_index_load` / `vector_index_is_valid` 声明（L195 / L205 / L213）
- `include/db/index/vector_index/persist/vector_index_persist.h`：删除 `vector_index_is_valid` 声明（L303）及其「定义在 vector_engine.c 中」的注释

> **注意**：`vector_index_save_hnsw` / `_load_hnsw` / `_save_diskann` / `_save_ivf` 等**后缀版本**位于
> `src/db/storage/vector/vector_index_persist.c`，是独立实现（直接用 `faiss_hnsw`），**不依赖 apps，全部保留**。

### S2 · 删除测试

- 删除整个 `test/apps/` 目录（5 个 `.cpp` + `CMakeLists.txt`），连带移除 4 个构建目标：
  `test_common_terminal_test`、`test_common_menu_test`、`games_g2048_core_test`、`games_snake_core_test`
- `test/CMakeLists.txt:18` 删除 `add_subdirectory(apps)`
- 删除 `test/db/storage/vector_index_persist_test.cpp`（测的是被删 API；虽未登记 CMake，留着是死代码）
- `test/db/storage/vector_persist_test.cpp`：**保留**。其 `VectorIndexPersistTest` 测的是
  header checksum / version 兼容，不依赖 `hnsw_persist` 实现；仅需更新 L250 的过时注释

### S3 · 删除 apps 目录

- `git rm -r engineering/apps`
- `engineering/CMakeLists.txt`：
  - 删除 L153–154 `if(EXISTS ... apps/CMakeLists.txt) add_subdirectory(apps) endif()`
  - 更新 L2 注释中的 `apps/`

### S4 · 验证（硬性）

```bash
# 删除前：记录基线
cmake --build <build> --target help > /tmp/targets-before.txt
cmake -S engineering -B <build>          # 期望 Configuring done
cmake --build <build> -j4                # 期望构建成功

# 删除后：同样记录并做差集
cmake --build <build> --target help > /tmp/targets-after.txt
diff <(sort /tmp/targets-before.txt) <(sort /tmp/targets-after.txt)   # 逐条解释差异
```

- 期望消失：48 个 apps 相关 target（`apps_*` / `games_*` / `vdb_cli` / `todo-app` / `multimodal` / `test_common_*`）
- 期望保留：`vector_persist`、`vector_engine_*`、其余全部 db/algo/sdk target
- 全量测试：`ctest` 或既有测试入口，确认无新增失败

### S5 · 文档同步

| 文件 | 动作 |
|---|---|
| `AGENTS.md` | 删除「Knowledge Hub API Server」整节（api-server 已随 apps 删除） |
| `engineering/README.md` | 删除 `apps/` 行与相关描述 |
| 根 `CLAUDE.md` L380 | 「新独立项目 → `engineering/apps/`」改为「engineering 下直接建目录」 |
| `engineering/apps/games/README.md` | 随目录删除 |
| 总纲 / 执行计划 | 追加第二轮「执行结果回填」：apps 已整目录删除 |
| `docs/architecture/{knowledge-hub,reading-radar}` | 已有迁移说明，补充「apps 目录已删除」 |

---

## 6. 风险与回滚

| 风险 | 等级 | 缓解 |
|---|---|---|
| 遗漏 `hnsw_persist_*` 调用点 → 链接失败 | 低 | 已全仓核查：仅 5 处，全在 `vector_engine.c`；且删除后编译期即暴露 |
| 外部（仓库外）消费者调用被删 API | 低 | 本仓无外部消费者；如未来需要，可从 git 历史恢复 |
| target 静默消失未被发现 | 中 | S4 强制做 target 差集核对 |
| 误删 `vector_index_save_hnsw` 等独立实现 | 中 | 实施时逐一核对：只删无后缀的通用版，后缀版全保留 |

**回滚**：单一 commit，`git revert` 即可完整恢复（apps 全部在 git 历史中，无需额外归档）。

---

## 7. 遗留说明

- 删除后 `vector_index_save/load/is_valid/get_meta` 这 4 个通用 API 不再存在。
  若将来需要「按通用接口持久化 HNSW」，可复用已有的 `vector_index_save_hnsw/load_hnsw`
  （`src/db/storage/vector/vector_index_persist.c`，不依赖 apps），当前无调用方。
- `multimodal_rag` 的向量检索能力随目录删除一并消失；若日后需要，可从 git 历史恢复。
