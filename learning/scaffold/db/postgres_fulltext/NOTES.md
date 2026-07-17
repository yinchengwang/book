# 工程代码对照笔记

## 对照源码

`engineering/src/db/index/fulltext/fulltext.c`

## 数据结构对比

### 倒排索引结构

**教学版（main.c）**：
- 使用固定大小数组 `inverted_entry_t entries[MAX_TERMS]`
- 每个词条直接存储倒排链表 `posting_t postings[MAX_DOCS]`
- 简单线性查找词条

**工程版（fulltext.c）**：
- 使用 GIN（Generalized Inverted Index）作为底层索引：`gin_index_t *inverted`
- 词条统计单独存储：`term_stats_t *term_stats` 动态扩容
- 文档信息独立管理：`ft_doc_t *docs` 动态数组

### 核心差异

| 特性 | 教学版 | 工程版 |
|------|--------|--------|
| 索引存储 | 固定大小数组 | GIN 索引 + 动态数组 |
| 词条查找 | 线性扫描 O(n) | GIN 索引查找 O(log n) |
| 内存管理 | 静态分配 | 动态扩容 |
| 分词器 | 硬编码空格分词 | 可插拔分词器接口 |
| 搜索语法 | 单一查询 | 支持 AND/OR/NOT |

## 函数对照表

### 分词函数

**教学版**：
```c
int tokenize(const char *text, char tokens[][MAX_TERM_LEN], int *positions)
```
- 直接处理字符串
- 返回固定二维数组

**工程版**：
```c
tokenizer_t *tokenizer_create(tokenizer_type_t type)
char **tokenize(const tokenizer_t *tok, const char *text, int *count)
void tokenizer_free_tokens(const tokenizer_t *tok, char **tokens, int count)
```
- 分词器抽象为对象
- 支持多种分词器类型（WHITESPACE, CHINESE_MM, MIXED）
- 需要手动释放 token 内存

### 索引函数

**教学版**：
```c
void add_to_index(fulltext_index_t *idx, int doc_id, const char *term, int position)
void index_document(fulltext_index_t *idx, int doc_id, const char *text)
```
- 简单直接的索引操作
- 无返回值，不处理错误

**工程版**：
```c
int fulltext_index_doc(fulltext_index_t *idx, int doc_id, const char *text)
int fulltext_index_batch(fulltext_index_t *idx, const int *doc_ids, const char **texts, int count)
```
- 返回状态码（成功/失败）
- 支持批量索引

### 搜索函数

**教学版**：
```c
int search(fulltext_index_t *idx, const char *query, int *result_docs, float *result_scores, int max_results)
```
- 单一搜索接口
- BM25 评分

**工程版**：
```c
int fulltext_search(const fulltext_index_t *idx, const char *query,
                    int *doc_ids, float *scores, int *count, int max_results)
```
- 支持 AND/OR/NOT 语法解析
- 内部调用 `_search_simple`, `_search_with_and`, `_search_with_or`

### 评分算法

**教学版**：
- 完整实现 BM25 公式
- 显式计算 IDF 和 TF 组件
- 参数可调整（k1=1.5, b=0.75）

**工程版**：
- `_search_simple` 中使用简化评分：`1.0f / term_count`
- TF-IDF 计算函数已定义但未使用（注释中保留）
- 实际评分策略可根据需求切换

## 工程增强特性

### 1. 分词器可插拔

```c
typedef enum {
    TOKENIZER_WHITESPACE,    // 空格分词
    TOKENIZER_CHINESE_MM,    // 中文最大匹配
    TOKENIZER_MIXED          // 混合分词
} tokenizer_type_t;
```

`fulltext_set_tokenizer()` 允许运行时切换分词器。

### 2. 词长过滤

```c
void fulltext_set_word_length(fulltext_index_t *idx, int min_len, int max_len)
```

控制索引词条的长度范围，过滤无效词条。

### 3. 统计信息

```c
void fulltext_stats(const fulltext_index_t *idx, int *out_n_docs,
                    int *out_n_terms, float *out_avg_doc_len)
```

获取索引元数据，支持查询优化。

### 4. 高亮显示

```c
int fulltext_highlight(const fulltext_index_t *idx, const char *text,
                       const char *query, char *output, int max_len)
```

在原文中标记匹配词条（简化实现）。

### 5. 中文分词支持

`_tokenize_chinese_mm()` 函数实现了最大匹配分词的简化版本：
- 识别 UTF-8 中文字符
- 尝试组成双字词
- 单字和双字同时输出

## 底层依赖

工程版依赖 GIN 索引模块：
```c
#include <db/index/gin/gin.h>

gin_index_t *inverted;     // 底层 GIN 索引
gin_insert(idx->inverted, tokens[i], doc_id);
gin_search(idx->inverted, terms[t], results, &result_count);
```

GIN（Generalized Inverted Index）是 PostgreSQL 风格的通用倒排索引，支持：
- BTree 或 Hash 存储词条
- 高效的范围查询
- 并发访问控制

## 总结

教学版侧重演示核心概念：
- 倒排索引结构清晰直观
- BM25 公式完整实现
- 适合学习理解算法原理

工程版侧重生产可用：
- 模块化设计，接口抽象
- 支持多种分词器和搜索语法
- 动态内存管理，可扩展
- 依赖成熟索引模块（GIN）

理解教学版后，工程版的设计意图和实现细节更容易掌握。