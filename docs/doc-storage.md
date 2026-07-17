# 文档存储使用指南

## 概述

文档存储引擎（Document Engine）专为文档数据的存储和检索设计，支持 JSON 文档存储、JSONPath 查询和全文搜索。

## 核心特性

| 特性 | 说明 |
|------|------|
| JSON 存储 | 原生 JSON 文档存储 |
| JSONPath | 支持 `$.field` 等路径查询 |
| 倒排索引 | 全文搜索索引 |
| BM25 打分 | 相关性排序 |
| FST 字典 | 高效术语查找 |

## 快速开始

### 1. 初始化引擎

```c
#include "db/storage/doc/doc_engine.h"

// 初始化文档引擎
doc_engine_init("/data/docs");

// 创建文档集合
doc_engine_create("articles", NULL);

// 打开文档集合
void *doc_db = doc_engine_open("articles", ACCESS_MODE_READ_WRITE);
```

### 2. 插入文档

```c
// 插入 JSON 文档
const char *json1 = "{"
    "\"title\":\"Introduction to C Programming\","
    "\"author\":\"John Doe\","
    "\"content\":\"This tutorial covers basics of C programming language.\","
    "\"tags\":[\"programming\", \"C\", \"tutorial\"],"
    "\"views\":1250,"
    "\"published\":true"
"}";

doc_engine_insert(doc_db, "doc1", json1, strlen(json1));

// 批量插入
const char *docs[] = {
    "{ \"title\":\"Data Structures\", \"content\":\"Learn about arrays, lists, trees...\" }",
    "{ \"title\":\"Algorithms\", \"content\":\"Sorting, searching, graph algorithms...\" }",
    "{ \"title\":\"Design Patterns\", \"content\":\"Common software design patterns...\" }"
};

for (int i = 0; i < 3; i++) {
    char doc_id[32];
    snprintf(doc_id, sizeof(doc_id), "doc%d", i + 2);
    doc_engine_insert(doc_db, doc_id, docs[i], strlen(docs[i]));
}
```

### 3. 查询文档

```c
// 使用 JSONPath 查询
doc_jsonpath_result_t *jp_results;
int jp_count = doc_engine_query_jsonpath(doc_db, "$.title", &jp_results, 100);

printf("查询 $.title 结果:\n");
for (int i = 0; i < jp_count; i++) {
    printf("  %s: %s\n", jp_results[i].doc_id, jp_results[i].value);
}
doc_engine_free_jsonpath_results(jp_results, jp_count);
```

## 倒排索引

倒排索引提供高效的全文搜索能力。

### 启用倒排索引

```c
// 启用倒排索引
// 参数：分词器类型（NULL 使用默认简单分词器）
int ret = doc_engine_enable_inverted_index(doc_db, "simple");
if (ret != 0) {
    fprintf(stderr, "启用倒排索引失败\n");
}

// 检查状态
doc_engine_db_t *db = (doc_engine_db_t *)doc_db;
if (db->use_inverted_index) {
    printf("倒排索引已启用\n");
}
```

### 全文搜索

```c
// 使用倒排索引搜索
doc_inverted_result_t results[100];

uint32_t result_count = doc_engine_inverted_search(doc_db, "programming tutorial", 
                                                     results, 100);

printf("搜索 \"programming tutorial\" 找到 %u 个结果:\n", result_count);
for (uint32_t i = 0; i < result_count; i++) {
    printf("  [%u] 文档: %s, 评分: %.4f\n", 
           i, ((doc_inverted_result_t*)results)[i].doc_id,
           ((doc_inverted_result_t*)results)[i].score);
}
```

### 保存/加载

```c
// 保存倒排索引
ret = doc_engine_save_inverted_index(doc_db);
if (ret != 0) {
    fprintf(stderr, "保存倒排索引失败\n");
}

// 禁用倒排索引
doc_engine_disable_inverted_index(doc_db);
```

## JSONPath 查询

支持标准 JSONPath 语法进行文档字段查询。

### 查询语法

| 语法 | 说明 | 示例 |
|------|------|------|
| `$.field` | 获取字段值 | `$.title` |
| `$.a.b.c` | 获取嵌套字段 | `$.store.book` |
| `$.arr[0]` | 获取数组元素 | `$.tags[0]` |
| `$.arr[*]` | 获取所有数组元素 | `$.tags[*]` |

### 查询示例

```c
// 准备测试数据
const char *doc = "{"
    "\"name\":\"test\","
    "\"value\":42,"
    "\"nested\":{"
        "\"a\":1,"
        "\"b\":{"
            "\"c\":\"hello\""
        "}"
    "},"
    "\"array\":[10, 20, 30]"
"}";
doc_engine_insert(doc_db, "test_doc", doc, strlen(doc));

// 查询顶层字段
doc_jsonpath_result_t *results;
int count;

// $.name
count = doc_engine_query_jsonpath(doc_db, "$.name", &results, 10);
printf("$.name = %s\n", results[0].value);
doc_engine_free_jsonpath_results(results, count);

// $.nested.b.c
count = doc_engine_query_jsonpath(doc_db, "$.nested.b.c", &results, 10);
printf("$.nested.b.c = %s\n", results[0].value);
doc_engine_free_jsonpath_results(results, count);

// $.array[1]
count = doc_engine_query_jsonpath(doc_db, "$.array[1]", &results, 10);
printf("$.array[1] = %s\n", results[0].value);
doc_engine_free_jsonpath_results(results, count);
```

## BM25 打分

BM25（Best Matching 25）是信息检索领域广泛使用的相关性评分算法。

### BM25 参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| k1 | 1.2 | 词频饱和参数 |
| b | 0.75 | 文档长度归一化参数 |

### 使用 BM25

```c
// BM25 在 doc_inverted_search 中自动使用
// 可以通过结果中的 score 字段获取相关性评分

doc_inverted_result_t results[100];
uint32_t count = doc_engine_inverted_search(doc_db, "programming", results, 100);

// 按评分排序输出
printf("按 BM25 评分排序的结果:\n");
for (uint32_t i = 0; i < count; i++) {
    printf("  %s: score=%.4f\n", results[i].doc_id, results[i].score);
}
```

## 完整示例

```c
#include "db/storage/doc/doc_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    // 1. 初始化
    doc_engine_init("/tmp/docs");
    doc_engine_create("blog", NULL);
    
    void *doc_db = doc_engine_open("blog", ACCESS_MODE_READ_WRITE);
    
    // 2. 启用倒排索引
    doc_engine_enable_inverted_index(doc_db, "simple");
    
    // 3. 插入博客文章
    printf("插入文档...\n");
    
    typedef struct {
        const char *id;
        const char *title;
        const char *author;
        const char *content;
        const char *tags;
    } article_t;
    
    article_t articles[] = {
        {"a1", "C Programming Basics", "Alice", 
         "Learn the fundamentals of C programming. "
         "C is a powerful general-purpose programming language.",
         "C, programming, tutorial"},
        
        {"a2", "Advanced C Techniques", "Bob",
         "Master advanced C programming concepts including pointers, "
         "memory management, and data structures.",
         "C, advanced, pointers"},
        
        {"a3", "Python for Beginners", "Alice",
         "Python is an easy-to-learn programming language. "
         "Great for beginners and professionals alike.",
         "Python, tutorial, beginners"},
        
        {"a4", "Data Structures in C", "Charlie",
         "Implement common data structures in C including linked lists, "
         "trees, and graphs.",
         "C, data structures, algorithms"},
        
        {"a5", "Algorithm Design", "Bob",
         "Learn algorithm design techniques including divide and conquer, "
         "dynamic programming, and greedy algorithms.",
         "algorithms, design, programming"}
    };
    
    for (int i = 0; i < 5; i++) {
        // 构建 JSON
        char json[1024];
        snprintf(json, sizeof(json),
            "{"
            "\"title\":\"%s\","
            "\"author\":\"%s\","
            "\"content\":\"%s\","
            "\"tags\":\"%s\""
            "}",
            articles[i].title, articles[i].author,
            articles[i].content, articles[i].tags);
        
        doc_engine_insert(doc_db, articles[i].id, json, strlen(json));
        printf("  添加: %s\n", articles[i].title);
    }
    
    // 4. JSONPath 查询
    printf("\nJSONPath 查询:\n");
    
    doc_jsonpath_result_t *jp_results;
    int jp_count;
    
    // 按作者查询
    jp_count = doc_engine_query_jsonpath(doc_db, "$.author", &jp_results, 100);
    printf("  所有作者:\n");
    for (int i = 0; i < jp_count; i++) {
        printf("    %s\n", jp_results[i].value);
    }
    doc_engine_free_jsonpath_results(jp_results, jp_count);
    
    // 5. 全文搜索
    printf("\n全文搜索:\n");
    
    const char *queries[] = {"C programming", "algorithms", "Python tutorial"};
    
    for (int i = 0; i < 3; i++) {
        doc_inverted_result_t results[100];
        uint32_t count = doc_engine_inverted_search(doc_db, queries[i], 
                                                     results, 100);
        
        printf("  搜索 \"%s\" -> %u 个结果:\n", queries[i], count);
        for (uint32_t j = 0; j < count && j < 3; j++) {
            printf("    [%u] %s (score=%.4f)\n", 
                   j, results[j].doc_id, results[j].score);
        }
    }
    
    // 6. 特定字段搜索
    printf("\n仅搜索标题:\n");
    // 注意：当前实现搜索整个文档内容
    // 如需字段级搜索，需要扩展分词器和索引逻辑
    
    // 7. 保存倒排索引
    printf("\n保存倒排索引...\n");
    doc_engine_save_inverted_index(doc_db);
    
    // 8. 清理
    doc_engine_close(doc_db);
    doc_engine_shutdown();
    
    return 0;
}
```

## 分词器说明

当前支持的简单分词器：

| 分词器 | 说明 |
|--------|------|
| "simple" | 按空格和标点符号分词，小写转换 |
| NULL/"" | 使用默认分词器（同 "simple"） |

### 分词示例

```
输入: "Learn C Programming"
分词结果: ["learn", "c", "programming"]

输入: "Data Structures & Algorithms"
分词结果: ["data", "structures", "algorithms"]
```

## 性能调优

### 倒排索引内存

| 文档规模 | 估计内存 |
|----------|----------|
| 1,000 篇 | ~1 MB |
| 100,000 篇 | ~100 MB |
| 1,000,000 篇 | ~1 GB |

### 搜索性能

| 操作 | 时间复杂度 |
|------|------------|
| 术语查找 | O(1) 哈希 |
| 倒排列表合并 | O(n) |
| BM25 评分 | O(k log k) 其中 k 为结果数 |

## 常见问题

**Q: 如何实现中文分词？**
A: 需要集成中文分词库（如 jieba），扩展 doc_inverted_index 的分词逻辑。

**Q: 倒排索引支持更新吗？**
A: 当前实现需要重建索引来更新文档。未来版本将支持增量更新和墓碑机制。

**Q: JSONPath 支持正则表达式吗？**
A: 当前不支持正则，仅支持基本路径语法。

**Q: BM25 的 k1 和 b 参数如何调整？**
A: k1 控制词频影响（越大词频越重要），b 控制文档长度归一化（越大短文档权重越高）。根据数据特点调整。
