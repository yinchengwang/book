## Context

当前项目的 BM25 是纯内存的倒排索引，核心数据流为：

```
文档 → tokenizer → [term hash → term_slot → posting[]]
查询 → tokenizer → [term定位 → TAAT/DAAT → TopK Heap]
```

基于 Elasticsearch(Lucene) 和 openGauss 的 BM25 实现调研，明确了两大参考：

| 参考源 | 可借鉴的设计 |
|--------|-------------|
| openGauss `BM25DocForwardItem` | 正排索引结构：docId → tokenId 列表，支撑删除 |
| openGauss VACUUM 两阶段删除 | 简化版：标记 → 按正排定位倒排链 → 物理删除 posting |
| openGauss `ReorderPosting` | 有序 posting 链 + 二分 Seek |
| Lucene `IndexWriter` + `FlushPolicy` | 内存积累 → 批量落盘的思路 |
| Lucene `SkipList` / `BlockMax` | 本项目已有同类实现（posting_filter），保持即可 |

## Goals / Non-Goals

**Goals:**
- 实现正排索引，使文档删除无需全量重建
- 支持磁盘持久化，索引可跨进程复用
- 支持批量文档添加，减少内存重分配次数
- Posting 按 docId 有序插入，消除查询阶段的散乱状态
- 扩展搜索统计，为调优提供观测数据

**Non-Goals:**
- 不引入 PMR（Page Buffer Manager）磁盘页管理 — 保持纯内存 + 手动序列化
- 不引入并行构建 worker — 单线程足够，多线程放在后续版本
- 不支持事务性回滚 — 删除即生效，与 openGauss VACUUM 的两阶段事务语义不同
- 不改变现有 public API 签名 — 所有修改为增量

## Decisions

### 决策1：正排索引采用紧凑数组而非链表

**选择**：`bm25_doc_forward_list_t` 为每个文档维护一个动态数组 `items[]`，容量按 2x 增长。

**理由**：
- 链表的节点分配开销大（每个 posting 一次 malloc）
- 数组支持二分查找（当需要查找某文档是否包含某 token 时）
- openGauss 也是类似的紧凑页存储（`BM25DocForwardItem` 连续排列在页中）

**替代方案**：
- 链表 → 内存碎片多，遍历时 cache miss 严重
- 直接在 term_slot 存 docId 反向映射 → term_slot 数量远大于单文档 token 数，浪费空间

### 决策2：有序 Posting 插入采用二分定位

**选择**：文档插入时，每个 term_slot 的 posting[] 在新追加前用二分查找定位插入位置，然后 `memmove` 后移。

**理由**：
- 写入时的 O(log N + M) 成本可接受（N = posting 数量，M = 移动元素数）
- 查询阶段直接顺序扫描，无需再排序
- openGauss 的 `ReorderPosting()` 在构建完成后全量排序，本项目在插入时增量维护
- DAAT/MaxScore 算法依赖 posting 按 docId 有序（Seek/Advance 操作必须）

**替代方案**：
- 插入时追加末尾 + 查询前全量排序 → 查询延迟不可控
- 用 skiplist → 实现复杂度高，内存开销大（指针多）

### 决策3：文档删除采用单阶段物理删除

**选择**：`bm25_delete_document()` 直接物理清除所有关联 posting，回收 docId。

**理由**：
- 纯内存、单线程场景，无并发事务可见性问题
- openGauss 的两阶段（标记 + VACUUM）适用于 MVCC 事务模型，本项目不需要
- 简化实现：正向定位 → 倒排链二分删除 → 统计更新 → 入池

**替代方案**：
- 标记删除（`is_deleted` flag）→ 延迟物理清理 → 与 openGauss 一致但复杂度高，无事务需求时属于过度设计

### 决策4：持久化采用顺序二进制写入

**选择**：自定义二进制格式，按 header → terms → postings → docs 顺序写单个文件。

**理由**：
- 相比 JSON/protobuf，二进制格式紧凑，加载更快
- 顺序写入/读取，适合全量保存/加载场景
- 参考 Lucene Segment 文件和项目中 B+Tree 的 `bptree_persist.c` 模式

**文件格式**：
```
┌──────────────────────────────────────────────┐
│ Header (32 bytes)                             │
│   magic: "BM25" (4 bytes)                    │
│   version: uint32 (4 bytes)                  │
│   k1, b: float (8 bytes)                     │
│   term_slot_count, doc_count: int32 (8 bytes) │
│   total_terms: int64 (8 bytes)                │
├──────────────────────────────────────────────┤
│ Terms Section                                 │
│   for each term_slot:                        │
│     term_len: uint16                         │
│     term_text: char[term_len]                │
│     hash_value: uint64                       │
│     doc_freq, total_term_freq: int32         │
│     posting_count: int32                     │
├──────────────────────────────────────────────┤
│ Postings Section                              │
│   for each term_slot:                        │
│     posting[]: {doc_id:int32, tf:int32} × N  │
│     filter[]: {int32×4 + float} × M          │
├──────────────────────────────────────────────┤
│ Documents Section                             │
│   doc_lengths[]: int32 × doc_count           │
│   for each doc:                              │
│     forward_item_count: int32                │
│     forward[]: {token_slot_idx:int32,         │
│                  token_hash:uint64} × N      │
├──────────────────────────────────────────────┤
│ Footer: crc32 (4 bytes)                      │
└──────────────────────────────────────────────┘
```

### 决策5：批量添加复用已有的单文档添加流程

**选择**：`bm25_index_add_documents()` 先预扩容 term 表和 doc 表到目标容量，然后循环调用 `bm25_index_add_document()` 的内部逻辑。

**理由**：
- 避免新增一条完全独立的批量路径，减少维护成本
- 一次性扩容减少 realloc 次数（单文档添加可能触发多次 realloc）
- 批量结束后统一 rebuild posting filters

**替代方案**：
- 完全独立的批量路径 → 代码重复，bug 多发
- 并行构建 → 后续版本考虑

## Risks / Trade-offs

- **[风险] 有序插入的 memmove 开销**：当 posting 链很长时，每次插入需移动大量元素
  - **缓解**：批量添加时按 docId 递增顺序插入，大多数情况下新 posting 的 docId 最大，定位到末尾直接追加，memmove 成本为 0
- **[风险] 删除后倒排链收缩不回收内存**：删除 posting 后数组缩小但不会 realloc 缩容
  - **缓解**：仅在 `bm25_save()` 时做 compact；后续版本可加 `bm25_compact()` 主动缩容
- **[权衡] 持久化格式与代码版本绑定**：version 字段仅标记格式版本，不保证跨大版本的兼容性
  - **接受**：本项目为学习/原型用途，不承诺长期格式兼容
