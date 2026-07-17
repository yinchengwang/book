# repo-root-runtime-cleanup

## Why

Phase 1/2 已完成根目录真实业务内容（apps/rag/sdk/notes 等）的双轨收敛与兼容入口删除，但根目录仍残留大量测试运行时产物和旧构建目录（`data/`、`build-learning/`、`build2/`、根级 `test_*.db`、`test_*.wal`、`test_vector_data/`、`test_mm_pool_data/`、`test_raft_state.bin.tmp`、`test_dir/`、`test-summary.json`）。这些产物违反"测试/运行产物必须落在 `test-results/<项目或轨道>/`"的双轨纪律，且污染根目录白名单视图。

## What Changes

- **删除根级未跟踪的测试运行时产物**：`data/` 整个目录、根级 `test_db.bin` 等 `.db/.wal/.bin/.tmp` 文件、`test_vector_data/`、`test_mm_pool_data/`、`test_raft_state.bin.tmp`、`test_dir/`。
- **删除旧构建目录**：`build-learning/`（旧的 `learning/` 轨构建目录，已被 `build/learning/` 取代）和 `build2/`（孤立旧构建）。
- **删除/迁移未跟踪的 `test-summary.json`**：检查后确认是测试运行时生成，删除。
- **保留 git 跟踪的 `kanban-c-mode.png`**：知识库看板图标资产，不删除。
- **更新 `.gitignore`**：确保 `test_*.db`、`test_*.wal`、`test_vector_data/`、`test_mm_pool_data/`、`test_raft_state.bin.tmp`、`test_dir/`、`build-learning/`、`build2/`、`test-summary.json` 全部被忽略，防止再次产生。
- **不动 `build/`、`test-results/`、`engineering/`、`learning/`、`docs/`、`openspec/`、`archive/`、`cmake/`、`scripts/`、`third_part/`、`reference/`、必要配置文件和工具配置目录**。

## Capabilities

### Modified Capabilities

- `build-test-artifacts`: 明确列出需要在根目录 ignore 的运行时产物模式，确保旧约定（数据写到根目录）不再复活。

## Impact

- 影响根目录白名单视图（删除残留后根目录更干净）
- 影响 `.gitignore`（新增 8-10 行 ignore 规则）
- 不影响工程轨、学习轨、根入口构建和测试
- 不影响 git 跟踪的任何真实内容