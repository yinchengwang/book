## ADDED Requirements

### Requirement: 索引序列化到文件
系统 SHALL 提供 `bm25_save(index, path)` 接口，将索引完整状态写入文件，格式为：
1. Header（32 字节）：magic("BM25")、version(1)、k1、b、term_slot_count、doc_count、total_terms
2. Terms Section：每个 term 的文本、hash、doc_freq、posting_count
3. Postings Section：每个 term 的 posting[] 数组和 filter[] 数组
4. Documents Section：doc_lengths[]、正排索引全部数据
5. Footer：CRC32 校验和（4 字节）

所有多字节整数使用小端序写入。

#### Scenario: 成功保存并验证文件存在
- **WHEN** 对包含 100 篇文档的索引调用 `bm25_save(index, "index.bm25")`
- **THEN** 文件 "index.bm25" 被创建，文件大小 > 0，且 CRC32 校验通过

#### Scenario: 保存空索引
- **WHEN** 对文档数为 0 的索引调用 `bm25_save(index, "empty.bm25")`
- **THEN** 文件创建成功，Header 中 doc_count=0，无 posting/doc 数据段

#### Scenario: 路径不可写
- **WHEN** 保存到无写入权限的路径
- **THEN** 返回错误码 -1，索引状态不变

### Requirement: 从文件反序列化恢复索引
系统 SHALL 提供 `bm25_index_load(path)` 接口，读取文件并按序重建内存索引结构：
1. 验证 magic 和 version
2. 读取参数并创建 bm25 实例
3. 逐一创建 term_slot（含 posting[] 和 filter[]）
4. 重建 term_hash_buckets 哈希表
5. 恢复 doc_lengths、doc_forwards 正排索引
6. 验证 CRC32 校验和

#### Scenario: 成功加载已保存的索引
- **WHEN** 从之前保存的 "index.bm25" 调用 `bm25_index_load("index.bm25")`
- **THEN** 返回的 bm25_t 指针非空，doc_count 与保存时一致，搜索功能正常

#### Scenario: 加载损坏的文件
- **WHEN** 加载一个 CRC32 不匹配的文件
- **THEN** 返回 NULL，不会创建不完整的索引实例

#### Scenario: 加载不存在的文件
- **WHEN** 加载不存在的路径
- **THEN** 返回 NULL

#### Scenario: 版本不兼容
- **WHEN** 加载 version 字段不同于当前版本的持久化文件
- **THEN** 返回 NULL（当前仅接受 version=1）

### Requirement: 保存/加载往返一致性
系统 SHALL 保证：索引保存后再加载，搜索功能的结果与保存前完全一致（相同查询返回相同的 doc_ids 和 scores，顺序一致）。

#### Scenario: 往返后搜索结果一致
- **WHEN** 对索引 A 搜索得到 results_A，保存 A 到文件，加载得到索引 B，对 B 搜索得到 results_B
- **THEN** results_A 与 results_B 的 doc_ids 和 scores 完全相同（浮点误差允许 ±1e-6）
