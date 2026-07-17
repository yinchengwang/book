# repo-root-runtime-cleanup — Design

## Context

Phase 1 (`repo-root-slimming-phase1`) 在设计阶段已确立"测试/运行产物统一到 `test-results/<项目或轨道>/`"的纪律，CMake/CTest 输出路径已迁移，相关 `.gitignore` 规则（`build/`、`test-results/`）已添加。但部分历史时期在根目录直接写入产物的测试代码（KV、mm_pool、vector、raft、wal、txn 等）尚未修缮，导致根目录持续产生孤立产物。

Phase 2 (`repo-root-slimming-phase2`) 已完成兼容入口删除和文档收口，但未触及根目录的运行时产物残留。

本变更是第三阶段清理：补齐 `.gitignore`，删除现存残留，确保根目录严格符合白名单。

## Goals / Non-Goals

**Goals:**

- 一次性清理根目录所有未跟踪的运行时产物
- 补齐 `.gitignore` 防止再次产生
- 不影响 git 跟踪的真实内容（`kanban-c-mode.png` 等）
- 保持工程轨、学习轨、根入口三套构建和 CTest 通过

**Non-Goals:**

- 不修改任何测试代码（运行时产物落点的代码改造属于另一变更）
- 不归档任何 git 跟踪的文件
- 不重构 CMake/CTest 输出路径（Phase 1 已完成）

## Decisions

### 1. 仅删除未跟踪的运行时产物

Phase 2 已明确 git 跟踪的产物（如 `kanban-c-mode.png`）是真实资产，不在本变更范围。

**判定标准**：
- `git ls-files <path>` 不返回该文件 → 未跟踪 → 删除
- `git ls-files <path>` 返回该文件 → 跟踪 → 保留

### 2. 补齐 `.gitignore` 防御性规则

新增以下规则（追加到 `.gitignore`）：

```gitignore
# 根目录运行时产物防御（防止历史测试代码写到根目录）
test_*.db
test_*.db.wal
test_*.bin
test_*.wal
test_vector_data/
test_mm_pool_data/
test_raft_state.bin.tmp
test_dir/
build-learning/
build2/
test-summary.json
```

理由：现有 `.gitignore` 已 ignore `build/` 和 `test-results/`，但这些具体产物模式未被覆盖。补充这些规则可以防御未来类似违规。

### 3. 不修改测试代码的产物落点

部分测试代码（KV/vector/mm pool/raft/wal/txn 等）仍把数据写到根目录 `test_*` 文件。这些代码改造属于"测试运行时产物落点规范"变更，需要逐个修缮测试代码并修改 CMake/CTest 设置，不在本变更范围。

本变更仅清理残留 + 加防御，不修测试代码。

## Risks / Trade-offs

- [误删 git 跟踪文件] → 删除前用 `git ls-files` 严格校验，确保只删未跟踪文件
- [其他工具依赖根级 test_* 文件] → 不存在（grep 已确认无脚本引用这些根级产物）
- [删后无法恢复] → 这些都是可重新生成的运行时产物（运行相应测试即可重建）

## Migration Plan

1. 用 `git ls-files` 列出根目录所有"未跟踪但确实存在"的条目
2. 按以下顺序清理（一次性）：
   - 删除根级 `test_*.db`、`test_*.db.wal`、`test_*.bin`、`test_*.wal`
   - 删除 `test_vector_data/`、`test_mm_pool_data/`、`test_raft_state.bin.tmp`、`test_dir/`
   - 删除 `data/` 整个目录
   - 删除 `build-learning/`、`build2/` 旧构建目录
   - 删除 `test-summary.json`
3. 更新 `.gitignore` 添加防御规则
4. 运行 `git status --short` 确认无意外删除 git 跟踪文件
5. 提交变更（一个 commit）