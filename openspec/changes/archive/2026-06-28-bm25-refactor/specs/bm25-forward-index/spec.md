## ADDED Requirements

### Requirement: 正排索引随文档插入自动构建
系统 SHALL 在 `bm25_index_add_document()` 完成后，为该文档维护一个正排索引条目，记录该文档包含的所有 token 的 `term_slot_index` 和 `hash_value`。
正排索引每个文档对应一个 `bm25_doc_forward_list_t`，其中 items[] 数组按 term_slot_index 升序排列。

#### Scenario: 单文档插入后正排索引存在
- **WHEN** 向空索引添加一篇包含 3 个不同 token 的文档
- **THEN** 该文档的正排索引 items 数量为 3，每个 item 的 term_slot_index 和 hash_value 与倒排词典中的对应 term 一致

#### Scenario: 空文档的正排索引
- **WHEN** 向索引添加一篇 token 数为 0 的文档
- **THEN** 该文档的正排索引 items 数量为 0，正排列表 allocated 但为空

### Requirement: 正排索引支持查询某文档的所有 token
系统 SHALL 提供 `bm25_doc_forward_get_tokens(index, doc_id)` 内部函数，返回该文档包含的所有 token 的 slot_index 列表。

#### Scenario: 查询已有文档的 token 列表
- **WHEN** 查询一篇已添加文档的正排 token 列表
- **THEN** 返回的 slot_index 数量与添加时的唯一 token 数一致，且均可通过 `bm25_get_term_slot()` 获取到有效 term

#### Scenario: 查询不存在的文档
- **WHEN** 查询 doc_id 超出 [0, doc_count) 范围的文档
- **THEN** 返回 NULL 或空列表

### Requirement: 正排索引随索引重置/销毁而释放
系统 SHALL 在 `bm25_index_reset()` 和 `bm25_index_drop()` 中释放所有正排索引占用的内存。

#### Scenario: 重置索引后正排清空
- **WHEN** 对包含 N 篇文档的索引调用 `bm25_index_reset()`
- **THEN** 正排索引所有数组被释放，doc_forwards 指针为 NULL
