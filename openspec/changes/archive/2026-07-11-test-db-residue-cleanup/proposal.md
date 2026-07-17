# test-db-residue-cleanup

## Why

工程轨/学习轨测试在运行时会就地生成 test_kv.db / test_txn.db / test_wal.db / test_db.bin 等临时数据库与 bin 文件。Phase 1 已规定"测试/运行产物统一到 `test-results/<项目或轨道>/`"，但当前测试代码尚未改造（属独立"测试代码产物落点改造"变更），导致残留散布在两处：

- A 类（合规位置）：`build/engineering/test/db/storage/test_kv.db` (+ .wal) 共 20KB
- B 类（违规位置）：`engineering/test/`、`engineering/test/db/` 顶层共 12 个文件 ~88KB（含 test_db.bin 1.7MB）

这些文件 untracked（git 跟踪是干净的，仅磁盘残留），下游构建/测试正常运行不需要它们——下次 ctest 会在毫秒级内重建。

## What Changes

- 删除全部 14 个 test_kv.db / test_txn.db / test_wal.db / test_db.bin 等运行时数据库与 bin 产物（约 108KB）
- 已在 `.gitignore` 添加 `test_*.db`、`test_*.db.wal`、`test_*.wal`、`test_*.bin`（前几次 commit 提前完成）
- **不**改动测试代码本身（属独立变更）

## Capabilities

### Modified Capabilities

- `build-test-artifacts`: 明确"测试代码产物落点改造"是独立待办，本变更仅清理磁盘残留并依赖现有 `.gitignore` 防御

## Impact

- 不影响 ctest：下次运行自动重建
- 不影响 git 跟踪：所有目标文件 untracked
- 释放约 108KB（其中 1.7MB 的 test_db.bin 主要是页面测试 fixture bin）
- 不修改任何测试代码或 CMake 配置
- 暴露的根因（测试代码硬编码相对路径）由独立变更解决