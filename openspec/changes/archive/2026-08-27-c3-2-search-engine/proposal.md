# C3-2 全文搜索引擎增强（自研）Proposal

## Why

差距分析 §5.2 + 用户全自研决策：自研已有 BM25（`bm25.c`）+ 倒排（`doc_inverted.c`）+ 三 tokenizer + 同义词，但缺中文分词（依赖 C2-6）、Snowball 词干化、字段加权、function_score 钩子、高亮、segment 化近实时索引——与 Lucene/ES 距离大。不集成 Lucene，从零扩展。

## What Changes

- 字段加权（field boost 乘子）：索引定义阶段 + 查询阶段双层乘子
- function_score 钩子：用户自定义打分函数注入（回调接口 + 简单表达式 DSL）
- 统一高亮器：postings 记录 token offset + 前后文窗口 + unified highlighter 风格
- Segment 化近实时索引（借鉴 Lucene）：
  - 内存 segment 追加 → 阈值 seal 为不可变磁盘 segment
  - 搜索时多 segment 归并（OR/AND/打分合并）
  - 后台 refresh 线程（毫秒级近实时）
- 与 C2-6 中文分词/Snowball 集成（链式分析器）
- doc_fts 高亮 API 扩展

## Capabilities

| 能力 | 交付 |
|------|------|
| 字段加权 | title:N 倍 boost 可配 |
| function_score | 用户注入打分函数影响结果排序 |
| 高亮 | 命中片段前后文窗口标记 |
| 近实时 | 写入后 ≤1s 可搜（refresh 间隔可配） |
| 多 segment | 并发写入不停搜索 |

## Impact

- 修改：doc_inverted.c、doc_fts.c、bm25.c
- 新增：segment_manager.c、highlighter.c、function_score.c、refresh_thread.c
- 预计 7-9 个 commit
- 依赖：C2-6
