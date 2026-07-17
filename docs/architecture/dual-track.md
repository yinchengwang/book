# 双轨架构（Dual-Track Architecture）

本仓库采用双轨收敛布局：**engineering** + **learning**，辅以 **reference** 参考轨道和**双轨共享**基础设施。

## 根目录白名单

根目录仅保留：

| 类型 | 内容 |
|---|---|
| 双轨入口 | `engineering/`, `learning/` |
| 共享治理 | `docs/`, `openspec/`, `archive/` |
| 共享构建 | `cmake/`, `scripts/` |
| 第三方/参考 | `third_part/`, `reference/` |
| 产物目录 | `build/`, `test-results/` |
| 配置文件 | `CMakeLists.txt`, `README.md`, `CLAUDE.md`, `AGENTS.md`, `.gitignore`, `.clang-format`, `.gitmodules` |

> 仓库已完成根目录瘦身（`repo-root-slimming-phase1` + `repo-root-slimming-phase2`）。根目录仅保留白名单目录和必要配置文件，所有业务内容均通过 canonical 双轨路径访问。

## 关系图

```
                        ┌─────────────────────────┐
                        │   根 CMakeLists.txt     │
                        │   (双轨调度器)           │
                        └────────────┬────────────┘
                                     │
              ┌──────────────────────┼──────────────────────┐
              │                      │                      │
              ▼                      ▼                      ▼
    ┌──────────────────┐   ┌──────────────────┐   ┌──────────────────┐
    │    learning/     │   │   engineering/   │   │    reference/    │
    │   学习轨道        │   │   工程轨道        │   │  参考资料轨道     │
    │  (LEARNING_BUILD │   │  (ENGINEERING_   │   │  (git submodule, │
    │   默认 OFF)       │   │   BUILD 默认 ON)  │   │   不构建)         │
    └────────┬─────────┘   └────────┬─────────┘   └────────┬─────────┘
             │                       │                       │
             │                       │                       │
   ┌─────────┴─────────┐    ┌────────┴─────────┐    ┌───────┴────────┐
   │ • notes/          │    │ • src/ (db/algo)  │    │ • open-source/ │
   │ • code-solutions/ │    │ • include/        │    │   ├ faiss      │
   │ • ds-c/orig/      │    │ • test/           │    │   ├ redis      │
   │ • algo-c/         │    │ • apps/           │    │   ├ ... (12)   │
   │ • interview/      │    │ • rag/            │    │                │
   │ • playground/     │    │ • sdk/            │    │                │
   │ • tools/          │    │ • tools/          │    │                │
   └───────────────────┘    │ • test_data/      │    └────────────────┘
                            │ • Dockerfile      │
                            └───────────────────┘
                                     │
                                     ▼
                          ┌──────────────────────┐
                          │   双轨共享            │
                          │ • docs/              │
                          │ • scripts/           │
                          │ • openspec/          │
                          │ • cmake/             │
                          │ • third_part/        │
                          │ • build/             │
                          │ • test-results/      │
                          └──────────────────────┘
```

## 跨轨边界（铁律）

**`engineering/` 与 `learning/` 之间不允许任何代码引用**：

| 方向 | 禁止内容 |
|---|---|
| engineering → learning | `add_subdirectory(learning)`、`target_link_libraries(... learning_target)`、`target_include_directories(... learning/include)` |
| learning → engineering | 同上（反向） |

由 `cmake/dual-track-guard.cmake` 在 CMake configure 阶段自动检查：
- 检测到违规 → 输出 `FATAL_ERROR` 并指出违规文件
- 根调度器（`CMakeLists.txt`）跳过检查，因为它必须 add_subdirectory 两个轨道

## 资产归属

### 工程轨 canonical 资产

`engineering/` 是以下内容的唯一 canonical 位置：

- `apps/`、`rag/`、`sdk/`、Docker 部署文件
- 工程头文件（已在 `engineering/include/`）
- 工程测试夹具（`engineering/test_data/` 或 `engineering/test/fixtures/`）
- 工程专用工具（`engineering/tools/`）

### 学习轨 canonical 资产

`learning/` 是以下内容的唯一 canonical 位置：

- `interview/`（面试资料）
- `notes/`（学习笔记）
- `playground/`（演示代码：demo/demo_code/demo_projects）
- `arkts/`、`blog/`、`exam/`
- `articles/`（CSDN 文章草稿）
- `misc/`（杂项学习文件）
- 学习专用工具（`learning/tools/`）

### 共享工具

顶层 `tools/` 已经按用途拆分：
- 全仓库治理/文档/迁移工具 → `scripts/` 或文档脚本区
- 工程专用工具 → `engineering/tools/`
- 学习专用工具 → `learning/tools/`

## reference 轨道

`reference/open-source/` 由 12 个 git submodule 组成：

| Submodule | 用途 |
|---|---|
| faiss | Facebook 向量检索 |
| redis | Redis 内存数据库 |
| postgres | PostgreSQL RDBMS |
| pgvector | PG 向量扩展 |
| sqlite3 | SQLite |
| elasticsearch | ES 搜索引擎 |
| chroma | Chroma 向量数据库 |
| milvus | Milvus 向量数据库 |
| ann-benchmarks | ANN 基准测试 |
| mysql | MySQL |
| neo4j | Neo4j 图数据库 |
| openGauss | openGauss 数据库 |

**reference 不参与构建** —— 它是只读参考材料，用于学习开源实现。

## 编译与测试产物

所有编译产物进入 `build/<项目或轨道>/`，所有测试/运行产物（日志、覆盖率、临时数据库）进入 `test-results/<项目或轨道>/`。源码目录不再产生构建二进制或运行时产物。

## 双轨构建命令

```bash
# 仅工程轨道（默认）
cmake -B build/engineering -S engineering -DBUILD_TESTING=ON
cmake --build build/engineering --parallel 4
ctest --test-dir build/engineering --output-on-failure

# 仅学习轨道
cmake -B build/learning -S learning -DBUILD_TESTING=ON
cmake --build build/learning --parallel 4
ctest --test-dir build/learning --output-on-failure

# 双轨同时
cmake -B build/root -S . -DENGINEERING_BUILD=ON -DLEARNING_BUILD=ON
cmake --build build/root --parallel 4

# 覆盖率（仅工程轨道）
cmake -B build/engineering -S engineering -DENABLE_COVERAGE=ON
```

## OpenSpec 治理

三轨之间的演进由 OpenSpec 流程管理：
- 每个变更在 `openspec/changes/<name>/` 下创建 proposal.md / design.md / tasks.md / specs/
- 双轨变更都走 OpenSpec，无例外
- 归档时移到 `openspec/changes/archive/<date>-<name>/`

当前 active 变更：详见 `openspec/changes/` 下的目录列表。

详见 [AGENTS.md](../AGENTS.md) 与 [CLAUDE.md](../CLAUDE.md)。

## 目录迁移兼容策略

### Phase 1（已完成）

- 真实内容已迁入 `engineering/` 或 `learning/` canonical 位置
- 旧顶层路径仅保留 README 指路（标记为 `[DEPRECATED: Phase 2 删除]`）
- 不保留真实代码双副本

### Phase 2（已完成）

- 已删除所有旧顶层兼容 README 和包装入口
- 根目录达成共享区白名单：仅 `engineering/`、`learning/`、`docs/`、`openspec/`、`archive/`、`cmake/`、`scripts/`、`third_part/`、`reference/`、必要配置文件和工具配置目录
