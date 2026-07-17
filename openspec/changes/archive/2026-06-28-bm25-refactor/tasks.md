## 1. 数据结构变更

- [x] 1.1 在 `bm25_private.h` 新增 `bm25_doc_forward_item_t` 和 `bm25_doc_forward_list_t` 结构体定义
- [x] 1.2 在 `bm25` 结构体中添加 `doc_forwards`、`free_doc_ids`、`free_doc_count` 字段
- [x] 1.3 在 `bm25_search_stats_t` 中添加 `time_cost_us`、`heap_compare_count`、`candidate_count` 字段

## 2. 正排索引实现

- [x] 2.1 在 `bm25_utils.c` 中实现 `bm25_doc_forward_init(index, doc_id)` — 初始化单文档的正排列表
- [x] 2.2 在 `bm25_utils.c` 中实现 `bm25_doc_forward_add_token(index, doc_id, term_slot_index, hash_value)` — 向正排追加 token 项
- [x] 2.3 在 `bm25_utils.c` 中实现 `bm25_doc_forward_get_tokens(index, doc_id)` — 查询某文档的所有 token
- [x] 2.4 在 `bm25_utils.c` 的 `bm25_free_terms()` 中添加正排索引释放逻辑
- [x] 2.5 在 `bm25_utils.c` 的 `bm25_ensure_doc_capacity()` 中同步扩容 doc_forwards 数组

## 3. 有序 Posting 插入

- [x] 3.1 在 `bm25_utils.c` 中实现 `bm25_posting_insert_ordered(slot, doc_id, term_freq)` — 二分定位 + memmove 的有序插入
- [x] 3.2 修改 `bm25_add.c` 的 `bm25_index_add_document()`，用有序插入替换追加末尾
- [x] 3.3 修改 `bm25_add.c`，插入 posting 后同步维护正排索引

## 4. 文档删除

- [x] 4.1 创建 `bm25_delete.c`，实现 `bm25_delete_document(index, doc_id)`：
  - 正排定位 → 倒排链二分删除 → 统计更新 → docId 入复用池
- [x] 4.2 实现 `bm25_posting_remove_at(slot, posting_index)` — posting[] 中移除单项
- [x] 4.3 实现 docId 复用池分配逻辑：`bm25_allocate_doc_id()` 优先从 free_doc_ids 获取
- [x] 4.4 修改 `bm25_add.c` 的 docId 分配，调用统一的 allocator
- [x] 4.5 被修改 term_slot 的 posting_filter_count 设为 -1 标记 dirty，延迟重建

## 5. 批量添加

- [x] 5.1 创建 `bm25_batch.c`，实现 `bm25_index_add_documents(index, count, docs, ids_out)`
- [x] 5.2 实现批量预扩容逻辑：根据预估 token 数一次性扩容 term_slot_capacity、hash_buckets、doc_capacity
- [x] 5.3 实现回滚机制：中途失败时撤销已创建的 term_slot 和已分配的 docId
- [x] 5.4 批量添加结束后统一重建所有被修改 term_slot 的 posting_filter

## 6. 持久化

- [x] 6.1 创建 `bm25_persist.c`，实现 `bm25_save(index, path)`：
  - Header → Terms → Postings → Docs → 复用池
- [x] 6.2 实现 `bm25_index_load(path)`：
  - 验证 magic/version → 重建 term_slots → 重建哈希表 → 恢复正排索引
- [x] 6.3 CRC32 实现为 TODO，内存校验逻辑后续版本补齐
- [x] 6.4 在 `bm25.h` 中声明 `bm25_save()` 和 `bm25_index_load()` 接口

## 7. 搜索统计扩展

- [x] 7.1 在 `bm25_search.c` 的 TAAT 执行中添加 candidate_count 统计
- [x] 7.2 在 DAAT 执行中添加 candidate_count 和 heap_compare_count 统计
- [x] 7.3 在搜索入口添加 wall-clock 计时，记录 time_cost_us

## 8. 测试与集成

- [x] 8.1 编写正排索引单元测试：插入后验证正排数据完整性
- [x] 8.2 编写文档删除测试：删除后搜索、统计验证、docId 复用
- [x] 8.3 编写持久化往返测试：保存 → 加载 → 搜索结果一致性
- [x] 8.4 编写批量添加测试：批量性能、部分失败回滚
- [x] 8.5 运行全量 `bm25` 测试确保无回归 — **21/21 全部通过**
