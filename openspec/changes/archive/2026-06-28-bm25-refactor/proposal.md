## Why

当前项目的 BM25 实现已有核心算法（TAAT/DAAT + BMW 跳跃），但缺少两项关键能力：文档删除（依赖正排索引）和磁盘持久化。通过对比 Elasticsearch（Lucene）与 openGauss 的 BM25 生产级实现，明确了两项缺失的设计模式，现在补全。

## What Changes

- **新增正排索引**：为每个文档记录其包含的 token 列表，实现 `doc_id → tokens` 的反向映射，支撑高效的单文档删除
- **新增文档删除**：`bm25_delete_document()` — 通过正排索引定位所有关联倒排链并移除 posting，docId 进入复用池
- **新增磁盘持久化**：`bm25_save(path)` / `bm25_load(path)` — 将内存索引序列化到文件并恢复，自定义二进制格式
- **新增批量添加**：`bm25_index_add_documents()` — 多文档批量建库，单次落盘/重哈希，减少中间开销
- **优化搜索统计**：扩展 `bm25_search_stats_t` 增加耗时、堆比较次数、候选数量等调优观测指标
- **优化倒排存储**：posting 数组改为按 docId 有序插入（插入时二分定位），避免查询阶段再排序
- **优化 Posting Filter 重建**：只在搜索/最终化时重建 filter，插入阶段延迟计算

## Capabilities

### New Capabilities

- `bm25-forward-index`: 正排索引数据结构，记录 doc_id → token 列表的映射，支持增删查，由倒排索引构建过程同步维护
- `bm25-document-deletion`: 文档删除接口，通过正排索引定位 → 倒排链二分删除 → docId 回收复用 → 全局统计更新
- `bm25-persistence`: 磁盘持久化，包含二进制格式定义、顺序写入/读取、完整性校验(CRC32)、版本兼容标记
- `bm25-batch-add`: 批量添加接口，支持传入文档数组，一次扩容+一次哈希重建，减少内存碎片

### Modified Capabilities

（无已有 spec 文件，此项留空）

## Impact

- **新增文件**：`src/index/vector_index/BM25/bm25_delete.c`、`src/index/vector_index/BM25/bm25_persist.c`、`src/index/vector_index/BM25/bm25_batch.c`
- **修改文件**：
  - `bm25_private.h` — 新增正排索引结构、docId 复用池、扩展统计字段
  - `bm25.h` — 新增删除/持久化/批量接口声明
  - `bm25_core.c` — 初始化/销毁逻辑适配新数据结构
  - `bm25_add.c` — 插入时同步维护正排索引、有序插入 posting
  - `bm25_search.c` — 统计字段扩展
  - `bm25_utils.c` — 新增正排辅助函数、有序插入工具
- **依赖**：无新增外部依赖；`dict_t` 分词器接口不变
- **向后兼容**：所有现有 public API 保持不变，新接口为纯增量
