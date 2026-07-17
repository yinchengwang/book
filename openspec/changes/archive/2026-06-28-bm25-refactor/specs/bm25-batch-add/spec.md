## ADDED Requirements

### Requirement: 批量添加多篇文档
系统 SHALL 提供 `bm25_index_add_documents(index, document_count, documents, doc_ids_out)` 接口，支持一次传入多篇 `bm25_document_input_t`，返回每篇文档分配的 doc_id。

#### Scenario: 批量添加 100 篇文档
- **WHEN** 调用 `bm25_index_add_documents()` 传入 100 篇有效文档
- **THEN** 返回 0（成功），doc_ids_out 填有 100 个连续的 doc_id（0~99），索引大小为 100

#### Scenario: 批量添加空列表
- **WHEN** 调用 `bm25_index_add_documents()` 传入 document_count=0
- **THEN** 返回 0，索引状态不变

#### Scenario: 批量添加中部分失败回滚
- **WHEN** 批量添加到第 50 篇时发生内存分配失败
- **THEN** 返回 -1，索引状态回滚到批量添加开始前（前 49 篇的变更被撤销）

### Requirement: 批量添加减少重分配次数
系统 SHALL 在批量添加开始时，一次性将 term_slot_capacity、doc_capacity、hash_bucket_count 扩容到预估目标容量，避免逐篇添加时的多次 realloc。

#### Scenario: 批量添加仅触发一次扩容
- **WHEN** 在空索引上批量添加 1000 篇文档，每篇包含 5 个不同 token
- **THEN** term_slots 的 realloc 次数不会超过 32（2^5=32 覆盖 ~5000 个唯一 term 的预期）

### Requirement: 批量添加后自动重建 Posting Filter
系统 SHALL 在所有文档写入完成后，统一对所有被修改的 term_slot 重建 posting_filter，而非在每篇文档插入时单独重建。

#### Scenario: 批量添加后 filter 完整
- **WHEN** 批量添加 N 篇文档后立即搜索
- **THEN** DAAT 搜索的 block_skip 行为正确，各 filter 的 last_doc_id 和 max_score 与实际 posting 数据一致
