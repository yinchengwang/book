# eng-rag-build-cleanup-and-docs-rag-empty — Design

## Context

Phase 1 已统一构建产物目录为 `build/<项目或轨道>/`，并移除了根 `CMakeLists.txt` 的违规构建。但 `engineering/rag/` 这层仍保留了一个违规的 `build/` 子目录——不在 Phase 1 规范的 `build/engineering/rag/` 路径下，而是嵌套在 `engineering/rag/build/` 中，违反"构建产物集中在仓库顶层 build/"的约定。

`docs/rag/` 是早期 RAG 文档规划的占位目录，只有 `diagrams/` 一个空子目录。实际 RAG 文档已存在于 `engineering/rag/docs/` 中（232KB），共享 docs 不应再重复保留空目录。

## Goals / Non-Goals

**Goals:**

- 删除 RAG 工程的违规内嵌构建（31MB）
- 删除空的 `docs/rag/` 占位目录
- 不影响 `engineering/src/db/index/`（真实索引源码）
- 不影响 `archive/distributed/`（Phase 9 归档源码）

**Non-Goals:**

- 不重新组织 `engineering/rag/` 的构建集成（应让工程轨 CMake `add_subdirectory` 接管，是另一独立变更）
- 不迁移 `docs/rag/` 内容（目录为空，无内容）
- 不清理任何 `engineering/src/db/index/` 子目录（10 个索引类型源代码继续用）

## Decisions

### 1. 仅删两个 untracked 目录

只删 `engineering/rag/build/` 和 `docs/rag/`，二者均 git untracked。`engineering/src/db/index/` 和 `archive/distributed/` 是真实源码，不在本变更范围。

### 2. 不修复 RAG 构建集成

`engineering/rag/build/` 的存在表明用户曾本地构建过 RAG。下次构建应通过工程轨 `cmake -B build/engineering -S engineering` 自动 `add_subdirectory(rag)` 重建到 `build/engineering/rag/`。

将 RAG 集成到工程轨 CMake 是独立变更，本变更仅清违规残留。

## Risks / Trade-offs

- [删 build 后用户重新 cmake 失败] → RAG 已通过 `engineering/CMakeLists.txt` 的 `add_subdirectory(rag)` 集成（验证 `engineering/rag/CMakeLists.txt` 存在），工程轨 cmake 重建会自动包含 RAG
- [删 docs/rag/ 后用户误用旧路径] → 唯一的引用是兼容入口 README（如果存在），但当前 `docs/rag/` 是空目录，无任何引用
- [未来需要 diagrams 占位] → 用户需要时可重新 mkdir，创建成本可忽略

## Migration Plan

1. 删除 `engineering/rag/build/` (31MB)
2. 删除 `docs/rag/` (空目录树)
3. git status 确认无 tracked 变动
4. 一个 commit 提交