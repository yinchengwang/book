# 全文索引与倒排索引

## 概述

演示倒排索引的数据结构和 BM25 相关度评分算法。

## 编译运行

```bash
make && ./test.exe
```

或直接编译：

```bash
gcc -std=c11 -Wall -Wextra -g main.c -o test.exe
./test.exe
```

## 核心概念

### 倒排索引（Inverted Index）

倒排索引是全文检索的核心数据结构：

- **结构**：`term → [doc_id, freq, positions]`
- **词条（Term）**：分词后的单词
- **倒排链表（Posting List）**：包含该词条的所有文档列表
- **词频（TF）**：词条在文档中的出现次数
- **文档频率（DF）**：包含该词条的文档数

### 简单分词（ngram 概念）

本演示使用简单的空格+标点分词：

- 按空格和标点符号分割
- 转换为小写
- 过滤短词（长度 ≥ 2）

工程代码支持更多分词器类型：
- `TOKENIZER_WHITESPACE`：空格分词
- `TOKENIZER_CHINESE_MM`：中文最大匹配分词
- `TOKENIZER_MIXED`：混合分词

### BM25 评分公式

BM25 是 TF-IDF 的改进版本：

```
score(D, Q) = Σ IDF(qi) × (tf × (k1+1)) / (tf + k1×(1-b+b×|D|/avgdl))
```

其中：
- **IDF(qi)** = `log((N - n(qi) + 0.5) / (n(qi) + 0.5) + 1)`
  - N：总文档数
  - n(qi)：包含该词条的文档数
- **tf**：词频
- **k1**：饱和参数（通常 1.2-2.0）
- **b**：长度归一化参数（通常 0.75）
- **|D|**：文档长度
- **avgdl**：平均文档长度

### 相关度排序

搜索结果按 BM25 分数降序排列：

1. 分数越高，相关度越高
2. IDF 高（稀有词）贡献更大
3. 长文档有长度惩罚
4. 词频饱和效应（避免高频词过度加权）

## 输出示例

```
=== 全文索引与 BM25 演示 ===

【索引文档】
  Doc 1: PostgreSQL is a powerful open source relational database system
  Doc 2: MySQL is an open source database management system
  Doc 3: Full text search enables fast text querying in databases

【倒排索引】
  'postgresql' -> [Doc1(tf=1)]
  'is' -> [Doc1(tf=1), Doc2(tf=1)]
  'powerful' -> [Doc1(tf=1)]
  'open' -> [Doc1(tf=1), Doc2(tf=1)]
  'source' -> [Doc1(tf=1), Doc2(tf=1)]
  'relational' -> [Doc1(tf=1)]
  'database' -> [Doc1(tf=1), Doc2(tf=1), Doc3(tf=1)]

【查询: "database"】
  结果 (BM25 排序):
  1. Doc 1 (score=0.xxx)
  2. Doc 2 (score=0.xxx)
  3. Doc 3 (score=0.xxx)
```

## 与工程代码对照

详见 `NOTES.md`。