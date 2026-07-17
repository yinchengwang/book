# repo-root-build-slimming

## Why

Phase 1 (`repo-root-slimming-phase1`) 已明确"编译产物进入 `build/<项目或轨道>/`"的纪律，并把工程轨/学习轨的构建目录迁移到 `build/engineering/` 和 `build/learning/`。但根 `CMakeLists.txt` 调度项目的违规构建残留（`cmake -B build` 错误地把根入口构建到 `build/` 而非 `build/root/`）仍存在：

- `build/CMakeCache.txt` 显示 `CMAKE_HOME_DIRECTORY=C:/code/book`（根入口构项目）
- `build/lib/`、`build/bin/`、`build/third_part/` 都是该根入口项目的子输出（~95MB）
- `build/Makefile`、`build/build.ninja`、`build/CMakeFiles/`(顶层) 都是该根入口的构建脚本
- `build/Testing/`(顶层) 是 CTest 运行时残留
- `build/compile_commands.json` 是 clangd 生成的，应该属于具体轨道而不是 `build/` 根

这些违规产物让 `build/` 看起来像"混合产物目录"，违反双轨清晰边界。同时 `build/engineering/test/` 内有多个 `test_db_init_*` 时间戳目录（gtest 测试夹具残留），每个 4KB 但累积 30+ 个空目录文件。

## What Changes

- **删除 `build/` 顶层违规根入口产物**：`CMakeCache.txt`、`Makefile`、`build.ninja`、`CMakeFiles/`(顶层)、`.cmake/`、`Testing/`、`cmake_install.cmake`、`bin/`、`lib/`、`third_part/`、`compile_commands.json`（11 个条目，共约 28MB）
- **保留**：`build/engineering/`、`build/learning/`、`build/root/`（Phase 1 规范构建目录）
- **保留** 任何未来若用户需要在 `build/engineering/` 下手动创建 `compile_commands.json` 的能力（Phase 1 工程轨 CMake 已正确处理）
- **清理 gtest 测试夹具残留**：`build/engineering/test/test_db_init_*/` 多个空目录（30+ 个，每个 4KB）

## Capabilities

### Modified Capabilities

- `build-test-artifacts`: 明确"根入口构建必须用 `cmake -B build/root -S .`"的使用约束，违规的 `cmake -B build` 不应在文档或脚本中推荐。

## Impact

- 影响用户路径习惯：以后需要根入口构建时，必须用 `cmake -B build/root -S .` 而非 `cmake -B build`
- 影响磁盘空间：清理约 28MB（主要是 `build/lib/`、`build/third_part/` 的旧 `.a` 和 `cjson_build`/`googletest_build` 子产物）；清理 30+ 个空目录
- 不影响工程轨/学习轨/根入口的实际构建产物（`build/engineering/`、`build/learning/`、`build/root/` 已存在）
- 不影响 git 跟踪内容（`build/` 整个被 `.gitignore` 忽略）
- 可逆性高：所有内容可由 `cmake -B build/<轨道> -S <轨道>` 重建