## 1. 目标清单确认

- [x] 1.1 扫描 engineering/ 和 build/ 下所有 test_*.db / test_kv.db / test_txn.db / test_wal.db / test_db.bin / *.wal / *.db.wal 文件
- [x] 1.2 验证 14 个目标文件全部 git untracked
- [x] 1.3 分类为 A 类（build/ 下，合规位置 2 个 20KB）和 B 类（engineering/test/ 下源码目录违规 12 个 88KB）

## 2. 执行清理

- [x] 2.1 删除 A 类 2 个文件（build/engineering/test/db/storage/test_kv.db + .wal）
- [x] 2.2 删除 B 类 12 个文件（engineering/test/ 和 engineering/test/db/ 下 test_db.bin / test_kv.db/.wal / test_txn.db/.wal / test_wal.db ×2 路径）

## 3. 验证

- [x] 3.1 `git status --short` 确认无 tracked 变动
- [x] 3.2 `find engineering/ build/ -name "test_*.db*" -o -name "test_*.wal" -o -name "test_db.bin"` 为空

## 4. 提交

- [x] 4.1 提交变更（一个 commit）

## 跳过/延迟记录

- **测试代码产物落点改造**：测试代码硬编码 `"test_kv.db"` 等相对路径，应改为 `<test-results>/engineering/` 路径 + CMake 加 `WORKING_DIRECTORY` + 测试 fixture tearDown 清理。属独立变更，本变更只清残留。

## 已知遗留项

- 工程轨 ~30+ 个测试文件硬编码相对路径（如 `cli_test.cpp`、`storage_kv_test.cpp`、`db_core_test.cpp` 等）—— 解决需修改 C++ 测试代码 + CMake/CTest 配置，独立变更处理