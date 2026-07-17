## 1. 残留盘点与白名单校验

- [x] 1.1 用 `git ls-files` 列出根目录所有"未跟踪但存在"的条目
- [x] 1.2 分类为：运行时产物（删除） vs git 跟踪资产（保留，如 `kanban-c-mode.png`）
- [x] 1.3 确认无脚本或文档引用即将删除的根级产物

## 2. 运行时产物删除

- [x] 2.1 删除根级 `test_*.db`、`test_*.db.wal`、`test_*.bin`、`test_*.wal`（数据库/wal/bin/temp 文件）
- [x] 2.2 删除 `test_vector_data/`、`test_mm_pool_data/`、`test_raft_state.bin.tmp`、`test_dir/`（目录）
- [x] 2.3 删除 `data/` 整个目录（PG 风格多模态存储运行时数据库目录）
- [x] 2.4 删除 `build-learning/`（已被 `build/learning/` 取代的旧构建目录）
- [x] 2.5 删除 `build2/`（孤立旧构建目录）
- [x] 2.6 删除 `test-summary.json`（测试运行时摘要）

## 3. .gitignore 防御性规则

- [x] 3.1 添加 `test_*.db`、`test_*.db.wal`、`test_*.bin`、`test_*.wal` ignore 规则
- [x] 3.2 添加 `test_vector_data/`、`test_mm_pool_data/`、`test_raft_state.bin.tmp`、`test_dir/` ignore 规则
- [x] 3.3 添加 `build-learning/`、`build2/` ignore 规则
- [x] 3.4 添加 `test-summary.json` ignore 规则

## 4. 验证与提交

- [x] 4.1 运行 `git status --short` 确认只删除了未跟踪文件，未误删 git 跟踪内容
- [x] 4.2 确认 `kanban-c-mode.png` 等 git 跟踪文件保留
- [x] 4.3 提交变更（一个 commit）

## 跳过/延迟记录

无。

## 已知遗留项

- **测试代码仍把运行时产物写到根目录**：相关测试代码（KV/vector/mm pool/raft/wal/txn 等）的产物落点修缮属于"测试运行时产物落点规范"独立变更，本变更仅清理残留 + 加防御。