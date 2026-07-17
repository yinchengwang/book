# Book — 双轨学习 + 工程仓库

[![CI](https://img.shields.io/badge/CI-passing-brightgreen)](.github/workflows/ci.yml)
[![Dual-Track](https://img.shields.io/badge/dual--track-active-blue)](engineering/CMakeLists.txt)
[![OpenSpec](https://img.shields.io/badge/OpenSpec-19-success)](openspec/changes/archive/)
[![Engineering Tests](https://img.shields.io/badge/eng--tests-88-brightgreen)](engineering/build/CTestTestfile.cmake)
[![Learning Tests](https://img.shields.io/badge/learn--tests-158-brightgreen)](learning/code-solutions/c/test/c/)
[![Coverage](https://img.shields.io/badge/coverage-TBD-lightgrey)](engineering/docs/coverage-policy.md)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

> 完整指标快照：[**docs/project-metrics.json**](docs/project-metrics.json)

## Coverage 报告

CI coverage job 跑完后，会在 `engineering/coverage-html/index.html` 产生 HTML 报告：[**Coverage Report**](engineering/coverage-html/index.html)（仅 Ubuntu + lcov CI 环境生成；Windows 本地用 gcov-only 路径）。

> 运行 `bash engineering/scripts/coverage/run.sh` 本地生成（自动选择 lcov 或 gcov 模式）。

本仓库采用**双轨制**布局，将"学习资料 + 教学代码"与"工程产品 + 生产代码"清晰分离，避免互相污染。

### Coverage 目标（α 工程作品集指标）

- 核心模块（db, algo）lines 覆盖率 ≥ 80%
- 所有工程模块 lines 覆盖率 ≥ 50%
- 每次 PR 自动对比基线，下降 >2% 触发警告
- 详见 [engineering/docs/coverage-policy.md](engineering/docs/coverage-policy.md)

## 项目定位

- **α 工程作品集**：`engineering/` —— PG 风格存储引擎、MiniVecDB、apps/、rag/、sdk/、CI/CD、Docker
- **β 学习日志**：`learning/` —— obsidian 笔记、LeetCode 题解、教学数据结构、面试资料、CSDN 文章
- **参考资料**：`reference/` —— 12 个开源项目源码镜像（git submodule，**不参与主构建**）

## 根目录白名单

根目录长期仅保留以下目录与文件，其他业务/学习/演示/工具真实内容只能存在于对应轨道或共享基础设施中：

| 类型 | 目录 / 文件 |
|---|---|
| 双轨入口 | `engineering/`, `learning/` |
| 共享治理 | `docs/`, `openspec/`, `archive/` |
| 共享构建 | `cmake/`, `scripts/` |
| 第三方/参考 | `third_part/`, `reference/` |
| 配置文件 | `CMakeLists.txt`, `README.md`, `CLAUDE.md`, `AGENTS.md`, `.gitignore`, `.clang-format`, `.gitmodules`, `.gitattributes` |
| 工具配置 | `.github/`, `.claude/`, `.vscode/`, `.agents/`, `.opencode/`, `.remember/`, `.playwright-mcp/` |

根目录已通过 `repo-root-slimming-phase1` + `repo-root-slimming-phase2` 完成瘦身，所有业务内容均通过 canonical 路径（`engineering/` 或 `learning/`）访问。

## 三轨目录树

```
book/
├── learning/                 # 学习轨道（默认不构建）
│   ├── notes/                # obsidian vault，132 个学习笔记
│   ├── code-solutions/       # LeetCode/面试题解 + 分布式实验代码
│   ├── ds-c/orig/            # 手撕数据结构教学版（原 src/ds/）
│   ├── ds-cpp/               # C++ 数据结构教学版
│   ├── algo-c/               # 教学算法
│   ├── interview/            # 面试资料
│   ├── exam/                 # 考试认证
│   ├── blog/                 # 博客草稿
│   ├── arkts/                # 鸿蒙 ArkTS
│   ├── articles/             # CSDN 长文
│   ├── playground/           # demo + demo_code + demo_projects
│   ├── tools/                # 学习专用工具
│   └── misc/                 # kanban-snapshot / lines_of_code / learn-vdb-deepdive-check
│
├── engineering/              # 工程轨道（默认构建）
│   ├── src/                  # 库代码：db/, algo/, cpp/, redis/
│   ├── include/              # 公共头文件
│   ├── test/                 # gtest 测试
│   ├── apps/                 # 独立应用（db_driver, games, tools, web）
│   ├── cmake/                # 工程工具（ProjectUtils.cmake 等）
│   ├── rag/                  # RAG 系统
│   ├── sdk/                  # 多语言 SDK
│   ├── tools/                # 工程专用工具
│   ├── test_data/            # 工程测试夹具
│   ├── Dockerfile + docker-compose.yml
│   └── CMakeLists.txt        # 工程构建入口
│
├── reference/                # 参考资料轨道（不构建）
│   └── open-source/          # 12 个 git submodule（faiss, redis, postgres, ...）
│
├── docs/                     # 双轨共享：架构 + 部署 + API + RAG 文档
├── scripts/                  # 双轨共享：CI 辅助脚本
├── openspec/                 # OpenSpec 变更管理（双轨都强制走）
├── cmake/                    # 双轨守卫：dual-track-guard.cmake
├── third_part/               # 第三方依赖（cjson, googletest, libmicrohttpd）
├── build/                    # 编译产物（build/engineering、build/learning、build/root）
├── test-results/             # 测试/运行产物（logs、coverage、test dbs）
├── learning/CMakeLists.txt   # 学习构建入口
├── CMakeLists.txt            # 根入口（双轨调度器）
├── AGENTS.md / CLAUDE.md     # 治理文档
└── .github/workflows/ci.yml  # CI 配置
```

## 编译与测试产物规范

所有编译产物进入 `build/<项目或轨道>/`，所有测试/运行产物进入 `test-results/<项目或轨道>/`。源码目录不再产生构建二进制、测试数据库、日志或覆盖率输出。

## 快速开始

### 默认构建（仅 engineering 轨道）

```bash
cmake -B build/engineering -S engineering -DBUILD_TESTING=ON
cmake --build build/engineering --parallel 4
ctest --test-dir build/engineering --output-on-failure
```

### 启用学习轨道

```bash
cmake -B build/learning -S learning -DBUILD_TESTING=ON
cmake --build build/learning --parallel 4
ctest --test-dir build/learning --output-on-failure
```

### 同时构建双轨

```bash
cmake -B build/root -S . -DENGINEERING_BUILD=ON -DLEARNING_BUILD=ON
cmake --build build/root --parallel 4
```

## 跨轨边界纪律

**`engineering/` 严禁 `add_subdirectory(learning)` 或链接 learning 任何 target。**
**`learning/` 反之亦然。**

由 `cmake/dual-track-guard.cmake` 在 configure 时自动检查（详见 [docs/architecture/dual-track.md](docs/architecture/dual-track.md)）。

## 目录迁移与兼容策略

本仓库已通过 `repo-root-slimming-phase1` + `repo-root-slimming-phase2` 完成根目录瘦身：

| 阶段 | 范围 | 状态 |
|---|---|---|
| Phase 1 | 迁移真实内容 + 保留旧顶层兼容 README 指路 | ✅ 已完成 |
| Phase 2 | 删除旧顶层兼容入口，达到最终根目录白名单 | ✅ 已完成 |

旧顶层兼容入口已全部删除，长期使用请直接引用 canonical 路径（`engineering/` 或 `learning/`）。

## OpenSpec 变更管理

所有变更（无论学习还是工程）必须走 OpenSpec 流程：
1. 讨论 → 达成共识
2. 在 `openspec/changes/<name>/` 创建 proposal.md / design.md / tasks.md / specs/<capability>/spec.md
3. 按 tasks.md 推进，**每完成一个任务同步更新状态**
4. 归档时移到 `openspec/changes/archive/<date>-<name>/`

当前 active change：
- `repo-root-slimming-phase1` —— 根目录瘦身 Phase 1（迁移 + 兼容）
- `repo-root-slimming-phase2` —— 根目录瘦身 Phase 2（删除兼容入口）

## SQL 执行引擎

实现了生产级 SQL 执行引擎，对标 PostgreSQL 16/17 架构。

### 功能特性

- **Volcano 迭代器模型**：统一算子接口
- **内存管理**：MemoryContext 层级管理
- **表达式求值**：字节码解释器 + JIT 预研
- **核心算子**：SeqScan, IndexScan, HashJoin, NestLoop, Agg, Sort, Limit
- **并行执行**：Worker 线程池 + TupleQueue
- **触发器**：BEFORE/AFTER 触发器链
- **分区路由**：范围/列表/哈希分区

### 性能指标

| 指标 | 结果 |
|------|------|
| SeqScan 100 万行 | < 1s |
| HashJoin 10 万 x 10 万 | < 5s |
| 向量搜索 QPS | > 1000 |
| 并行加速比 | > 3x |

### 文档

- [API 参考](docs/sql-executor/API.md)
- [架构文档](docs/sql-executor/ARCHITECTURE.md)
- [用户手册](docs/sql-executor/USER_GUIDE.md)

### 快速开始

```c
#include "db/sql/sql_driver.h"

int main() {
    void *db = kv_open(":memory:");

    execute_ddl("CREATE TABLE t (id INT, name TEXT)", db);
    execute_sql("INSERT INTO t VALUES (1, 'test')", db);

    QueryResult *result = execute_sql("SELECT * FROM t", db);
    printf("Rows: %d\n", result->nrows);

    FreeQueryResult(result);
    kv_close(db);
    return 0;
}
```

## 治理文档

- [AGENTS.md](AGENTS.md) —— AI 助手操作指南（构建命令、库列表、代码风格、编译器标志）
- [CLAUDE.md](CLAUDE.md) —— Claude Code 项目指令（中文规范、OpenSpec 铁律）
- [docs/architecture/dual-track.md](docs/architecture/dual-track.md) —— 双轨架构图与跨轨纪律
