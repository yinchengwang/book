# eng-rag-build-cleanup-and-docs-rag-empty

## Why

深入清理四个候选目录后发现：

- `engineering/rag/build/` (31MB, untracked)：RAG 工程违规内嵌构建残留。`engineering/rag/` 应通过 CMake `add_subdirectory` 集成到工程轨，其构建产物应落在 `build/engineering/rag/`，但当前用户的本地构建把整个 RAG 构建到了 `engineering/rag/build/`（不在 Phase 1 规范的 `build/` 路径下）。
- `docs/rag/` (4KB 空目录树, untracked)：只有 `diagrams/` 一个空子目录，未填充任何内容，属于早期规划占位但未实施的目录。
- `engineering/src/db/index/` (1.7MB, git tracked)：**真实索引子系统源代码**（10 个索引类型 art/bitmap/bplus_tree/brin/btree/fulltext/gin/gist/hash/hilbert/radix_tree），不可压缩。
- `archive/distributed/` (184KB, git tracked)：**Phase 9 分布式模块归档源码**（coordinator/dist_txn/raft/shard 四对 .c/.h），保留供历史参考。

## What Changes

- **删除** `engineering/rag/build/` (31MB)：违规内嵌构建残留，下次需要时通过规范的工程轨 `cmake -B build/engineering -S engineering` 重建到正确路径
- **删除** `docs/rag/` (空目录树)：未填充的占位目录，与 `engineering/rag/docs/` (232KB) 重复内容不应在共享 docs 中重复
- **不删** `engineering/src/db/index/` 和 `archive/distributed/` (已确认是真实跟踪内容)

## Impact

- 不影响工程轨构建（`engineering/rag/build/` 是 untracked，重新 cmake 会重建）
- 不影响 docs 工程文档（`docs/rag/diagrams/` 为空，无内容迁移）
- 不影响索引子系统（10 个索引类型源代码保留）
- 不影响历史归档（Phase 9 分布式模块完整保留）