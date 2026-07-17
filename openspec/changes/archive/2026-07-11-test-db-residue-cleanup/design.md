# test-db-residue-cleanup — Design

## Context

Phase 1 的 build-test-artifacts spec 规定：

> 测试/运行产物统一使用 `test-results/<项目或轨道>/`

但工程轨存在历史测试代码（含 `engineering/test/db/storage/storage_*.cpp` 等）就地把数据库写到工作目录（默认 `build/engineering/test/db/storage/` 或更早时期的 `engineering/test/`），加上部分测试中断或缺 tearDown，留下 ~14 个 `test_kv.db` 等文件。

其中 B 类（位于 `engineering/test/`、`engineering/test/db/`）在源码目录里——这暴露测试代码本身的产物落点问题，必须通过"测试代码产物落点改造"独立变更解决。本变更**仅清残留**，不修源代码。

## Goals / Non-Goals

**Goals:**

- 一次性清除全部 14 个 test_*.db / test_*.wal / test_db.bin 文件 ~108KB
- 保留 `.gitignore` 防御规则（已添加）
- 不影响后续 ctest

**Non-Goals:**

- 不修改任何测试代码（属独立变更）
- 不修改 CMake/CTest 配置（属独立变更）
- 不重命名或迁移目录结构
- 不清理 `build/engineering/test/db/{storage,...}/` 里 future 一致性需要的 `data` 目录

## Decisions

### 1. 一刀切删除全部 14 个文件

依据：均 untracked，均为测试运行时产物，均可由 `ctest --test-dir build/engineering` 在毫秒级重建。

判定方式：`git ls-files <path>` 返回空 = untracked，确认可删。

### 2. 不修源代码

测试代码在 `test_kv_test.cpp` / `storage_kv_test.cpp` 等文件中硬编码了相对路径（如 `"test_kv.db"`），导致测试运行时在工作目录生成这些文件。

**完整修复路径**（不在本变更范围）：

1. 测试代码改用绝对路径或 `<git-root>/test-results/engineering/` 路径
2. CMake/CTest 设置 `add_test(NAME ... WORKING_DIRECTORY test-results/engineering)`
3. 测试 fixture 添加 `TearDown()` 调用 `std::remove(...)` 清理

### 3. 加 .gitignore 防御（已完成）

前几次 `repo-root-runtime-cleanup` 和 `build-slimming` 变更已添加：

- `test_*.db`
- `test_*.db.wal`
- `test_*.wal`
- `test_*.bin`
- `test_db.bin`（被 `test_*.bin` 覆盖）

无需新增规则。

## Risks / Trade-offs

- [误删非产物的 test_*.db] → 不存在：grep 确认仓库内只有测试代码提到 `test_kv.db` 等文件名（fixture 写或测试运行生成），无源代码引用它们作为输入
- [下次 ctest 失败] → 不会：测试代码自身负责创建这些 db，下游 setUp 会重建
- [潜在挂载的并发测试残留] → 当前未运行 ctest，磁盘无活跃句柄

## Migration Plan

1. 验证所有 14 个目标文件 untracked（已确认）
2. 一次性删除
3. git status 确认无 tracked 变动
4. 一个 commit 提交