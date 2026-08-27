# C3-2 全文搜索引擎增强设计文档

## 设计目标

把 Document 模态从"有 BM25"升级为"完整单机搜索引擎"：链式分析器 + 字段加权 + 高亮器 + Segment 化近实时索引。

## 方案

1. **链式分析器**：DocTokenizer 接口扩展为 char_filter → tokenizer → token_filter 三段管线
2. **字段加权**：schema 定义时 field_boost；查询时 field_boost × BM25 score
3. **高亮器**：postings token offset 记录 + 前后文窗口渲染（unified highlighter 风格）
4. **Segment 化近实时**：内存段（append-only）→ flush 不可变磁盘段 → search 多段 OR 归并

## 实现文件

- `engineering/include/db/doc_fts_enhanced.h`（新增）
- `engineering/src/db/storage/doc/doc_fts_enhanced.c`（新增）
