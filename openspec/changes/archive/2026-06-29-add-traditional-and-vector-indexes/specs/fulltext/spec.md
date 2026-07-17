# FULLTEXT 全文索引

## Purpose

实现 FULLTEXT 全文索引，支持中英文分词和相关性排序。

## Requirements

### Requirement: 全文索引基础操作

全文索引 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 说明 |
|------|----------|------|
| 创建 | `fulltext_index_t *fulltext_create(void)` | 创建全文索引 |
| 配置分词器 | `int fulltext_set_tokenizer(fulltext_index_t *idx, tokenizer_t *tok)` | 设置分词器 |
| 索引文档 | `int fulltext_index_doc(fulltext_index_t *idx, int doc_id, const char *text)` | 索引文档 |
| 批量索引 | `int fulltext_index_batch(fulltext_index_t *idx, int *doc_ids, const char **texts, int count)` | 批量索引 |
| 搜索 | `int fulltext_search(fulltext_index_t *idx, const char *query, int *doc_ids, float *scores, int *count)` | 搜索并排序 |
| 高亮 | `int fulltext_highlight(fulltext_index_t *idx, const char *text, const char *query, char *output, int max_len)` | 高亮匹配词 |
| 销毁 | `void fulltext_destroy(fulltext_index_t *idx)` | 释放资源 |

### Requirement: 全文索引核心结构

```c
// TF-IDF 权重
typedef struct term_weight {
    char *term;
    float tf;              // 词频
    float idf;             // 逆文档频率
    int doc_count;         // 包含该词的文档数
} term_weight_t;

// 文档信息
typedef struct ft_doc {
    int doc_id;
    int word_count;
    hash_table_t *term_freqs;  // term -> freq
} ft_doc_t;

// 全文索引
typedef struct fulltext_index {
    gin_index_t *inverted;     // 底层 GIN 倒排索引
    hash_table_t *docs;        // doc_id -> ft_doc_t
    int total_docs;            // 总文档数
    tokenizer_t *tokenizer;    // 分词器
    int min_word_len;          // 最小词长过滤
    int max_word_len;          // 最大词长过滤
} fulltext_index_t;
```

### Requirement: TF-IDF 排序

搜索结果 SHALL 按 TF-IDF 分数排序：

```
score(d, q) = Σ tf(t,d) * idf(t)
idf(t) = log(N / df(t) + 1)
```

| 参数 | 说明 | 默认值 |
|------|------|--------|
| N | 总文档数 | - |
| df(t) | 包含词 t 的文档数 | - |
| TF | 词频归一化 | `freq / max_freq` |

### Requirement: 分词器支持

全文索引 SHALL 支持多种分词器：

| 分词器 | 说明 | 适用场景 |
|--------|------|----------|
| 空格分词 | 按空格和标点分割 | 英文 |
| 中文分词 | 正向最大匹配 (MM) | 中文基础 |
| 混合分词 | 中英文混合文本 | 中英混合 |

```c
typedef struct tokenizer {
    char **(*tokenize)(const char *text, int *count);
    void (*free_tokens)(char **tokens, int count);
} tokenizer_t;
```

### Requirement: 高级搜索语法

全文索引 SHOULD 支持以下搜索语法：

| 语法 | 示例 | 说明 |
|------|------|------|
| AND | `a AND b` | 同时包含 a 和 b |
| OR | `a OR b` | 包含 a 或 b |
| NOT | `a NOT b` | 包含 a 但不包含 b |
| 短语 | `"hello world"` | 精确短语匹配 |
| 前缀 | `hello*` | 前缀匹配 |

#### Scenario: 基础搜索

- **WHEN** 用户调用 `fulltext_search(idx, "索引 实现", results, scores, &count)`
- **THEN** 系统 SHALL 返回包含"索引"和"实现"的文档
- **AND** 按 TF-IDF 分数降序排列

#### Scenario: 短语搜索

- **WHEN** 用户调用 `fulltext_search(idx, "\"向量索引\"", results, scores, &count)`
- **THEN** 系统 SHALL 只返回包含精确短语"向量索引"的文档
