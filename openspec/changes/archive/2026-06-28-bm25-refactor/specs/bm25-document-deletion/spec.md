## ADDED Requirements

### Requirement: 通过正排索引定位并删除所有关联 posting
系统 SHALL 提供 `bm25_delete_document(index, doc_id)` 接口，执行以下步骤完成物理删除：
1. 通过正排索引获取该文档所有 token 的 slot_index
2. 遍历每个 term_slot，在其 posting[] 中二分查找定位到目标 posting 并移除（memmove 覆盖）
3. 更新 term_slot 的 doc_freq、total_term_freq
4. 将 posting_filter 标记为 dirty，下次查询/保存时重建

#### Scenario: 成功删除一篇文档
- **WHEN** 向包含 5 篇文档的索引调用 `bm25_delete_document(index, 2)`
- **THEN** doc_id=2 的文档不再出现在任何搜索结果的 doc_ids 中，且被删除文档对应的 docId 进入复用池

#### Scenario: 删除不存在的文档
- **WHEN** 调用 `bm25_delete_document(index, 999)` 且 doc_count=5
- **THEN** 返回错误码 -1，索引状态不变

#### Scenario: 删除后搜索正确性
- **WHEN** 删除一篇包含 term "hello" 的文档后，搜索 "hello"
- **THEN** 命中数量减少 1，且被删除文档的 doc_id 不出现在结果中

### Requirement: 已删除 DocId 可复用
系统 SHALL 维护一个 `free_doc_ids[]` 复用池。当新文档插入时，优先从复用池分配 docId；复用池为空时才使用 `doc_count` 递增分配。
复用 docId 的 doc_lengths 和 doc_forwards 槽位被新文档覆盖。

#### Scenario: 删除后新文档复用 DocId
- **WHEN** 删除 doc_id=3 的文档后，再添加一篇新文档
- **THEN** 新文档被分配的 doc_id 等于 3（而非递增到 doc_count）

#### Scenario: 复用池耗尽后正常递增
- **WHEN** 复用池为空时添加新文档
- **THEN** 新文档分配的 doc_id 等于当前 doc_count

### Requirement: 文档删除维护全局统计
系统 SHALL 在删除文档时更新 `total_terms`（减去被删除文档的 token 数量）和 `doc_count`（减 1）。

#### Scenario: 删除后统计正确
- **WHEN** 索引有 10 篇文档、total_terms=100，删除一篇 8 个 token 的文档
- **THEN** bm25_index_size() 返回 9，bm25_index_average_doc_length() 返回 92.0/9.0

### Requirement: Posting Filter 延迟重建
系统 SHALL 在文档删除后将对应 term_slot 的 posting_filters 标记为 dirty（`posting_filter_count = -1`），实际重建推迟到下次搜索或持久化时。

#### Scenario: 删除后搜索触发 filter 重建
- **WHEN** 删除文档后立即搜索
- **THEN** 搜索过程中对 dirty filter 的 term_slot 自动重建 posting_filter，搜索结果正确
