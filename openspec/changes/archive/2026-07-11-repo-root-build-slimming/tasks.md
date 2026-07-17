## 1. build/ 顶层违规产物盘点

- [x] 1.1 用 `head -20 build/CMakeCache.txt` 确认 `CMAKE_HOME_DIRECTORY=C:/code/book`（根入口构建）
- [x] 1.2 列出 build/ 顶层所有非规范条目
- [x] 1.3 区分违规条目 vs 规范条目（`build/engineering/`、`build/learning/`、`build/root/` 保留）

## 2. 删除 build/ 顶层违规产物

- [x] 2.1 删除 `build/CMakeCache.txt`（根入口 CMake 缓存）
- [x] 2.2 删除 `build/Makefile`、`build/build.ninja`、`build/cmake_install.cmake`（根入口构建脚本）
- [x] 2.3 删除 `build/CMakeFiles/`（顶层 CMake 内部状态）
- [x] 2.4 删除 `build/.cmake/`（CMake 缓存）
- [x] 2.5 删除 `build/Testing/`（顶层 CTest 残留）
- [x] 2.6 删除 `build/compile_commands.json`（跟随机根入口构建移动，IDE clangd 重新 cmake 即恢复）
- [x] 2.7 删除 `build/bin/`、`build/lib/`（根入口子输出）
- [x] 2.8 删除 `build/third_part/`（根入口子输出，`cjson_build`+`googletest_build`+`libmicrohttpd_build`）

## 3. 清理 build/engineering/test/test_db_init_* 残留

- [x] 3.1 列出 `build/engineering/test/test_db_init_*/` 所有空目录
- [x] 3.2 删除这些 gtest 夹具残留目录（30+ 个，每个 4KB）

## 3.5. 深入 build/ 子目录清理（CTest 测试运行残留）

- [x] 3.5.1 删除 `build/engineering/test_db_init_*/`（6 个 4KB 含内容的目录，PG schema 残留）
- [x] 3.5.2 删除 `build/engineering/Testing/Temporary/` 和 `build/learning/Testing/Temporary/`（CTest 运行时残留：LastTest.log、CTestCostData.txt、LastTestsFailed.log）
- [x] 3.5.3 删除 `engineering/build-cov/` (363MB, lcov 工程副本) + `engineering/coverage-gcov/` (6.4MB, lcov 报告)
- [x] 3.5.4 深入分析 engineering/apps/web/ 1.4GB（三个子项目的 node_modules）：knowledge_hub 724MB + games-mini-program 583MB + reading-radar 41MB

## 4. 验证 + 提交

- [x] 4.1 运行 `git status --short` 确认 build/ 下仅删除未跟踪，无 git 跟踪内容变动
- [x] 4.2 用 `du -sh build/` 验证清理后大小（414MB → 384MB）
- [x] 4.3 提交变更（一个 commit）

## 跳过/延迟记录

无。`build/engineering/`、`build/learning/`、`build/root/` 不在本变更范围（属独立"build 子目录深入瘦身"待办）。

## 已知遗留项

- **`build/root/` 内可能还有上次的 ctest 残留**（Temporary/）：不属违规条目，由后续清理或用户运行 ctest --test-dir build/root 后产生清理
- **`apps/web/reading-radar/` 空目录 DACL 异常**：之前已被告知不在本变更范围
- **`build/engineering/test/db/storage/test_kv.db` (16KB)**：gtest 就地生成的测试数据库，属测试代码产物落点改造范围（独立变更），不在本清理范围
- **`build/engineering/test/db/*/test_*.exe`**：CMake 必重建的测试可执行文件，~30MB × 20+ = 200MB+，是构建系统的合法产物无法清理
- **`engineering/apps/web/{knowledge_hub,games-mini-program,reading-radar}/node_modules/` 共 ~1.35GB**：第三方 npm 依赖；前两者已被 `.gitignore` 屏蔽，**但 reading-radar 175 个 node_modules 文件被错误追踪入 git**（playwright 库）—— 属独立反模式修复变更，本变更不处理
- **`engineering/apps/web/reading-radar/` 在 disk 上的旧空目录残留**（被 Windows 锁定）—— 用户已确认后续手动处理