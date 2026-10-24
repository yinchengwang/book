# `book` 仓库重构系统设计（方案 C：两步走 + reference 独立仓）

> 作者：高见远（Gao）· 架构师>   
> 日期：2026-10-23>   
> 状态：设计稿，待用户拍板 §5 的 9 个决策点>   
> 约束：本文档为**只读探查产出**，未对仓库执行任何写操作

---

## 0. 探查结论：5 条与任务简报不符的事实（**必须先读**）

简报里的部分数字来自 `git ls-files` 的**默认输出**，而该输出在 `core.quotePath=true`（Git 默认）下会把**非 ASCII 路径做 C 风格转义并加引号**。这导致了一个连锁误判。逐条纠正：

| #      | 简报说法                                                  | 实际（已验证）                                                                                                                                                                                                                                                                                       | 影响                                                                                                         |
| ------ | ----------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------- |
| **F1** | 121 个 `"learning/` **幽灵路径**，磁盘不存在，需 `git rm --cached` | **121 个文件全部真实存在于磁盘**，0 个缺失。它们是 `learning/notes/`、`learning/blog/` 下的**中文文件名**（如 `学习/阿里-排序算法.md`），被 git 转义成 `\344\270\255...` 才显示为引号条目。解析 `git ls-files -z` 或 `git -c core.quotePath=false ls-files` 后，真实 `learning/` 条目为 **2158** 条，无一条以引号开头                                                  | 🔴 **`git rm --cached` 会删掉 121 篇中文笔记**，且这 121 个文件磁盘上存在，`--cached` 后会变成untracked，改动有实质数据损失风险。**阶段一必须撤销这一项** |
| **F2** | 6172 个受跟踪文件                                           | **5730** 个。差值 442 正是 brief 未排除的 submodule gitlink 计数方式差异                                                                                                                                                                                                                                      | 轻微，影响体积估算                                                                                                  |
| **F3** | `reference/` = 10.5 万文件 / 400 MB                      | **536,317 文件 / 10.4 GB**（工作树实测）。其中 `reference/relational` 单独就 121,363 文件 / 2.6 GB                                                                                                                                                                                                             | 🔴 切出 `reference/` 的收益比预估高 **25 倍**，这个阶段的价值远大于brief 认知                                                     |
| **F4** | 仓库 541 MB git 对象                                      | **`.git` = 26 GB**。其中 `.git/modules/reference` = **25 GB**（99 个 submodule 各自的独立 .git），主仓 `.git/objects` 仅 1013 MB（其中 `.git/lfs` 673 MB）。                                                                                                                                                      | 🔴 切出 reference 可**立即回收 25 GB 磁盘**，这是本次重构最大的实际收益                                                           |
| **F5** | `engine/apps/` 432 文件、9 个应用                      | **`engine/apps/` 已于 4 天前被删**（`907301fa9 refactor(apps): 删除 engine/apps 整目录与 CMake 入口`）。`build-games-full.sh` 同批删除（`762165ed0`）。当前 `engine/` 二级为 `src 754 / include 497 / test 429 / tools 53 / docs 43 / sdk 23 / tests 18 / scripts 13 / third_part 10 / drivers 9 / cmake 4` | 目标树里不能有 `apps/` 从 engineering 搬运的分支；`web/` 的"生死决策"变成了纯删除决策                                                 |

**额外发现（简报未提，但影响阶段一）**：

- **F6｜`dataset/` 是 692 MB，不是 40 个小文件。** `dataset/sift/` 单独 645 MB。且 `.gitattributes` 为 `dataset/**/*.bin|fvecs|ivecs` 配了 LFS 规则，但**实测文件头是真实二进制而非 LFS pointer**（`dataset/sift/base.bin` 首字节非 `version https://git-lfs`）→ **LFS 规则形同虚设，692 MB 明文二进制直接进了 git 历史**。这与 F4 的 `.git/lfs` 673 MB 相互印证。

  > ⚠️ **2026-10-25 事后更正：本条结论错误。** LFS **一直正常工作**。实测
  > `git cat-file -p HEAD:dataset/sift/base.bin` 返回的是 134 字节的 LFS 指针
  > （`version https://git-lfs.github.com/spec/v1` + `oid sha256:158e7033...` +
  > `size 327271936`）。`git ls-tree -r -l HEAD -- dataset` 求和仅 **13.6 MB**，
  > 不是 692 MB。当时的判断错在拿**磁盘工作副本**（LFS 已 smudge 出的实体）当成了
  > git 中的内容。详见 §附录 C。

- **F7｜双轨守卫有覆盖盲区。** `cmake/dual-track-guard.cmake:12` 硬编码 `_current_dir STREQUAL "learning" OR "engineering"`，且 `file(READ)` 只读**当前这一个文件**。所以它只检查 `learning/CMakeLists.txt` 和 `engine/CMakeLists.txt` 两个文件。子目录里的跨轨 include（`engine/src/algo-prod/CMakeLists.txt:16`、`engine/src/db/api/CMakeLists.txt:7` 都写了 `${ENGINEERING_SOURCE_DIR}/../learning/include`）**从未被守卫覆盖**。→ 简报"只有两处双轨耦合"的结论不准确，实际有 **4 处**。
- **F8｜`uthash` 有三份拷贝，且只有一份是活的。** `learning/include/uthash/`（git 跟踪的 6 个头文件，**唯一有效来源**）、`engine/third_part/uthash/`（submodule，`src/` 存在）、`vendor/third_part/uthash/`（submodule，**空目录**）。而 `engine/src/db/storage/graph/CMakeLists.txt:19` 引用了**不存在的** `${CMAKE_SOURCE_DIR}/third_part/uthash/src` —— 这条 include 一直是空挂的，编译能过是因为 `db/api` 的 `find_path(... HINTS ../learning/include PATH_SUFFIXES uthash)` 恰好命中了 `learning/include/uthash/`。

---

## 0.5 活跃度驱动的分区判断（2026-10-23 增补，**优先级高于 §1 原方案**）

### 0.5.0 数据复核

我独立复核了 team-lead 提供的活跃度数据，**结论完全一致**（修正计数方法：`git rev-list --count`，而非我第一次误用的 `--name-only` 行数）：

| 目录                    | 近90天 commit | 受跟踪文件      | 文件/活跃度比 | 最后改动        |
| --------------------- | ----------- | ---------- | ------- | ----------- |
| `engine/`        | **480**     | 1860       | 3.9     | 活跃          |
| `docs/`               | **282**     | 414        | 1.5     | **极活跃**     |
| `openspec/`           | 88          | 282        | 3.2     | 活跃          |
| `vendor/third_part/`         | 9           | 121        | 13.4    | 静止          |
| `reference/`          | 13          | 102(+536k) | —       | 静止          |
| `scripts/`            | 7           | 7          | 1.0     | 活跃          |
| `learning/`           | **5**       | 2158       | **432** | **冻结**      |
| `archive/`            | 5           | 35         | 7       | 静止          |
| `web/`                | 10          | 35         | 3.5     | 9/21 后冻结    |
| `dataset/`            | 2           | 40         | 20      | 静止          |
| `apps/` | 1           | 325        | 325     | 10-22 一次性重写 |

**`learning/` 的 432:1 是全仓最极端的失配** —— 比 `reference/`（13/102）更悬殊，比 `web/`（3.5）差两个数量级。

但**活跃度不能单独决定分区**。我进一步查了三条关键约束，结论与"直接切成第二个独立仓"的直觉**相反**。

---

### 0.5.1 问题一：`learning/` 是否该像 `reference/` 一样切出去？

#### 结论：**可以切，且应该切 —— 但必须先解掉一个硬阻塞**

#### 阻塞项：`learning/include/` 是 `engine/` 的活跃构建依赖

`learning/` 近 90 天只有 5 次改动，乍看是纯静态素材。但 `engine/` 的 CMake 里有**两处 include 路径指向它**（§0F7）：

```cmake
# engine/src/algo-prod/CMakeLists.txt:16
${ENGINEERING_SOURCE_DIR}/../learning/include
# engine/src/db/api/CMakeLists.txt:7
find_path(UTHASH_INCLUDE_DIR uthash.h HINTS ${ENGINEERING_SOURCE_DIR}/../learning/include PATH_SUFFIXES uthash)
```

我把 `engine/` 源码里所有对 `learning/` 头文件的引用全查了一遍：

| 被引用的头                    | `engine/include/` 有？ | `learning/include/` 有？ | 引用者                              | 是否参与构建            |
| ------------------------ | ------------------------- | ---------------------- | -------------------------------- | ----------------- |
| `ds/trie.h`              | ✅ 有                       | ✅ 有                    | 多个                               | ✅ 是               |
| `uthash/uthash.h`        | ❌ 无                       | ✅ 有                    | `db/api` 等                       | ✅ **是（关键路径）**     |
| `ds/distributed_index.h` | ❌ 无                       | ✅ 有                    | `algo-prod/distributed/impl/*.c` | ❌ **已被 CMake 排除** |
| `ds/persistent_ds.h`     | ❌ 无                       | ✅ 有                    | 同上                               | ❌ **已被 CMake 排除** |

**两个"看起来只存在于 learning"的头文件，恰好都是死代码**：`engine/src/algo-prod/CMakeLists.txt:8` 明确写着

```cmake
# S6 排除学习性内容：distributed/impl/（依赖缺失的头文件 ds/distributed_index.h）
list(FILTER ALGO_PROD_SOURCE_FILE EXCLUDE REGEX "^distributed/impl/")
```

`engine/test/algo/ds/consistent_hash_gtest.cpp` 虽 include 了 `persistent_ds.h`，但**未被任何 CMakeLists 纳入**。

**所以唯一的真实阻塞只剩 `uthash`。** 而它有解 —— 我实测：

```
engine/third_part/uthash/src/uthash.h   ✅ 存在（submodule v2.4.0）
learning/include/uthash/uthash.h             ✅ 存在（vendored v2.3.0）
diff → 仅 8 行差异，全是版权年与版本号声明：
  Copyright 2003-2025 → 2003-2026
  #define UTHASH_VERSION 2.3.0 → 2.4.0
```

其余 5 个头文件 diff：`utarray.h` 15 行、`utlist.h` 152 行、`utringbuffer.h` 8 行、`utstack.h` 8 行、`utstring.h` 8 行（同样以版本年差异为主）。

**`engine/third_part/uthash/` 这个 submodule 本来就在 `.gitmodules` 里、CI 里 `git submodule update --init` 也已经拉它了** —— 它一直在那里，只是 CMake 的 `find_path` 因 `HINTS` 顺序把 `learning/include` 排在了前面。

#### 因此，切出 `learning/` 的前置动作收敛为 3 步（详见 §2阶段四）

| 步 | 动作                                                                                                                                                                                              | 验证                                                                                                                        |
| - | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| 1 | `engine/src/db/api/CMakeLists.txt:7` 的 `HINTS` 改指 `${ENGINEERING_SOURCE_DIR}/third_part/uthash/src`                                                                                             | `cmake -B build/engineering -S engineering` 后 `grep UTHASH_INCLUDE_DIR build/engineering/CMakeCache.txt` 应指向 submodule 路径 |
| 2 | `engine/src/algo-prod/CMakeLists.txt:16` 的 `${ENGINEERING_SOURCE_DIR}/../learning/include` **直接删除**（该目录对本库无任何有效头文件）                                                                             | 构建通过                                                                                                                      |
| 3 | `engine/src/db/storage/graph/CMakeLists.txt:19` 与 `engine/test/db/graph/CMakeLists.txt:25` 引用的**不存在路径** `../third_part/uthash/src` **改为真实路径** `${ENGINEERING_SOURCE_DIR}/third_part/uthash/src` | 构建通过                                                                                                                      |

执行这 3 步后，`engine/` 对 `learning/` 的依赖**归零**，`learning/` 变成真正可独立切出的静态仓。

#### 切出的收益（对比 `reference/`）

| 维度                    | `learning/` 切出后               | 说明                                                                      |
| --------------------- | ----------------------------- | ----------------------------------------------------------------------- |
| 主仓受跟踪文件               | 5730 → **3572**（-38%）         |                                                                         |
| 主仓工作树                 | ~11 GB → ~10.4 GB             | 省的是 `reference/` 那 10.4 GB，与 learning 的 27 MB 无关                        |
| `.git` 体积             | 26 GB → ~1.1 GB               | 主要是 `reference/` 的功劳                                                    |
| **`git status` 扫描速度** | 显著改善                          | 2158 个文件的 stat 从每次操作中移除                                                 |
| **双轨守卫可彻底删除**         | ✅                             | 守卫存在的唯一理由是防跨轨引用；依赖归零后 `cmake/dual-track-guard.cmake` 可整个删掉，省掉 B3b 的改造成本 |
| **CI 简化**             | ✅                             | CI 现有 2 个 job（Build/Test Learning）可整体删除                                 |
| 新增成本                  | 需处理 833 commits 的历史边界 + 新仓初始化 | 唯一代价                                                                    |

#### 风险与推荐

| 方案                                         | 评价                                                                                                                                                    |
| ------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| **推荐：阶段四切出 `learning/` → `book-learning`** | `learning/` 与 `engine/` 的活动节律相差 86 倍（5 vs 480）、文件数 2158 占全仓 33% 却近乎冻结、依赖已可解零。**它和 `reference/` 是同一类资产 —— 静态素材**，只是 `reference/` 更大（536k 文件）所以更显眼 |
| 备选：留在主仓仅做目录重排（我§1 原方案）                     | 可行但次优：留下一个 2158 文件、90 天动 5 次的分区，`git status` 每次都要 stat 它，且双轨守卫要继续维护、根 `CMakeLists.txt` 要继续保留 `LEARNING_BUILD` 分支                                      |

**我的建议：采纳"切出"，但排在阶段四（reference 之后）**，因为它比 reference 简单得多 —— 不涉及 submodule 重挂，只是 2158 个普通文件的仓库边界变更。

---

### 0.5.2 问题二：`docs/` 414 文件该怎么组织？

#### 结论：**保持 `docs/` 为独立顶层主干，不拆散** —— 且我原方案的 R1/R2 规则需要**推翻**

我按二级目录实测了 `docs/` 内部的活跃度，结果**完全颠覆**了我 §1.3 的假设：

| `docs/` 子目录                                                         | 近90天 commit  | 文件数 | 活跃度    | 原方案归属                 | 修正后归属                       |
| ------------------------------------------------------------------- | ------------ | --- | ------ | --------------------- | --------------------------- |
| **`db-wiki/`**                                                      | **161**      | 75  | 🔥🔥🔥 | `notes/wiki/`（R1）     | **`docs/wiki/db/`** ← 推翻    |
| `superpowers/`                                                      | 50           | 104 | 🔥🔥   | `archive/`（R2）        | **`docs/process/`** ← 推翻    |
| **`rag-wiki/`**                                                     | **26**       | 13  | 🔥🔥   | `notes/wiki/`（R1）     | **`docs/wiki/rag/`** ← 推翻   |
| **`docs/*.md`（顶层散）**                                                | **274**（去重前） | 23  | 🔥     | 多数→`engine/docs/`（R6） | **`docs/`** ← 部分推翻          |
| `gap-analysis/`                                                     | 19           | 15  | 🔥     | `docs/analysis/`（R5）  | `docs/analysis/` ✅          |
| **`agent-wiki/`**                                                   | **17**       | 15  | 🔥     | `notes/wiki/`（R1）     | **`docs/wiki/agent/`** ← 推翻 |
| `architecture/`                                                     | 6            | 36  | ❄️     | `engine/docs/`（R4）    | `docs/arch/` ← 推翻           |
| `diagrams/`                                                         | 4            | 89  | ❄️     | `engine/docs/`（R3）    | **`docs/diagrams/`** ← 推翻   |
| `rag/`                                                              | 3            | 1   | ❄️     | `engine/docs/`（R4）    | `docs/` ← 推翻                |
| `multimodal-gap-analysis-2026/`                                     | 3            | 11  | ❄️     | `docs/analysis/`（R5）  | `docs/analysis/` ✅          |
| `db/`                                                               | 3            | 2   | ❄️     | `engine/docs/`（R4）    | `docs/` ← 推翻                |
| `index/`                                                            | 2            | 17  | ❄️     | `engine/docs/`（R4）    | `docs/arch/` ← 推翻           |
| 其余（usage/sql-executor/openspec/learning/api/deployment/modular-rag） | 各 1-2        | 13  | ❄️     | `engine/docs/`（R4）    | 按内容归属                       |

**两个决定性发现**：

**① `db-wiki` 是全仓当前最热的文档资产，不是归档材料。**

```
docs/db-wiki 161 次 commit 的月份分布：
  2026-10: 161   ← 全部集中在最近 3 周
```

最近 5 次提交（全部 2026-10-22）：

```
71fe0a2bf docs: README 分档改用通用正则重新判档（🔬48/🔷19/◻️1），门禁同步
37eb471c2 docs: 修正 README 深化分档（couchdb/ravendb 含行号引用应为🔬）并新增 --mode-readme 一致性校验
5fe35ef8a docs: db-wiki README 深化列改为三档质量标记
b24a054e6 docs: db-wiki README 深化列改为三档标记（按引用可验证程度）
f1ff62213 docs: velox.md 补齐模式 B approx 标注（通过引用门禁）
```

它有**专职治理工具** `scripts/check-wiki-refs.py`（WD-29 首创，WD-31~33 加强），做源码行号级引用校验，`utarray.h` 式的"引用错误分类"还在演进。我实测其引用强度：

```
docs/db-wiki 中带行号的源码引用总数：3437 条
引用最密集：vector/faiss.md 302 条、vector/qdrant.md 216 条、vector/vald.md 202 条
```

**把它当成"外部项目 wiki 归档"塞进 `notes/`（我的 R1 规则）是错的** —— 它是正在建设、且有工具门禁的质量资产。

**② `docs/` 与 `reference/` 是强耦合的姊妹资产，切出 reference 会打断门禁。**

`db-wiki` 的 12 个模型分类与 `reference/` 的 14 个 submodule 分类**几乎一一对应**：

```
reference/  : ai benchmark columnar distributed-sql document embedded extension graph key-value relational search stream time-series vector
db-wiki/    : columnar distributed-sql document embedded extension graph key-value relational search stream time-series vector  (+README)
```

`check-wiki-refs.py` 的 `--submodules` 参数直接把 wiki 引用前缀映射到 `reference/` 真实路径。**`db-wiki` 是 `reference/` 源码的"阅读笔记层"**。

这直接影响阶段三的决策：**切出 `reference/` 时，`docs/db-wiki` 必须一起走**，否则：

- 3437 条行号引用失去被引对象
- `check-wiki-refs.py` 模式 A 完全失效
- 虽有模式 B 兜底，但 `MODE_B_WIKIS` 清单只列了 **10 个文件**（boltdb/duckdb/lmdb/pklite/arrow/datafusion/hnswlib/lance/pgvector/velox），其余 59 个 db-wiki 文件无兜底


**③ `architecture/` 与 `diagrams/` 是最冷的部分（6 和 4 次/ 90 天），它们才是真正该下沉到 `engine/` 的。**

`diagrams/` 89 个文件、4 次改动，且全是 `level0-project`→`level6-rag` 的源码架构图 —— 描述 `engine/src/db/**`，与代码同生命周期。放顶层确实错位。

#### 修正后的 `docs/` 组织

```
docs/                              ← 独立顶层主干，不拆散
├── README.md                      ← 导航（新建）
├── wiki/                          ← R1 修正：外部项目 wiki（跟随 reference 切出，见阶段三 C8')
│   ├── db/          ← 原 docs/db-wiki      75 文件  161 commit  🔥
│   ├── rag/         ← 原 docs/rag-wiki     13 文件   26 commit  🔥
│   └── agent/       ← 原 docs/agent-wiki   15 文件   17 commit  🔥
├── analysis/                       ← R5 保持：跨轨研究
│   ├── gap-analysis/               15 文件   19 commit  🔥
│   └── multimodal-gap-analysis-2026/ 11 文件    3 commit
├── process/                        ← R2 修正：AI 协作过程产物（50 commit，仍在用）
│   ├── plans/                      68 文件
│   └── specs/                      36 文件
├── arch/                           ← R4 修正：本仓架构总览（冷，但仍是"文档"）
│   ├── database/                   ← 原 docs/arch/db  （01-05 视图等）
│   ├── applications/               ← 原 docs/arch/{algo,db-driver,kbase}
│   └── index/                ← 原 docs/arch/
├── diagrams/                       ← R3 修正：**保留在 docs/**（4 commit/89 文件，但它是跨轨分层图 L0-L6）
├── storage/                        ← 原 docs 顶层散：storage-* / spatial-* / timeseries-* / vector-* / graph-* / doc-*（19 文件）
└── *.md（跨轨报告）                 ← multimodal-* / *-report.md / project-metrics.json（4 文件）
```

**与 §1.3 原方案的差异汇总**：

| 规则 | 原方案                                              | 修正后                               | 依据                                                                        |
| -- | ------------------------------------------------ | --------------------------------- | ------------------------------------------------------------------------- |
| R1 | `docs/{db,agent,rag}-wiki/` → `notes/wiki/`      | → **`docs/wiki/{db,rag,agent}/`** | db-wiki 161 commit/10月全月，是最热资产；且与 reference 强耦合，应随 reference 走而非随 notes 走 |
| R2 | `docs/process/` → `archive/`                 | → **`docs/process/`**             | 50 commit，仍活跃；且它是 db-wiki 门禁的工作记录                                         |
| R3 | `docs/diagrams/` → `engine/docs/`                | → **保留 `docs/diagrams/`**         | L0-L6 是跨轨分层视图（项目→分布式→存储→向量→Redis→RAG），不是单一模块架构图                           |
| R4 | `docs/{architecture,index,...}` → `engine/docs/` | → **`docs/arch/`**                | 6 commit/36 文件，是最冷的部分，但仍是"文档"而非"代码附属"                                     |
| R6 | 19 个 `storage-*`/`vector-*` 等 → `engine/docs/`   | → **`docs/storage/`**             | 这些是存储引擎设计文档，与 `db-wiki` 的外部项目文档同性质（同为"知识文档"）                              |

**结论：`docs/` 保持为独立顶层，不拆散进 `engine/`。** 理由是活跃度实测：`docs/` 282 commit 紧跟 `engine/` 480，是全仓第二主干；而 `engine/docs`（现有）只有 7 commit。把 `docs/` 拆进 `engine/` 等于把第二主干降级为第三主干的附属。

**唯一例外**：`openspec/`（1 文件）是 `openspec/` 的镜像，可直接消除。

---

### 0.5.3 问题三：`web/` 与其归档的处置

#### 结论：`web/` 直接删；归档 tar.gz **建议一并删除**，无需迁移到 `archive/`

#### `web/` 判定（补强 §1 的 D3）

| 证据                                                 | 结论     |
| -------------------------------------------------- | ------ |
| 近 90 天 10 commit，**全部集中在 2026-09-21 之前，之后归零**      | 已废弃    |
| 不被任何 `CMakeLists.txt` / `.cmake` 引用（§0 已 grep 确认）  | 构建无关   |
| `AGENTS.md:77-90` 明确声明前端已迁移到 `apps/` | 定位已被取代 |
| `engine/apps/web/`（Taro 源）已于 `907301fa9` 删除   | 上游已断   |
| 35 文件，功能与 `apps/`（3 个原生小程序）重复                      | 冗余     |

→ **`git rm -r web`，不需要任何兼容处理。**

#### 归档 tar.gz 判定

实测 `.archive/engineering-apps-web-migrated-srcs-20261022.tar.gz`：

| 项    | 值                                                                                   |
| ---- | ----------------------------------------------------------------------------------- |
| 大小   | 4,183,260 字节（4.1 MB）                                                                |
| 加入时间 | `4c22b8136`（2026-10-22）                                                             |
| 内容   | 497 条目，全部在 `engine/apps/web/{games-mini-program,knowledge_hub,reading-radar}/` |
| 内容性质 | **旧 Taro/React 源码**，对应的是**已被删除的** `engine/apps/web/`                           |
| 冗余度  | 497 条目中仅 7 条是垃圾（`.swc` 插件二进制、`playwright-report`），490 条是源码                          |

**三个理由支持删除而非迁移**：

1. **git 历史已完整保存它**。`engine/apps/web/` 的全部内容在 `4c22b8136` 之前的每一个 commit 里都能取回。tar.gz 只是把 git 历史**又抄了一遍** —— 这是对版本控制的无意义重复。
2. **它的形态是反模式**。二进制 tar.gz 进 git 后：① 无法 diff、无法 review；② 未来若有变更只能整包替换；③ `.gitattributes` 的 LFS 规则不覆盖 `.tar.gz` 之外的路径匹配（实际规则里有 `dataset/**/*.tar.gz`，但 `.archive/*.tar.gz` 不匹配），所以它是明文入库。
3. **它与 §0F6 的问题同源**。`dataset/` 692 MB 明文二进制 + 这个 4.1 MB tar.gz，说明"用二进制文件做归档"是本仓的既有习惯问题 —— 重构时不应再复制这个习惯。

**唯一保留理由**：若担心将来想参考旧 Taro 实现。但这个需求已被 `apps/` 的原生重写版本覆盖（`4c22b8136` 的 commit message 就是"三应用原生化"），旧实现的技术价值接近零。

#### 处置建议

| 动作                 | 命令                                                                              | 说明                        |
| ------------------ | ------------------------------------------------------------------------------- | ------------------------- |
| 1. 删 `web/`        | `git rm -r web`                                                                 | 阶段一 A5                    |
| 2. **删归档 tar.gz**  | `git rm .archive/engineering-apps-web-migrated-srcs-20261022.tar.gz`            | 新增，替代原 B6 的"迁移到 archive/" |
| 3. 删空的 `.archive/` | `rmdir .archive`                                                                | 唯一文件已删                    |
| **需要保留时的替代方案**     | 若用户仍想留：`git rm --cached` + `.gitignore` 加 `.archive/*.tar.gz`，同时把文件移到**仓外**本地备份 | 二进制不入库，但内容不丢              |

**建议：D3 扩展为「删 `web/` + 删归档 tar.gz + 删 `.archive/`」**，见 §5 的 D3'。

---

### 0.5.4 活跃度驱动的最终分区表（**取代 §1 原方案**）

把活跃度结论与原方案合并后的最终形态：

| 顶层                        | 内容                                                      | 近90天 commit | 文件数         | 与原方案差异                              |
| ------------------------- | ------------------------------------------------------- | ----------- | ----------- | ----------------------------------- |
| `engine/`                 | `engine/` 改名                                       | **480**     | 1742        | 不变                                  |
| **`docs/`**               | 保持独立顶层，内部按 §0.5.2 重整                                    | **282**     | **414（全留）** | 🔴 **推翻"拆散进 engine/notes/archive"** |
| `openspec/`               | 不变                                                      | 88          | 282         | 不变                                  |
| `apps/`                   | `apps/`                                   | 1           | 325         | 不变                                  |
| `vendor/`                 | `vendor/third_part/` + `cmake/`                                | 9           | 124         | **不含 `dataset/`**（D2）               |
| **`book-learning`（独立仓）**  | `learning/` 2158 文件切出                                   | **5**       | 0（主仓）       | 🔴 **从"分区"升级为"独立仓"（阶段四）**           |
| **`book-reference`（独立仓）** | `reference/` 98 submodule                               | **13**      | 102（主仓留元数据） | 不变（阶段三）                             |
| **`docs/wiki/` 随迁**       | db-wiki/rag-wiki/agent-wiki **103 文件跟随 book-reference** | 204         | 0（主仓）       | 🔴 **新增：wiki 与其被注释的源码同仓**           |
| `archive/`                | `archive/` 34 + `openspec/changes/archive/` 121         | 5           | 155         | 不变                                  |
| `.ai/`                    | AI 工具目录                                                 | —           | 79          | 不变                                  |
| ~~`web/`~~                | —                                                       | —           | —           | 🔴 **删（含归档 tar.gz）**                |

**顶层从 21 项 → 11 项**，且主仓受跟踪文件 5730 → **约 3572**。

**三个静态资产被识别出来**（活跃度< 15 commit/ 90 天）：

1. `reference/`（13 commit，536k 文件，25 GB git）→ 阶段三独立仓
2. `learning/`（5 commit，2158 文件）→ 阶段四独立仓
3. `dataset/`（2 commit，692 MB 二进制）→ 出git（D2）

**两个活跃资产被识别出来**（必须留在主仓）：

1. `engine/`（480 commit）→ `engine/`
2. `docs/`（282 commit）→ **保持独立顶层**

---

### 0.5.5 因活跃度结论导致的任务调整

| 原任务                    | 调整                                                                                                              | 原因                       |
| ---------------------- | --------------------------------------------------------------------------------------------------------------- | ------------------------ |
| **T02**（阶段一清创）         | 新增"删归档 tar.gz + 删 `.archive/`"                                                                                  | D3'                      |
| **T03**（阶段二A `git mv`） | 删去 `learning/*` → `notes/`+`practice/` 的 8 条 `git mv`；删去 `docs/` → `notes/wiki/` 与 `archive/superpowers/` 的 4 条 | learning 改走阶段四；docs 保持独立 |
| **T04**（阶段二 B 路径修正）    | **大幅缩减**：不再需要改 `learning/CMakeLists.txt`、不再需要改 `notes/_scripts/linkify.sh` 的 `learning/code-solutions` 路径       | learning 不动              |
| **T04** 新增             | 新增「**解耦 3 步**」：改 `db/api` 的 `find_path HINTS`、删 `algo-prod` 的死 include、修 2 处不存在的 uthash 路径                      | 这是阶段四的前置条件，必须在阶段二完成      |
| **T04** 新增             | 删 `cmake/dual-track-guard.cmake` 整个文件 + 根 `CMakeLists.txt` 的 `LEARNING_BUILD` 分支                                | 解耦后守卫无存在意义               |
| **T05**（阶段二 C 文档）      | 改为按 §0.5.2 重整 `docs/` 内部结构                                                                                      | 新分类规则                    |
| **新增 T06**             | **阶段三**：切出 `reference/` **+ `docs/wiki/` 一并迁出**                                                                 | 3437 条行号引用不能断            |
| **新增 T07**             | **阶段四**：解耦后切出 `learning/` → `book-learning`                                                                     | 5 commit/2158 文件         |

---

## 1. 目标目录树（原方案，已被 §0.5 局部取代，保留供对照）

### 1.1 顶层（方案 C 落地形态）

```
book/
├── engine/                ← engine/ 主体（改名，1860 → 1742：tests 18 + docs 43 外迁）
├── notes/                 ← learning/notes(230) + learning/blog(8) + learning/arkts(77)
│                              + docs/db-wiki(75) + docs/agent-wiki(15) + docs/rag-wiki(13)
├── practice/              ← learning/scaffold(1554) + code-solutions(85) + ds-c(78)
│                              + interview(22) + algo-c(21) + playground(15) + exam(4)
├── apps/                ← apps/(325)
├── vendor/                ← vendor/third_part/(121) + cmake/(3) + dataset/(40)
├── reference/             ← 99 submodule（阶段三切出，仓库内仅留 catalog.yml + README + AUDIT）
├── archive/               ← archive/(34) + .archive/(1) + openspec/changes/archive/(121)
├── docs/                  ← 见 §1.3 拆分规则
├── openspec/              ← 保留（282）
├── scripts/               ← 保留（7）
├── web/                   ← 【阶段一删除，见 D3】
├── CMakeLists.txt         ← 保留，双轨调度器
├── AGENTS.md / CLAUDE.md / README.md / LICENSE
└── .ai/                   ← .claude/(91) + .agents/(85) + .opencode/(22) + .superpowers/(3) + .vscode/(4)
```

### 1.2 各目录属性表

| 目标           | 来源                                                                         | 性质                           | git跟踪      | 预估文件数      | 磁盘           |
| ------------ | -------------------------------------------------------------------------- | ---------------------------- | ---------- | ---------- | ------------ |
| `engine/`    | `engine/`（改名）                                                         | C/C++ 生产代码 + 测试 + Python SDK | 是          | 1742       | ~39 M        |
| `notes/`     | `learning/{notes,blog,arkts}` + `docs/{db,agent,rag}-wiki`                 | Obsidian vault + 外部项目 wiki   | 是          | 418        | ~30 M        |
| `practice/`  | `learning/{scaffold,code-solutions,ds-c,interview,algo-c,playground,exam}` | 教学代码 + 脚手架（**不参与主构建**）       | 是          | 1779       | ~22 M        |
| `apps/`      | `apps/`                                                      | 3 个零依赖原生小程序                  | 是          | 325        | 3.6 M        |
| `vendor/`    | `vendor/third_part/` + `cmake/` + `dataset/`                                      | 第三方依赖 + CMake 工具 + 数据集脚本     | 是          | 164        | **692 M** ⚠  |
| `reference/` | 99 submodule + 4 个元数据文件                                                    | 第三方源码镜像（**阶段三外移**）           | 是（gitlink） | 103 + 536k | **10.4 G** ⚠ |
| `archive/`   | `archive/` + `.archive/` + `openspec/changes/archive/`                     | 历史归档，只读                      | 是          | 156        | 4.6 M        |
| `docs/`      | 见 §1.3                                                                     | 设计文档 + 图 + 过程产物              | 是          | ~200       | 12 M         |
| `openspec/`  | 不变                                                                         | 变更管理                         | 是          | 282        | 1.3 M        |
| `.ai/`       | 5 个AI 工具目录                                                                 | Agent 配置与技能                  | 部分         | 205        | ~4 M         |

**我否掉了简报候选方案的两点**，理由：

1. **否掉 `vendor/` 收编 `dataset/`** —— `dataset/` 692 MB 里99% 是二进制数据（`sift/` 645 MB），与 `vendor/third_part/`（源码依赖）、`cmake/`（构建工具）性质完全不同。混在一个 `vendor/` 下会让"删vendor 数据"这种批量操作误伤数据集。建议 `dataset/` **留在顶层或直接整体 gitignore**，见 D4。
2. **否掉把 `docs/` 的wiki 部分并入 `notes/`** —— `notes/` 是 Obsidian vault（内有 `.obsidian/` 配置、wikilink 语法、`_scripts/linkify.sh` 双向链接脚本），把 103 个外部项目 wiki 塞进去会让 vault 的图谱被无关节点污染。wiki 应独立成 `docs/wiki/`。

### 1.3 `docs/` 414 文件拆分规则（明确规则，不含糊）

按**三个判定维度**依次归属，每个文件只命中一条规则，先匹配先生效：

| 规则                 | 判定条件（可机械执行）                                                                                                                                                                                                                                   | 归属                                                | 数量  |
| ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------- | --- |
| **R1 外部 wiki**     | 路径在 `docs/{db,agent,rag}-wiki/` 下                                                                                                                                                                                                             | → `notes/wiki/`                                   | 103 |
| **R2 过程产物**        | 路径在 `docs/process/` 下（`plans/` 68 + `specs/` 36）                                                                                                                                                                                          | → `archive/superpowers/`                          | 104 |
| **R3 源码图**         | 路径在 `docs/diagrams/` 下（`.excalidraw.json` + 配套 `.md`，89 文件按 `level0`~`level6` 组织）                                                                                                                                                             | → `engine/docs/diagrams/`（图描述的是 db 源码架构，与代码同生命周期） | 89  |
| **R4 架构设计**        | 路径在 `docs/arch/`（36）、`docs/arch/`（17）、`docs/db/`（2）、`docs/sql-executor/`（3）、`docs/modular-rag/`（4）、`docs/deployment/`（2）、`docs/usage/`（1）、`docs/rag/`（1）、`docs/api/`（1）、`docs/learning/`（1）                                          | → `engine/docs/`（跟代码走）                            | 68  |
| **R5 差距分析**        | 路径在 `docs/analysis/`（15）、`docs/analysis/`（11）                                                                                                                                                                         | → `docs/analysis/`（跨轨研究报告，留在 docs）                | 26  |
| **R6 顶层散落**        | `docs/*.md` 直接子文件（23）：按内容归属 —— 含`storage-`/`vector-`/`graph-`/`spatial-`/`timeseries-`/`db-` 前缀的 → `engine/docs/`（19）；`ARCHITECTURE.md` / `multimodal-*` / `*-report.md` / `project-metrics.json` / `postgresql.conf.example` → `docs/` 顶层（4） | 混合                                                | 23  |
| **R7 openspec 镜像** | `openspec/`（1）                                                                                                                                                                                                                           | → `openspec/`（消除镜像）                               | 1   |


**拆分后**：

```
docs/                          ← 顶层入口 + 跨轨研究（~54）
├── README.md（新建导航）
├── architecture/（R6 保留 4 个跨轨 md）
├── analysis/                 ← gap-analysis 15 + multimodal-gap-analysis 11
└── openspec.md→ 移出
engine/docs/                  ← 跟代码同生命周期的设计文档（~155）
├── diagrams/                 ← level0~level6（89）
├── index/ rag-wiki→无 / sql-executor/ deployment/ usage/ api/ db/
└── storage-*.md vector-*.md graph-*.md spatial-*.md timeseries-*.md（19）
notes/wiki/                   ← db-wiki 75 + agent-wiki 15 + rag-wiki 13（103）
archive/superpowers/          ← plans 68 + specs 36（104）
```

**注意 `docs/diagrams/` 归`engine/docs/` 的理由**：89 个文件按 `level0-project` → `level6-rag` 分层，描述的全是 `engine/src/db/**` 的架构。放在顶层 `docs/` 会让读者误以为是跨轨文档。

---

## 2. 各阶段迁移顺序与依赖

### 阶段一：清创（零风险，每步独立可回滚）

**前置**：所有操作前先打基线标签 `git tag pre-cleanup-baseline`，或直接依赖 `origin/main`（本地 836 commits，origin 在 `8a90d5a1`）。

| #      | 任务                                                                                     | 命令                                                                                                                                                                                                       | 验证                                                                                                                                                                    | 回滚                                      |
| ------ | -------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------- |
| **A1** | **撤销幽灵路径清理**（简报 F1 纠正）—— **不做任何 `git rm --cached`**。改为配置 `core.quotepath=false` 消除误判来源 | `git config core.quotepath false`                                                                                                                                                                        | `git ls-files \| grep '^"' \| wc -l` → 应为 **0**；`git ls-files \| grep -c '^learning/'` → 应为 **2158**                                                                  | `git config --unset core.quotepath`     |
| **A2** | 删 4 个空壳目录（纯磁盘，未受跟踪）                                                                    | `rmdir and cd Claw "2026-10-21-23-48-17/.workbuddy-ai" "2026-10-21-23-48-17"`                                                                                                                            | `ls -d and cd Claw 2026-*` → 均 No such file                                                                                                                           | 重新 `mkdir`（本就为空）                        |
| **A3** | 删 `engine/build/`（构建产物躺源码树）                                                       | `rm -rf engine/build`                                                                                                                                                                               | `ls engine/build` → No such file。**注意 `.gitignore:42` 的 `build/` 规则本就覆盖此处**（`git check-ignore -v` 已确认命中 `.gitignore:42`），所以此步只清磁盘、不影响 git                        | 重新构建即生成                                 |
| **A4** | 删`build-games-full.sh`                                                                 | 已随 `907301fa9`/`762165ed0` 删除，**无需动作**                                                                                                                                                                   | `git ls-files \| grep build-games` → 空                                                                                                                                | —                                       |
| **A5** | 决定并执行 `web/` 生死（见 D3，推荐删）                                                              | `git rm -r web`                                                                                                                                                                                          | `git ls-files web \| wc -l` → 0；`git grep -l 'web/' -- '*.cmake' CMakeLists.txt` → 空（已验证无 CMake 引用）                                                                   | `git revert <sha>`                      |
| **A6** | 合并 `engine/tests/`(18) → `engine/test/db/storage/legacy/`                    | `mkdir -p engine/test/db/storage/legacy && git mv engine/tests/*.c engine/test/db/storage/legacy/` + 在 `engine/test/db/storage/CMakeLists.txt` 追加 `add_project_test(legacy_xxx ...)` | `cmake -B build/engineering -S engineering -DBUILD_TESTING=ON && cmake --build build/engineering --parallel 4` 通过；`ctest --test-dir build/engineering -N` 新增 18 个测试条目 | `git mv` 反向 + `git checkout` CMakeLists |
| **A7** | 重写 `.gitignore`（3.7 KB → 约 1.6 KB）                                                     | 见 §3.7                                                                                                                                                                                                   | `git status --porcelain` 不新增噪声；`git check-ignore -v <每个历史被忽略路径>` 逐条比对通过                                                                                               | `git checkout .gitignore`               |
| **A8** | 收敛 5 个 AI 工具目录到 `.ai/`（`git mv`，保留历史）                                                  | 见 §3.8                                                                                                                                                                                                   | `git ls-files .claude \| wc -l` → 91（在新位置）；构建不受影响（非CMake 引用）                                                                                                          | `git mv` 反向                             |

**阶段一出口条件**：`cmake -B build/engineering -S engineering -DBUILD_TESTING=ON` 成功 + `ctest` 通过 + `git status --porcelain` 干净（除 5 个 submodule 指针）。

---

### 阶段二：顶层分区（`git mv` 级重排 + 路径修正）

**关键设计决策：全部用 `git mv`，不重新 add**。这样 829 commits 的历史跟随重命名（`git log --follow` 可用），且 `git mv` 是单次原子操作，`git reset --hard HEAD` 即可完全回滚。

**顺序严格按依赖：从被引用最多、最深的开始移**

| #       | 任务                                                                                                                               | 命令                                                                                                                                                                                                       | 依赖                         | 验证                                                                                                           | 回滚                          |    |                                                                            |    |
| ------- | -------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------- | ------------------------------------------------------------------------------------------------------------ | --------------------------- | -- | -------------------------------------------------------------------------- | -- |
| **B1**  | `engine/` → `engine/`                                                                                                       | `git mv engineering engine`                                                                                                                                                                              | —                          | 见B1a                                                                                                         | `git reset --hard HEAD~1`   |    |                                                                            |    |
| B1a     | 修根 `CMakeLists.txt`                                                                                                              | \`sed -i 's                                                                                                                                                                                              | engine/CMakeLists.txt | engine/CMakeLists.txt                                                                                        | g' CMakeLists.txt\`         | B1 | `cmake -B build/root -S . -DENGINEERING_BUILD=ON -DLEARNING_BUILD=ON` 配置成功 | 同上 |
| B1b     | 修 `engine/` 内部 3 处自引用                                                                                                       | `engine/CMakeLists.txt:18`（`../cmake/`）保持不变（相对路径仍正确）；`:119-125` 的 `../third_part/` 依赖 vendor 位置，见 B4                                                                                                | B1                         | `grep -rn '\.\./third_part' engine/`                                                                         | 同上                          |    |                                                                            |    |
| **B2**  | `learning/notes` → `notes/`，`learning/{blog,arkts}` → `notes/`                                                                   | `git mv learning/notes notes` 等 3 条                                                                                                                                                                      | B1（不依赖 B1，但排在后面便于统一验证）     | `ls notes/.obsidian` 存在                                                                                      | `git reset --hard HEAD~1`   |    |                                                                            |    |
| **B3**  | `learning/{scaffold,code-solutions,ds-c,interview,algo-c,playground,exam}` → `practice/`，`learning/include` → `practice/include` | 8 条 `git mv`                                                                                                                                                                                             | B2                         | ⚠️ 见 §3.3 跨轨引用修正（B3a）                                                                                        | 同上                          |    |                                                                            |    |
| B3a     | **修跨轨 include**（F7 暴露的 4 处）                                                                                                      | `sed -i 's\|\.\./learning/include\|../practice/include\|g' engine/src/algo-prod/CMakeLists.txt engine/src/db/api/CMakeLists.txt` + `learning/CMakeLists.txt` 内的 `${CMAKE_CURRENT_LIST_DIR}/../cmake/` 保持 | B3                         | `cmake -B build/engineering -S engine -DBUILD_TESTING=ON` 成功（若 `find_path` 找不到 uthash会 FATAL_ERROR，这是有效检测）   | 同上                          |    |                                                                            |    |
| B3b     | 修 `guard_no_cross_track_reference` 硬编码目录名（F7）                                                                                    | `cmake/dual-track-guard.cmake:12` 改为读变量而非硬编码：`if(_current_dir STREQUAL "${_track_a}" OR ...)`，调用方传入轨道名                                                                                                   | B3                         | `cmake -B build/root -S .` 成功（守卫仍生效，不误报）                                                                     | 同上                          |    |                                                                            |    |
| **B4**  | `vendor/third_part/` → `vendor/third_part/`，`cmake/` → `vendor/cmake/`，`dataset/` **留在顶层**                                              | `git mv third_part vendor/third_part` + `git mv cmake vendor/cmake`                                                                                                                                      | B1                         | `engine/CMakeLists.txt:119-125` 的 `../third_part/` 仍正确（因 vendor 在同级）；但 `../cmake/dual-track-guard.cmake` 仍正确 | 同上                          |    |                                                                            |    |
| B4a     | 若采纳 D4（dataset 整体 gitignore）                                                                                                     | `git rm -r --cached dataset` + `.gitignore` 加 `dataset/*.bin` 等                                                                                                                                          | B4                         | `git status` 干净；`git log -- dataset` 历史仍在                                                                    | `git reset HEAD -- dataset` |    |                                                                            |    |
| **B5**  | `apps/` → `apps/`                                                                                                  | `git mv wechat-miniprogram apps`                                                                                                                                                                         | —                          | `ls apps/knowledge-hub` 存在                                                                                   | 同上                          |    |                                                                            |    |
| **B6**  | `archive/` + `.archive/` + `openspec/changes/archive/` → `archive/`                                                              | 3 条 `git mv`                                                                                                                                                                                             | —                          | `ls archive/`                                                                                                | 同上                          |    |                                                                            |    |
| **B7**  | `docs/` 按 §1.3 的 R1~R7 拆分                                                                                                        | 约 20 条 `git mv`                                                                                                                                                                                          | B1,B2                      | 见下                                                                                                           | 同上                          |    |                                                                            |    |
| B7a     | R3 `docs/diagrams/` → `engine/docs/diagrams/`                                                                                    | `git mv docs/diagrams engine/docs/diagrams`                                                                                                                                                              | B1,B7                      | —                                                                                                            | 同上                          |    |                                                                            |    |
| B7b     | R4 `docs/{architecture,index,db,sql-executor,modular-rag,deployment,usage,rag,api}` → `engine/docs/`                             | 9 条                                                                                                                                                                                                      | B1,B7                      | —                                                                                                            | 同上                          |    |                                                                            |    |
| B7c     | R1 `docs/{db,agent,rag}-wiki` → `notes/wiki/{db,agent,rag}`                                                                      | 3 条 + 改名                                                                                                                                                                                                 | B2,B7                      | —                                                                                                            | 同上                          |    |                                                                            |    |
| B7d     | R2 `docs/superpowers` → `archive/superpowers`                                                                                    | 1 条                                                                                                                                                                                                      | B6,B7                      | —                                                                                                            | 同上                          |    |                                                                            |    |
| B7e     | R5 `docs/{gap-analysis,multimodal-gap-analysis-2026}` → `docs/analysis/`                                                         | 2 条                                                                                                                                                                                                      | B7                         | —                                                                                                            | 同上                          |    |                                                                            |    |
| B7f     | R6 顶层23 个散 md 逐个判定（19 → `engine/docs/`）                                                                                          | 19 条                                                                                                                                                                                                     | B1,B7                      | 逐个人工确认                                                                                                       | 同上                          |    |                                                                            |    |
| **B8**  | AI 目录 `.claude` `.agents` `.opencode` `.superpowers` `.vscode` → `.ai/`                                                          | 5 条 `git mv`                                                                                                                                                                                             | A8                         | `git ls-files .ai \| wc -l` → 205                                                                            | 同上                          |    |                                                                            |    |
| **B9**  | 全量修正路径引用                                                                                                                         | 见 §3                                                                                                                                                                                                     | B1~B8                      | 见 §3 各表                                                                                                      | 同上                          |    |                                                                            |    |
| **B10** | 重写 `README.md` 项目结构树                                                                                                             | 逐段手改（现有 148 行）                                                                                                                                                                                           | B1~B9                      | `README.md` 结构树与 `ls` 输出一致                                                                                   | 同上                          |    |                                                                            |    |

**阶段二出口条件**（必须全部满足）：

```bash
# 1. 工程轨可构建 + 测试全跑
cmake -B build/engineering -S engine -DBUILD_TESTING=ON
cmake --build build/engineering --parallel 4
ctest --test-dir build/engineering --output-on-failure

# 2. 学习轨可构建（若保留 learning/ 轨道名，见 D5）
cmake -B build/learning -S practice -DBUILD_TESTING=ON
cmake --build build/learning --parallel 4

# 3. 根双轨调度器
cmake -B build/root -S . -DENGINEERING_BUILD=ON -DLEARNING_BUILD=ON

# 4. 无残留旧路径引用
git grep -n -E '(^|[^a-zA-Z_/.-])(engineering|learning)/' -- ':!reference' ':!openspec' ':!archive' \
  | grep -vE '\.(md):.*(历史|已迁移|原 )' | wc -l    # 期望 0
```

---

### 阶段三：切出 `reference/` 独立资料仓（**可延后，风险最高**）

**收益量化（实测）**：

| 指标               | 切出前              | 切出后                                                                                    | 收益                                  |
| ---------------- | ---------------- | -------------------------------------------------------------------------------------- | ----------------------------------- |
| 主仓受跟踪文件          | 5730             | **5630**                                                                               | 主仓索引轻量                              |
| 主仓工作树            | 10.4 GB + 0.7 GB | **~0.8 GB**                                                                            | 省~10 GB                             |
| **`.git` 体积**    | **26 GB**        | **~1.1 GB**                                                                            | 🔴 **省 25 GB**                      |
| 主仓 submodule 数   | 99               | **4**（`vendor/third_part/{googletest,uthash}` + `engine/third_part/{googletest,uthash}`） | CI 的 `submodule update` 从99 个降到 4 个 |
| 主仓 `.gitmodules` | 99 条             | 4 条                                                                                    | 配置文件瘦身                              |

**这25 GB 是 `.git/modules/reference/` 下 95 个 reference submodule 各自的独立 .git 目录**。切出后主仓立即回收。

#### 阶段三任务序列

| #      | 任务                                       | 命令要点                                                                                                                                                                                                        | 验证                                        | 回滚                                 |
| ------ | ---------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------- | ---------------------------------- |
| **C1** | **前置快照**（不可跳过）                           | `git tag stage2-complete` + `git push origin stage2-complete` + 记录 `git submodule status > /tmp/sub-before.txt`                                                                                             | 标签存在且已推送                                  | `git tag -d`                       |
| **C2** | 在 `reference/` 原地初始化新仓（**先建后拆**，不移动任何文件） | `cd reference && git init -b main && git remote add origin <new-repo-url>`                                                                                                                                  | `git -C reference status` 正常              | `rm -rf reference/.git`            |
| **C3** | 处理 4 个非 submodule 元数据文件                  | `git add catalog.yml README.md AUDIT.md fetch-manifest.txt`（`git ls-files reference` 实测 102 条= 98 gitlink + 4 文件）                                                                                           | `git -C reference ls-files \| wc -l` → 4  | —                                  |
| **C4** | 把 95 个 reference submodule 转为新仓的 gitlink | 在 `reference/.gitmodules` 中把 path 从 `reference/xxx/yyy` 改为 `xxx/yyy`，然后在主仓 `git rm --cached reference/{ai,benchmark,...}`（95 条），再在主仓 `.gitmodules` 删除对应 95 个 stanza                                         | `git submodule status \| wc -l` → **4**   | `git reset --hard stage2-complete` |
| **C5** | 主仓加 `reference/` 忽略 + 说明文件               | `.gitignore` 加 `/reference/*` + `!/reference/{README.md,catalog.yml,AUDIT.md}`（保留 4 个元数据文件在主仓，便于主仓文档引用）                                                                                                     | `git status` 干净                           | 同上                                 |
| **C6** | 首次 clone 验证（**关键验证**）                    | 新目录执行 `git clone <new-repo> && cd book-reference && git submodule update --init --recursive --depth 1`                                                                                                      | 95 个 submodule 全部拉取成功                     | 删除测试目录                             |
| **C7** | CI 修正                                    | `.github/workflows/{ci,asan}.yml` 的 `submodule update --init --depth 1 vendor/third_part/googletest engine/third_part/uthash` **无需改动**（只列了 4 个非 reference submodule）→ 实际是 **减负**，CI 从未初始化 reference submodule | CI 两个 workflow 全绿                         | `git revert`                       |
| **C8** | 文档内链修正（669 行，见 §3.6）                     | `sed` 把 `reference/xxx/yyy` 改为 `book-reference` 仓 URL 或 `../book-reference/xxx/yyy`                                                                                                                         | `git grep -c 'reference/' -- docs` → 大幅下降 | 同上                                 |

**阶段三风险与兜底**：

| 风险                                | 概率       | 兜底                                                                                                                       |
| --------------------------------- | -------- | ------------------------------------------------------------------------------------------------------------------------ |
| 新仓 push 失败（仓库大小/配额/LFS）           | 中        | C1 标签已在远端，可`git reset --hard stage2-complete` 完全恢复                                                                       |
| submodule 重挂后 SHA 不一致             | 中        | C1 的 `/tmp/sub-before.txt` 逐行 diff 校验；不一致则 `git submodule update --init --reference`                                     |
| 有人本地在 `reference/` 有未提交改动         | 低        | C2 前必跑 `git -C reference status --short`（95 个 submodule 各自检查）                                                            |
| **`docs/wiki/` 3437 条行号引用失去被引对象** | 🔴 **高** | **必须与 reference 同步迁出（§0.5.2）**。若只切 reference 不切 wiki，`check-wiki-refs.py` 模式 A 全面失效，59/69 个 db-wiki 文件无模式 B 兜底           |
| `docs/` 中 669 行 `reference/` 内链失效 | 高        | 分两批：先改 wiki 类（`docs/wiki/`，约 600 行），再改散落（69 行）。**迁出后 wiki 内部引用改为相对路径（随 submodule 一起走），主仓残留引用改为 `book-reference` 远端 URL** |

**建议：阶段三延后到阶段二合并后至少 1 周**，且只在 C6 验证通过后才删主仓的 95 个 gitlink。

---

### 阶段四：切出 `learning/` 独立仓（新增，**必须在 T04 的解耦 3 步完成后**）

**前置条件（硬性）**：`engine/` 对 `learning/include` 的依赖归零，即 T04 中的「解耦 3 步」已完成并通过构建验证。若未解耦就切仓，`engineering` 的 CMake 会直接 FATAL_ERROR。

| #      | 任务                    | 命令要点                                                                                                                                                                                    | 验证                                              | 回滚                                 |
| ------ | --------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------- | ---------------------------------- |
| **D1** | 快照 tag                | `git tag stage3-complete && git push origin stage3-complete`                                                                                                                            | 标签已推送                                           | —                                  |
| **D2** | `learning/` 原地初始化新仓   | `cd learning && git init -b main && git remote add origin <new-repo>`                                                                                                                   | `git -C learning status`                        | `rm -rf learning/.git`             |
| **D3** | 提交 2158 个文件           | `git add -A && git commit`                                                                                                                                                              | `git -C learning ls-files \| wc -l` → **2158**  | —                                  |
| **D4** | 主仓删 gitlink           | `git rm -r --cached learning` + `.gitignore` 加 `/learning/*` + `!learning/README.md`                                                                                                    | `git status` 干净                                 | `git reset --hard stage3-complete` |
| **D5** | 新仓首次 clone 验证         | `git clone <new-repo> && cd book-learning && cmake -B build -S . -DBUILD_TESTING=ON`                                                                                                    | 配置+ 构建通过（`learning/` 自带完整 `CMakeLists.txt`，自包含） | 删测试目录                              |
| **D6** | CI 清理                 | 删 `ci.yml` 的 `build-learning-ubuntu`(147) / `test-learning-ubuntu`(177) 两个 job；`submodule update` 行去掉 `vendor/third_part/googletest` 前的 learning 关联（实际 learning 不依赖 submodule，仅 googletest 共用） | CI 剩 4 个 job 全绿                                 | `git revert`                       |
| **D7** | 根 `CMakeLists.txt` 简化 | 删 `LEARNING_BUILD` option + `add_subdirectory(learning)` 块（根文件只剩工程轨）                                                                                                                    | `cmake -B build/root -S .` 通过                   | 同上                                 |
| **D8** | 删双轨守卫                 | 删 `cmake/dual-track-guard.cmake`（3 文件），删 `engine/CMakeLists.txt:17-22` 的 include 块                                                                                                 | 构建通过                                            | 同上                                 |


**阶段四收益**：

| 维度                        | 切出前                                                | 切出后                     |
| ------------------------- | -------------------------------------------------- | ----------------------- |
| 主仓受跟踪文件                   | 5730                                               | **3572**（-38%）          |
| 主仓 `git status` 需stat 的文件 | 5730                                               | 3572                    |
| CI job 数                  | 6                                                  | **4**                   |
| 根 `CMakeLists.txt` 复杂度    | 双轨调度器（14 行）                                        | 单轨（5 行）                 |
| `cmake/` 目录               | 需含 `dual-track-guard.cmake` + `ProjectUtils.cmake` | 只剩 `ProjectUtils.cmake` |

**阶段四风险与兜底**：

| 风险                              | 兜底                                      |
| ------------------------------- | --------------------------------------- |
| 解耦不彻底导致构建失败                     | 阶段二 T04 已通过完整构建验证，此风险在阶段二就被暴露           |
| 新仓 push 失败（2158 文件 / 27 MB，很轻量） | 风险极低；`git reset --hard stage3-complete` |
| 有人本地 `learning/` 有未提交改动         | D2 前必跑 `git -C learning status --short` |

**建议：阶段四可与阶段三并行**（两者无依赖），但建议先做阶段三（收益 25 GB > 阶段四的 2158 文件）。

---

## 3. 路径修正清单（已用 grep 实测）

### 3.1 全局规模

排除 `reference/`、`openspec/`、`.claude`、`.agents`、`.playwright-mcp` 后，`(engineering|learning)/` 引用共 **741 文件 / 3950 行**。按扩展名：

| 类型            | 行数       | 文件数     | 修正手法                                                 | 工作量         |
| ------------- | -------- | ------- | ---------------------------------------------------- | ----------- |
| `.md`         | 3776     | 633     | **分类处理**（见 3.6）                                      | 2-3 h       |
| `.c/.h/.cpp`  | 51       | 46      | 全部是**注释里的路径**（`// engine/src/...`），非 `#include` | 30 min（sed） |
| `.yml`（CI）    | 28       | 2       | 手改（步骤关键）                                             | 30 min      |
| `.sh`         | 23       | 5       | sed                                                  | 20 min      |
| `.py`         | 22       | 9       | sed                                                  | 20 min      |
| `.txt`（CMake） | 31       | 16      | sed                                                  | 20 min      |
| `.gitignore`  | 8        | 1       | 手改                                                   | 15 min      |
| `.gitmodules` | 4        | 1       | 手改（`git config -f .gitmodules --unset-all`）          | 15 min      |
| `.cmake`      | 1        | 1       | 手改                                                   | 5 min       |
| **合计**        | **3950** | **741** | —                                                    | **约 5-6 h** |

### 3.2 CMake（**最高优先级，改错直接构建失败**）

| 文件:行                                                    | 现状                                                          | 改为                                                                            | 手法              |
| ------------------------------------------------------- | ----------------------------------------------------------- | ----------------------------------------------------------------------------- | --------------- |
| `CMakeLists.txt:8,9`                                    | `engine/CMakeLists.txt`                                | `engine/CMakeLists.txt`                                                       | sed             |
| `CMakeLists.txt:12,13`                                  | `learning/CMakeLists.txt`                                   | `practice/CMakeLists.txt`                                                     | sed（若 D5 采纳改名）  |
| `engine/CMakeLists.txt:18`                              | `${CMAKE_CURRENT_LIST_DIR}/../cmake/dual-track-guard.cmake` | `../vendor/cmake/...`                                                         | sed（B4 后）       |
| `engine/CMakeLists.txt:119,120,124,125`                 | `../third_part/{libmicrohttpd,googletest}`                  | `../vendor/third_part/{...}`                                                  | sed（B4 后）       |
| `engine/CMakeLists.txt:114,115`                         | `${CMAKE_CURRENT_LIST_DIR}/third_part/cjson`                | 保持（这是 `engine/` **内部**的third_part，不受 B4 影响）                              | 无需改             |
| `learning/CMakeLists.txt:14`                            | `../cmake/dual-track-guard.cmake`                           | `../vendor/cmake/...`                                                         | sed             |
| `learning/CMakeLists.txt:47-50`                         | `../third_part/googletest`                                  | `../vendor/third_part/googletest`                                             | sed             |
| **`engine/src/algo-prod/CMakeLists.txt:16`**            | `${ENGINEERING_SOURCE_DIR}/../learning/include`             | `../practice/include`                                                         | sed（**跨轨，B3a**） |
| **`engine/src/db/api/CMakeLists.txt:7`**                | `HINTS ${ENGINEERING_SOURCE_DIR}/../learning/include`       | `../practice/include`                                                         | sed（**跨轨，B3a**） |
| `engine/src/db/storage/graph/CMakeLists.txt:19`         | `${CMAKE_SOURCE_DIR}/third_part/uthash/src`                 | ⚠️ **该路径不存在**（F8），建议直接删除该行                                                    | 手改              |
| `engine/src/db/storage/sparse/CMakeLists.txt:16`        | `${ENGINEERING_SOURCE_DIR}/third_part/uthash/src`           | 存在，可保留                                                                        | 无需改             |
| `engine/src/db/storage/ts/CMakeLists.txt:12`            | 同上                                                          | 存在，可保留                                                                        | 无需改             |
| `engine/test/db/graph/CMakeLists.txt:25`                | `${CMAKE_SOURCE_DIR}/third_part/uthash/src`                 | ⚠️ 同不存在，建议删                                                                   | 手改              |
| `engine/test/db/executor/CMakeLists.txt:10,23,32,40,48` | `${CMAKE_SOURCE_DIR}/engineering/include`                   | `${CMAKE_SOURCE_DIR}/include`（简化，因 `-S engine` 时 CMAKE_SOURCE_DIR 已是 engine/） | sed + 验证        |

**兼容性软链方案（备选，任务量最省）**：若 B1~B4 修正点太多导致风险偏高，可在根目录建 3 个软链：

```bash
ln -s engine engineering; ln -s practice learning; ln -s vendor/third_part third_part
```

**但我不推荐**：软链会被 CMake 的 `EXISTS` 检查当真实目录处理，且 Windows/MinGW 环境下 Git Bash 的软链需要管理员权限，会制造新的跨平台问题。仅作为 B9 失败时的兜底。

### 3.3 脚本

| 文件                                                        | 引用数 | 具体                                                                                                                                                               |      |
| --------------------------------------------------------- | --- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---- |
| `scripts/verify_migration.sh:10 处`                        | 10  | 硬编码 `engine/src/...` 字符串，用于统计 malloc/calloc                                                                                                                 | sed  |
| `engine/sdk/tests/cross_lang_e2e.sh`                 | 7   | 路径 + 编译参数                                                                                                                                                        | sed  |
| `learning/notes/_scripts/linkify.sh`                      | 6   | `NOTES_DIR="learning/notes"` / `CODE_DIR="learning/code-solutions"` — **必须手改**，它是 `learning/notes` 与 `learning/code-solutions` 之间的双向链接生成器，路径错了会静默失效（不报错，只是不生成链接） | 手改   |
| `engine/scripts/coverage/run.sh`                     | 4   | lcov/gcov 路径                                                                                                                                                     | sed  |
| `engine/scripts/coverage/lcov-aggregate.sh`          | 3   | 同上                                                                                                                                                               | sed  |
| `learning/scripts/sync-learn-deep.sh:17`                  | 3   | `${REPO_ROOT}/learning/notes/learn_deep`                                                                                                                         | sed  |
| `engine/scripts/check_wsl_build.sh`                  | 1   |                                                                                                                                                                  | sed  |
| `engine/scripts/coverage/*.py` (6 个)                 | —   | 输出路径                                                                                                                                                             | sed  |
| `engine/scripts/rename-ids.py` / `rename-ids-run.py` | —   | 源码路径                                                                                                                                                             | sed  |
| `dataset/*.py` (6 个)                                      | —   | 相对路径为主，风险低                                                                                                                                                       | 检查即可 |

### 3.4 CI（`.github/workflows/`）

| 文件:行                                         | 现状                                                                                          | 动作                                                                                                             |
| -------------------------------------------- | ------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| `asan.yml:27`、`ci.yml:58,88,119,158,189,224` | `git submodule update --init --depth 1 vendor/third_part/googletest engine/third_part/uthash` | → `vendor/third_part/googletest engine/third_part/uthash`（sed，7 处）                                             |
| `ci.yml:67,97,134`                           | `cmake -B engine/build -S engineering`                                                 | → `-B build/engineering -S engine`（顺带修正历史遗留：CI 在源码树内建 `engine/build`，违反 `AGENTS.md:35` 的"产物进 `build/`"规范） |
| `ci.yml:103`                                 | `working-directory: ./engineering/build`                                                    | → `./build/engineering`                                                                                        |
| `ci.yml:167,198`                             | `cmake -B build-learning -S learning`                                                       | → `-B build/learning -S practice`（若 D5 改名）                                                                     |
| `ci.yml:205`                                 | `working-directory: ./build-learning`                                                       | → `./build/learning`                                                                                           |
| `ci.yml:233,236,242,250,260`                 | `working-directory: ./engineering` / `cmake -B build-cov -S .`                              | → `./engine`                                                                                                   |
| `ci.yml:246`                                 | `working-directory: ./engineering/build-cov`                                                | → `./build/engineering-cov`（同时 `.gitignore:14` 规则要跟着改）                                                         |
| `ci.yml:272-277`                             | `engine/coverage-*.{html,info,json,md,svg}`                                            | → `build/engineering-cov/...`                                                                                  |
| `ci.yml:8-26`（大段注释）                          | 描述 submodule 布局                                                                             | 阶段三后需重写（99 → 4）                                                                                                |

**CI 工作量**：约 25 处 sed + 注释重写，**1-1.5 h**。这是阶段二最容易漏的地方，务必用 §2阶段二出口条件第 3 条验证。

### 3.5 Python SDK

`engine/sdk/python/setup.py:112,115` → `name="pymultimodal"`、`packages=["pymultimodal"]` — **用的是包名而非文件路径**，所以 `engine/` → `engine/` 对 Python SDK **零影响**。`minivecdb/` 同理（纯目录，无 package_dir 声明）。

唯一受影响：`.gitignore:118-119` 的

```
engine/sdk/python/minivecdb/__pycache__/
engine/sdk/python/minivecdb/*.pyc
```

→ `engine/sdk/python/...`。但 `.gitignore:120-121` 已有全局 `__pycache__/` 和 `*.pyc`兜底 → **这两条冗余规则可直接删除**。

**结论：Python SDK 零修正**。简报把它列为风险点是多余的。

### 3.6 文档内链（3776 行 / 633 文件）

**分类处理，不能一把 sed**：

| 类别                    | 文件数  | 行数    | 手法                                                                                                                                                                    |
| --------------------- | ---- | ----- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **必须改**（构建/运行指令）      | 8    | ~40   | 手改：`README.md`（9 处结构树+ 命令）、`AGENTS.md`（`vendor/third_part/googletest` 1 处 + 构建命令 3 处）、`CLAUDE.md`（391 行，需逐段核对）、`engine/README.md`、`engine/docs/coverage-policy.md` |
| **应改**（仍在生效的技术文档）     | ~60  | ~600  | sed：`docs/arch/**` 里的路径、`docs/arch/**`、`engine/docs/**`                                                                                                 |
| **可跳过**（历史快照/变更记录）    | ~565 | ~3330 | **不动**：`archive/`、`openspec/`、`docs/process/`（已归档）、以及所有 `task-N-report.md` / `*-plan.md` / `*-spec.md`。理由：这些是**当时状态的记录**，改写会篡改历史语义，且无人依赖                          |
| **必须改**（wiki 类，阶段三相关） | 30   | 669   | 见 C8，`reference/` → 新仓 URL                                                                                                                                            |

**`.md` 精确 sed 模板**（仅对"必须改/应改"子集）：

```bash
# 只在已分类的文档子集上跑，不要全仓sed
git grep -l -E '(^|[^a-zA-Z_/.-])engine/' -- \
  'README.md' 'AGENTS.md' 'CLAUDE.md' 'docs/arch/**' 'docs/arch/**' \
  'engine/docs/**' | xargs sed -i \
  -e 's|\bengineering/src/|engine/src/|g' \
  -e 's|\bengineering/include/|engine/include/|g' \
  -e 's|\bengineering/test/|engine/test/|g' \
  -e 's|\bengineering/docs/|engine/docs/|g' \
  -e 's|\bengineering/build/|build/engineering/|g' \
  -e 's|\bthird_part/|vendor/third_part/|g' \
  -e 's|\blearning/notes/|notes/|g' \
  -e 's|\blearning/code-solutions/|practice/code-solutions/|g'
```

⚠️ **不要用 `\bengineering/` 全局替换** —— 会命中 `D:/code/book/engineering/` 这样的历史命令记录、以及 `engine/third_part/`（这是 engineering **内部**目录，不改）。

### 3.7 `.gitignore`（3.7 KB → 约 1.6 KB）

**现存 14 行带路径前缀的规则**及其处置：

| 行           | 规则                                                             | 处置                                                                                  |
| ----------- | -------------------------------------------------------------- | ----------------------------------------------------------------------------------- |
| 14-15       | `engine/build-cov/` `engine/coverage-gcov/`          | → `engine/build-cov/` `engine/coverage-gcov/`（或改用 `build/` 统一收口）                    |
| 91-93       | `/test/test_*.db` 等3 条                                         | 指向**不存在的** `/test/` 目录（实为 `engine/test/`）→ **删除**                              |
| 111-112     | `vendor/third_part/libmicrohttpd/` `vendor/third_part/cjson/`                | ⚠️ `cjson` 实际在 `engine/third_part/cjson`，根 `vendor/third_part/` 无 cjson → 根规则删，工程规则保留 |
| 115         | `openspec/changes/archive/`                                    | ⚠️ 该目录**已有 121 个文件被跟踪**，`git check-ignore` 不生效（已跟踪文件不受 gitignore 约束）→ 规则保留，但需注意     |
| 118-119     | `engine/sdk/python/minivecdb/{__pycache__,*.pyc}`         | **删除**（被120-121 全局规则覆盖）                                                             |
| 127,136,173 | `.claude/{worktrees,sdd,tasks}/`                               | → `.ai/.claude/...`                                                                 |
| 130-131     | `reference/models/{*.gguf,*.bin}`                              | 阶段三后失效 → 删除                                                                         |
| 170         | `engine/build-linux/`                                     | → `engine/build-linux/`                                                             |
| 172         | `vendor/third_part/cmocka_install/`                                   | → `vendor/third_part/cmocka_install/`                                               |
| 181-183     | `engine/third_part/{cpp-httplib,nlohmann,openssl-stubs}/` | 保持（engineering 内部）                                                                  |
| 186-187     | 时间戳目录正则                                                        | 保持（WorkBuddy 工作区）                                                                   |
| 190         | `.workbuddy-ai/`                                               | 保持                                                                                  |

**重复规则清理**：`build-learning/` 出现 **3 次**（56,76,126 行）、`test-results/` **2 次**（45,80）、`build2/` **2 次**（57,96）、`test_dir/` **2 次**（55,98）、`build-engineering/` **2 次**（75,125）、`build-default/` **2 次**（74,124）→ 各留 1 条。

**新 `.gitignore` 结构**：分5 段—— 编译产物通配（`*.[osol]`）/ 构建目录（`build*`）/ 依赖与工具（`node_modules/`、`.ai/` 运行时）/ 环境与密钥（`.env*`）/ AI 工具运行时（`.ai/*/worktrees| tasks|sdd`、`/20[0-9].../`）。

### 3.8 AI 工具目录 → `.ai/`

| 原路径                | 文件数 | 体积    | 处置                                                                                                                                                |
| ------------------ | --- | ----- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| `.playwright-mcp/` | 129 | 2.6 M | 🔴 **只保留 `.gitkeep`，其余全部 `git rm`**。99 个 yml + 19 个 console log + 8 个 png 全是 MCP 调试产物（文件名是时间戳 + 页面快照），无任何长期价值。→ `.gitignore` 加 `.playwright-mcp/` |
| `.claude/`         | 91  | 765 K | `skills`(59) + `projects`(20) + `commands`(11) + `settings.json`(1)。`projects/D--code-book/memory/` 20 个 md 是**项目记忆**，有长期价值 → 保留                  |
| `.agents/`         | 85  | 596 K | `skills/` → 保留（与 `.claude/skills/` 有重复，可后续合并）                                                                                                     |
| `.opencode/`       | 22  | 196 K | `skills`(11) + `commands`(11) → 保留                                                                                                                |
| `.superpowers/`    | 3   | —     | `sdd/task-4-report.md` 等过程报告 → 移入 `archive/`                                                                                                      |
| `.vscode/`         | 4   | —     | `tasks.json` / `launch.json` 含绝对路径 `D:/code/book` → 保留但需修正路径                                                                                      |
| **合计**             | 334 | ~4 M  | **净减 126 文件 / 2.6 MB**                                                                                                                            |

### 3.9 `.gitmodules`（4 条非 reference 部分）

阶段二改path（submodule 的 URL 不变，只改挂载点）：

```bash
git config -f .gitmodules submodule.third_part/googletest.path vendor/third_part/googletest
git config -f .gitmodules submodule.third_part/uthash.path vendor/third_part/uthash
git config -f .gitmodules submodule.engineering/third_part/googletest.path engine/third_part/googletest
git config -f .gitmodules submodule.engineering/third_part/uthash.path engine/third_part/uthash
# 同步 .git/config 与工作区
git submodule sync --recursive
```


⚠️ **注意**：submodule 的 gitlink SHA 也会随 `git mv` 移动，必须确认 `.git/modules/` 下的实际 git 目录跟着走。`git mv` 对 submodule 目录的行为在 Git 2.17+ 已修复（本机 Git 2.55，无此问题），但仍需 `git submodule status` 逐条校验。

---

## 4. `"learning/` 幽灵路径处理 —— **结论：撤销该项**

### 4.1 查证过程与结论

```bash
# 现象
$ git ls-files | grep '^"' | wc -l
121

# 根因：core.quotePath 默认为 true，非 ASCII 路径被 C 风格转义
$ git config --get core.quotepath# 空 → 使用默认 true

# 验证 1：-z 输出（不做任何转义）
$ git ls-files -z | tr '\0' '\n' | grep -c '^learning/'
2158#真实 learning 条目数

# 验证 2：关闭 quotePath
$ git -c core.quotepath=false ls-files | grep -c '^learning/'
2158                # 完全一致

# 验证 3：是否真有文件以引号开头
$ git ls-files -z | python -c "..."   # entries starting with literal quote char: 0

# 验证 4：这 121 个文件在磁盘上是否存在
$ # 逐个 os.path.exists 检查 → non-ASCII entries MISSING on disk: 0
```

**结论：121 个文件全部真实存在于磁盘，0 个缺失，且索引中没有任何路径以字面引号开头。**

它们是 `learning/notes/` 与 `learning/blog/` 下的**中文文件名**，例如：

- `learning/blog/学习/阿里-排序算法.md`
- `learning/notes/language/C语言/细说/6.指针函数指针.md`
- `learning/notes/excerpt/2025/小林coding/代码整洁之道-如何写出好代码的实操.md`

Git 用 `\344\270\255\346\226\207` 这类八进制转义 + 双引号包裹来保证 `ls-files` 输出的逐行可解析性。**它们是文件，不是幽灵。**

### 4.2 为什么 `git rm --cached` 必须撤销

若执行 `git rm --cached '"learning/blog/..."`：

1. 121 个文件从索引移除 → 进入 untracked 状态
2. **文件内容仍在磁盘**，看似"无损"
3. 但：① 提交后这121 个文件不再受版本控制，任何一次 `git clean -fd` 都会**永久删除**；② `git checkout` 到其他分支时不会恢复它们；③ `.obsidian` vault 里这些笔记将失去 git 备份 —— 而这是用户 27 M 学习资料的核心
4. 引号转义导致路径传给 shell 时需多层反转义，**极易误伤**：121 条路径里包含 `(`、`）`、`+` 等 shell 元字符（实测如 `6.指针函数指针(附表速查）.md`），手工构造 121 条 `git rm` 命令的出错概率接近 100%

### 4.3 正确做法

**不删任何文件**。只需一条本地配置消除误判来源：

```bash
git config core.quotepath false
```

之后 `git ls-files`、��� `git status`、`git diff` 对中文路径均直接显示可读形式。

**阶段一 A1 任务替换为**：设置 `core.quotepath=false` + 在 `CLAUDE.md` / `AGENTS.md` 记一条「排查路径问题前先 `git config core.quotepath false`」。

**验证历史未丢失**（这一步本来就不需要做，但若已误执行，恢复命令是）：

```bash
# 若已误删，恢复：
git checkout HEAD -- .
# 确认121 个文件仍在索引：
git ls-files | grep -c '^"learning/'   # 应回到 121
git status --porcelain                  # 应为空
```

---

## 5. 决策点与风险

### 5.1 需要用户拍板的 9 个开放问题

| #       | 问题                                      | 推荐选项                                                                                                      | 理由                                                                                                                                                                                                                                                                                                       | 影响面             |
| ------- | --------------------------------------- | --------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
| **D1**  | 是否确认撤销"清理 121 个幽灵路径"？                   | ✅ **撤销**，改为 `git config core.quotepath false`                                                             | 已实测 121 个文件全部存在于磁盘（F1）                                                                                                                                                                                                                                                                                   | 阶段一 A1          |
| **D2**  | `dataset/` 692 MB 二进制怎么处理？              | ⚠️ **未执行**（前提已失效，见附录 C.2/C.3）<br>原建议：**整体移出 git**：`git rm -r --cached dataset` + `.gitignore` 加 `dataset/**` +保留 6 个下载/预处理 `.py` 脚本受跟踪         | ① 692 MB 明文二进制（且 LFS 规则因文件在规则前提交而**未生效**）拖慢每次 clone；② `dataset/sift/` 645 MB 是 SIFT1M 公开数据集，可用现成的 `dataset/download_sift1m.py` 重新获取；③ `.gitattributes` 的 LFS 规则形同虚设                                                                                                                                      | 阶段一 A7/ B4a     |
| **D3**  | `web/`（35 文件，Vite+React+Tailwind）生死？    | **删除**（`git rm -r web`）                                                                                   | ① `AGENTS.md:77-90` 明确声明前端已迁移到 `apps/`，`engine/apps/web/` 已删；② 实测**无任何 CMakeLists/.cmake 引用它**；③ `.archive/engineering-apps-web-migrated-srcs-20261022.tar.gz`（4.1 MB）已归档旧 Taro 源码；④ 与 `apps/`（小程序）功能定位重复                                                                             | 阶段一 A5          |
| **D3'** | **归档 tar.gz 是否也删？**（§0.5.3 新增）          | ✅ **删，不迁移到 `archive/`**                                                                                   | ① 内容是**已被删除的** `engine/apps/web/` 旧 Taro 源，git 历史里每个 commit 都能取回，tar.gz 只是把历史又抄一遍；② 二进制入库无法 diff/review，且不匹配 `.gitattributes` 的 LFS 规则；③ 与 `dataset/` 692 MB 明文二进制（F6）是同一个"用二进制做归档"的既有习惯问题，重构不应复制                                                                                                   | 阶段一 A5          |
| **D4**  | `vendor/third_part/`+ `cmake/` 是否合成 `vendor/`？ | ✅ 合成                                                                                                      | 二者都是"构建期第三方"，与源码/笔记/数据集性质不同；合成后根目录从 21 项降到 12 项                                                                                                                                                                                                                                                          | 阶段二 B4          |
| **D5**  | `learning/` 如何处置？                       | 🔴 **改为切出独立仓**（推翻初版"保持原名仅做内部重排"）                                                                          | ① 活跃度 5 commit/90 天 vs `engine/` 480 —— **差 86 倍**，文件/活跃度比 **432:1** 是全仓最极端失配；② 占全仓文件 33%（2158/5730）却近乎冻结；③ 与 `reference/`（13 commit）同属静态素材，只是 reference 更大所以更显眼。**硬前置**：需先完成 T04 的解耦 3 步（依赖已实测可归零）                                                                                                 | 阶段四 T07         |
| **D6**  | `engine/` 是否改名 `engine/`？          | ✅ 改名                                                                                                      | ① `engineering` 语义偏"工程管理"，实际是"产品代码"；② 与 `.github/workflows/` 的 6 处构建命令、741 个文件的引用相比，改名成本可控（sed 为主）；③ 改名后 `engine/`+`docs/` 双主干清晰                                                                                                                                                                         | 阶段二 B1          |
| **D7**  | `docs/` 是否拆散进 `engine/`？                | 🔴 **保持独立顶层，不拆散**（推翻初版 R1-R4）                                                                             | ① `docs/` 282 commit 紧跟 `engine/` 480，是**全仓第二主干**，而现有 `engine/docs/` 只有 7 commit —— 拆进去等于把第二主干降级为第三主干的附属；② `docs/db-wiki` 单项 161 commit 且**全部集中在 2026-10（最近 3 周）**，有专职门禁 `scripts/check-wiki-refs.py` 做 3437 条行号级引用校验，是正在建设的质量资产而非归档材料；③ `docs/` 与 `reference/` 是强耦合姊妹资产（12 个模型分类几乎一一对应），应同仓进出 | 阶段二 B7（重整内部结构）  |
| **D8**  | `reference/` 独立仓是否**立即**执行？             | ⏸️ **延后到阶段二完成后 ≥1 周**，且**必须与 `docs/wiki/` 同步迁出**                                                          | ① 涉及 95 个 submodule 重挂 + 103 个 wiki 文件，是全流程唯一不可用 `git reset` 完全回滚的操作；② 收益（25 GB）虽大但不紧急；③ ⚠️ **若只切 reference 不切 wiki，3437 条行号引用失去被引对象，`check-wiki-refs.py` 模式 A 全面失效，而模式 B 兜底清单只列了 10 个文件，59/69 个 db-wiki 文件无兜底**                                                                                         | 阶段三 T06         |
| **D9**  | 新仓的归属与命名？                               | `github.com/yinchengwang/book-reference` + `github.com/yinchengwang/book-learning`，均 `main` 分支，与主仓同 owner | 保持单一 owner；命名与主仓 `book` 呼应                                                                                                                                                                                                                                                                               | 阶段三 C2 / 阶段四 D2 |

### 5.2 各阶段失败兜底

| 阶段    | 失败点                                                      | 兜底                                                                                                                 |
| ----- | -------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| **一** | A6 合并 `tests/` 后构建失败                                     | `git reset --hard HEAD~1`（18 个文件从未被任何 CMake 引用，删除它们不影响构建—— 但**保留文件**才是关键，reset 即可完全恢复）                             |
| **一** | A7 新 `.gitignore` 误忽略应跟踪文件                               | `git add -f <path>` 逐个补救；`git status` 会明确列出被忽略的新文件                                                                 |
| **二** | B1~B8 任一 `git mv` 后构建失败                                  | `git reset --hard HEAD~1` + `git clean -fd`（build/ 已被忽略，安全）                                                        |
| **二** | B9 路径修正遗漏（表现为 CMake configure 失败）                        | 错误信息直接给出缺失路径，逐条补。**兜底**：`ln -s engine engineering` 临时软链（仅本地，不提交），先让 CI 绿，再慢慢修                                      |
| **二** | 阶段二一次性commit 太大难以 review/回滚                              | 建议**拆成 3 个 commit**：① 纯 `git mv`（B1~B8）；② 构建修正（B9 的 CMake/CI/脚本）；③ 文档修正（B9 的 md/README）。每个都可独立 `git revert`        |
| **三** | C2/C3 新仓初始化失败                                            | `rm -rf reference/.git` + `git reset --hard stage2-complete`                                                       |
| **三** | C4 之后 `git submodule status` 与 `/tmp/sub-before.txt` 不一致 | `git reset --hard stage2-complete` + `git submodule sync --recursive` + `git submodule update --init --recursive`  |
| **三** | C6 clone 验证失败（大仓/配额/LFS）                                 | 远端 tag `stage2-complete` 已在，随时 `git reset --hard stage2-complete` + `git submodule update --init --recursive` 全量恢复 |

---

## 6. 待明确事项（需要回问用户）

| #      | 问题                                                                                                                           | 为什么必须问                                                                                                                                                                               |
| ------ | ---------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Q1** | 简报中的"121 个幽灵路径"结论从何而来？是否曾发生过实际的 git 异常（文件丢失、checkout 报错）？                                                                    | 我的查证证明这 121 个文件完好。但用户的动机可能来自一次真实的困扰（也许是中文文件名在某个工具里显示异常）。若确有具体症状，需知道是什么才能对症下药                                                                                                         |
| **Q2** | `dataset/` 的 692 MB 数据（SIFT1M 等）是否可重新获取？是否愿意接受"新clone 的人需要跑 `python dataset/download_sift1m.py`"？                            | 决定 D2。若数据不可复现或用户不愿改变使用习惯，D2 需改为"用 `git lfs migrate import` 补做 LFS 转换"（但这会重写全部历史，风险和耗时都极高）                                                                                            |
| **Q3** | `learning/scaffold/`（1554 文件，占 learning 的 72%）未来定位是什么？                                                                       | 它是"每个练习一个 Makefile + NOTES.md + README.md + main.c"的脚手架群。是否要拆成独立 `book-practice` 仓？还是保持在主仓但整体 gitignore（因为是过程性材料）？                                                                   |
| **Q4** | `.claude/projects/D--code-book/memory/` 的 20 个 md 是有价值的项目记忆。是否要迁到 `docs/` 下正式化（如 `docs/memory/`）？还是继续作为 Agent 私有记忆留在 `.ai/`？ | 关系到 D7 的文档体系完整性                                                                                                                                                                      |
| **Q5** | `openspec/changes/archive/`（121 文件，已跟踪但被 gitignore）—— 是否要真正归档到 `archive/openspec/`？                                          | `.gitignore:115` 规则对已跟踪文件无效，这是个既存的规则失效。建议顺手修正                                                                                                                                        |
| **Q6** | 是否接受"阶段二拆成 3 个 commit"以及"每阶段打 tag"的流程？                                                                                       | 829 commits 的仓库，一次性大改若无 tag 保护，回滚成本高                                                                                                                                                 |
| **Q7** | `engine/tests/` 的 18 个 `.c`（5117 行）合并后，是否要**真正接入 CMake** 使其成为受 CI 保护的测试？                                                | 我在 A6 设计里假设接入（`add_project_test`）。但这 18 个测试从未编译过，**首次接入很可能编译失败**（`.superpowers/sdd/task-4-report.md:25` 记载它们是"手工 .o 的暂存目录"）。是否接受 A6 拆成两步：先合并文件（零风险），再单独一个 commit 尝试接入 CMake（可失败可回滚）？ |

---

## 附录 A：任务分解（7 个任务，按依赖排序；已按 §0.5 活跃度结论调整）

> **相对初版的主要变化**：T03 大幅缩减（`learning/`、`docs/wiki/` 不再搬）、T04 新增「解耦 3 步」与守卫删除、新增 T06（阶段三含 wiki 迁移）与 T07（阶段四）。

| Task    | 名称                                                                                                                        | 源文件                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            | 依赖                          | 优先级    |
| ------- | ------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------- | ------ |
| **T01** | **基线固定与事实校正**                                                                                                             | `AGENTS.md`(追加路径排查守则)、`CLAUDE.md`(同)、`.git/config`(core.quotepath)                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | —                           | **P0** |
| **T02** | **阶段一清创**：删空壳目录 + 删 `engine/build/` + **删 `web/` 与归档 tar.gz** + 合并 `engine/tests/` + 重写 `.gitignore` + 收敛 AI 目录 | `and/` `cd/` `Claw/` `2026-10-21-23-48-17/`(删)、`engine/build/`(删)、`web/`(删)、`.archive/engineering-apps-web-migrated-srcs-20261022.tar.gz`(删)、`.archive/`(删)、`engine/tests/*.c` → `engine/test/db/storage/legacy/`、`engine/test/db/storage/CMakeLists.txt`、`.gitignore`、`.claude/`→`.ai/.claude/`、`.agents/`→`.ai/.agents/`、`.opencode/`→`.ai/.opencode/`、`.superpowers/`→`archive/`、`.vscode/`→`.ai/.vscode/`、`.playwright-mcp/`(清空)                                                                                       | T01                         | **P0** |
| **T03** | **阶段二 A：`git mv` 重排**（缩减版 —— **不含 `learning/`、不含 `docs/wiki/` 外迁**）                                                       | `engine/`→`engine/`、`vendor/third_part/`→`vendor/third_part/`、`cmake/`→`vendor/cmake/`、`apps/`→`apps/`、`archive/`+`openspec/changes/archive/`→`archive/`、`docs/{db,agent,rag}-wiki/`→`docs/wiki/`（**仍在 docs 体系内**）、`docs/process/`→`docs/process/`、`docs/{architecture,index}/`→`docs/arch/`、`docs/diagrams/`(不动)、`docs/` 顶层 19 个散 md→`docs/storage/`、`openspec/`→`openspec/`                                                                                                                                             | T02                         | **P0** |
| **T04** | **阶段二 B：解耦 3 步 + 构建路径修正**（**新增解耦，为 T07 硬前置**）                                                                             | **解耦 3 步**：`engine/src/db/api/CMakeLists.txt:7`(find_path HINTS → `vendor/third_part/uthash/src`)、`engine/src/algo-prod/CMakeLists.txt:16`(删死 include)、`engine/src/db/storage/graph/CMakeLists.txt:19` + `engine/test/db/graph/CMakeLists.txt:25`(修不存在路径)。**改名**：根 `CMakeLists.txt`、`engine/CMakeLists.txt`、`engine/test/db/executor/CMakeLists.txt`、`engine/scripts/**`(7)、`scripts/verify_migration.sh`、`.github/workflows/{ci,asan}.yml`(约 25 处)、`.gitmodules`(4 条非 reference)。**删除**：`cmake/dual-track-guard.cmake` 整个文件 | T03                         | **P0** |
| **T05** | **阶段二 C：文档与配置修正** + 打 `stage2-complete` tag                                                                               | `README.md`、`AGENTS.md`、`CLAUDE.md`、`engine/README.md`、`engine/docs/coverage-policy.md`、`docs/{analysis,arch,storage}/**`、`.ai/.vscode/{tasks,launch}.json`                                                                                                                                                                                                                                                                                                                                                                                    | T04                         | **P1** |
| **T06** | **阶段三：切出 `reference/` + `docs/wiki/`**（延后 ≥1 周；C1~C8 见 §2，**新增 C0：把 `docs/wiki/` 纳入迁出范围**）                                | `.gitmodules`(删 95 stanza)、`reference/{README.md,catalog.yml,AUDIT.md,fetch-manifest.txt}`、`docs/wiki/**`(103 文件)、`scripts/check-wiki-refs.py`(改 `--repo-root` 与 `--submodules` 映射)、主仓 `.gitignore`                                                                                                                                                                                                                                                                                                                                            | T05 + tag `stage2-complete` | **P1** |
| **T07** | **阶段四：切出 `learning/`**（须 T04 解耦已完成）                                                                                       | `learning/**`(2158 文件)、`.gitignore`、根 `CMakeLists.txt`(删 `LEARNING_BUILD`)、`.github/workflows/ci.yml`(删 2 个 learning job)                                                                                                                                                                                                                                                                                                                                                                                                                      | T04 + T05                   | **P2** |

**依赖图**：

```
T01 → T02 → T03 → T04 → T05 → [tag stage2-complete] → T06（阶段三，延后≥1 周）
                                └──────────────────────→ T07（阶段四，可与 T06 并行）
```

**T05 完成后打 `stage2-complete` tag 并推送** —— 这是 T06 的前置条件。  
**T04 完成后即满足 T07 的解耦前置条件**（`engine/` 不再引用 `learning/include`），所以 T07 只等 T05 的文档修正完成即可启动。

---

## 附录 B：验证命令速查（已按 §0.5 调整）

```bash
# ===== 阶段一 出口（T02后）=====
cmake -B build/engineering -S engineering -DBUILD_TESTING=ON && \
  cmake --build build/engineering --parallel 4 && \
  ctest --test-dir build/engineering --output-on-failure
git status --porcelain        # 期望：仅 5 个 submodule 指针
git ls-files | grep -c '^"'   # 期望 0（T01 已设 core.quotepath=false）

# ===== 阶段二 出口（T05 后）=====
# 2.1 工程轨可构建 + 测试全跑
cmake -B build/engineering -S engine -DBUILD_TESTING=ON
cmake --build build/engineering --parallel 4
ctest --test-dir build/engineering --output-on-failure

# 2.2 根调度器（learning 仍在主仓时）
cmake -B build/root -S . -DENGINEERING_BUILD=ON -DLEARNING_BUILD=ON

# 2.3 ★解耦验证（T07 的硬前置）：engineering 不再引用 learning/include
grep -rn 'learning/include' engine/ --include=CMakeLists.txt   # 期望 0 命中
grep UTHASH_INCLUDE_DIR build/engineering/CMakeCache.txt       # 期望指向 vendor/third_part/uthash/src

# 2.4 无残留旧路径
git grep -nE '(^|[^a-zA-Z_/.-])(engineering|learning)/' -- ':!reference' ':!openspec' ':!archive' \
  | grep -vE '历史|已迁移|原 engineering|原 learning' | wc -l   # 期望 0

# ===== 阶段三 出口（T06 后）=====
git submodule status | wc -l          # 期望 4
du -sh .git                           # 期望 ~1.1 GB（原 26 GB）
git ls-files | wc -l                  # 期望 ~3467（5730 - 536k? 不含 submodule 内容，见注）
# 新仓 clone 验证（含 wiki 门禁）
git clone <new-repo> && cd book-reference && git submodule update --init --recursive --depth 1
cd book-reference && python scripts/check-wiki-refs.py --mode-b --repo-root .   # 门禁可用

# ===== 阶段四 出口（T07 后）=====
git ls-files | wc -l                  # 期望 ~3572（5730 - 2158）
git ls-files learning | wc -l         # 期望 0
# learning 独立仓自构建验证
git clone <new-learning-repo> && cd book-learning && \
  cmake -B build -S . -DBUILD_TESTING=ON && cmake --build build --parallel 4
# 主仓 CI 只剩 4 个 job
grep -c 'name: ' .github/workflows/ci.yml
```


**文档结束。** 全部结论基于对 `C:\code\book` 的只读探查，未执行任何写操作。

---

## 附录 C：事后更正（2026-10-25 实测）

重构完成后复核，发现本文档有两处结论与实测不符，特此更正。**结论本身错了，但推导过程值得保留**——两处都是同一类错误：**把磁盘工作副本当成了 git 里的内容**。

### C.1 F6「LFS 规则形同虚设」是错的 —— LFS 一直正常工作

**原文断言**：`dataset/**` 的 LFS 规则未生效，692 MB 明文二进制进了 git 历史。

**实测反驳**：

```console
$ git cat-file -s $(git rev-parse HEAD:dataset/sift/base.bin)
134
$ git cat-file -p HEAD:dataset/sift/base.bin | head -3
version https://git-lfs.github.com/spec/v1
oid sha256:158e703367356eb9fa812475fd99c278479ab35b83a2ef5b372fe6ff7bd1e763
size 327271936

$ git ls-tree -r -l HEAD -- dataset | awk '{s+=$4} END {print s/1048576" MB"}'
13.6 MB
```

`base.bin` 在 git 里是 **134 字节的 LFS 指针**，整个 `dataset/` 在 HEAD 中只有 **13.6 MB**。

**错在哪**：当时用 `head -c 100 dataset/sift/base.bin` 检查文件头，看到的是真实二进制——但那是 **LFS smudge 之后的工作副本**，不是 git 里存的 blob。正确做法是查 `git cat-file`，或看 `git ls-tree -l` 的大小列。

**`.git` 1.7 GB 的真实构成**（供参考）：

| 部分 | 体积 | 说明 |
|---|---|---|
| `.git/objects` | 1.1 GB | 860 commit 的正常累积，最大单 blob 仅 8.8 MB（`sqlite3.c`） |
| `.git/lfs` | 673 MB | 19 个 LFS 对象 = dataset 二进制 |
| `.git/modules` | 16 MB | 剩余两个 submodule |

历史里**没有**可清理的 692 MB 垃圾（>1 MB 的 blob 只有 3 个、合计 13.6 MB），所以**历史重写没有收益**，未执行。

### C.2 D2 的前提「可用 `download_sift1m.py` 重新获取」不成立 —— 下载源已失效

**原文断言**：D2 建议 `git rm -r --cached dataset`，理由是数据可用现成脚本重取。

**实测**：

| 源 | 结果 |
|---|---|
| `ftp://ftp.irisa.fr/pub/texmex/corpus/`（脚本现用） | ❌ `curl: (9) Server denied you to change to the given directory` |
| `http://corpus-texmex.irisa.fr/` | ❌ 403 / `Empty reply from server` |
| `https://corpus-texmex.irisa.fr/` | ❌ SSL/TLS handshake failed |
| `http://ann-benchmarks.com/sift-128-euclidean.hdf5` | ✅ HTTP 200，525 MB，HDF5 魔数 `89 48 44 46` 校验通过 |

`dataset/download_sift1m.py` **已彻底失效**，且脚本没有内容校验——下载失败会把 HTML 错误页直接存成目标文件。仓库里那 4 个 196 字节的「404 页面伪装成二进制」就是这么产生的（已于 commit `f4d8e9ac6` 删除）：

```
cmocka.tar.xz
dataset/sift/sift_groundtruth.ivecs
dataset/sift/sift_query.fvecs
dataset/sift/sift_query.fvecs.gz
```

**结论**：dataset **可以**重新获取，但必须走 HDF5 路线（ann-benchmarks 下载 + h5py 转换），现有脚本一行都用不上。本地有 `h5py 3.12.1`。

### C.3 因此 D2 未执行（2026-10-25 决定）

用户决定**暂不移出 dataset**。理由：恢复路径需要先写一个新的 HDF5 转换脚本并验证能还原出相同数据，否则「可恢复」只是理论上的。

若将来要移出，正确顺序是：

1. 写 `dataset/download_sift1m_hdf5.py`（下载 HDF5 → 转 fvecs/ivecs）
2. 转换后与现有 `dataset/sift/fvecs/` 逐字节比对，证明可还原
3. 再 `git rm -r --cached dataset` + `.gitignore`（保留 9 个 `.py`/`.c` 脚本）

### C.4 顺带提醒：下载脚本应加内容校验

本次 4 个 404 文件的根因是「下载失败但文件照写」。任何数据下载脚本都应校验内容，例如：

- fvecs/ivecs：首 4 字节是维度值（SIFT 为 128），且文件大小应为 `4 + 4*dim` 的整数倍
- gzip：前 2 字节应为 `1f 8b`
- 通用兜底：文件大小 < 1 KB 的「二进制」文件一律可疑（404 页面固定 196 字节）
