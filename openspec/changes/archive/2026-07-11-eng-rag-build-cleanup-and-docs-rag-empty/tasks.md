## 1. 候选目录分类盘点

- [x] 1.1 检查 `engineering/rag/build/` (31MB, untracked) — 违规内嵌构建
- [x] 1.2 检查 `engineering/src/db/index/` (1.7MB, git tracked) — **真实索引源代码**（10 个索引类型）— **不删**
- [x] 1.3 检查 `docs/rag/` (4KB 空目录, untracked) — 占位空目录
- [x] 1.4 检查 `archive/distributed/` (184KB, git tracked) — **Phase 9 真实归档源码**（4 对 .c/.h）— **不删**

## 2. 执行清理

- [x] 2.1 删除 `engineering/rag/build/` (31MB)
- [x] 2.2 删除 `docs/rag/` (空目录树)

## 3. 验证 + 提交

- [x] 3.1 运行 `git status --short` 确认无 tracked 变动
- [x] 3.2 提交变更（一个 commit）

## 跳过/延迟记录

- **`engineering/rag/` CMake 集成改造**：把 RAG 通过 `add_subdirectory` 接管其构建产物到 `build/engineering/rag/`，移除 `engineering/rag/build/` 路径存在可能性。属独立变更。
- **`docs/rag/diagrams/` 内容规划**：是否需要在共享 `docs/rag/` 中放 RAG 架构图？当前 `engineering/rag/docs/` (232KB) 已涵盖。如有规划应单独评估。

## 已知遗留项

- **`engineering/src/db/index/` 1.7MB**：10 个索引类型源代码（art/bitmap/bplus_tree/brin/btree/fulltext/gin/gist/hash/hilbert/radix_tree），本就不该被压缩
- **`archive/distributed/` 184KB**：Phase 9 分布式模块归档（coordinator/dist_txn/raft/shard），历史参考保留