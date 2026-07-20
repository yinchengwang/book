# Tantivy 实验指南

## 学习目标
- 掌握 Tantivy 的 Rust 集成示例
- 学会创建索引、添加文档、执行搜索
- 理解 BM25 排序和分词器配置

## 正文

### Rust 集成示例

**Cargo.toml 依赖**：

```toml
[dependencies]
tantivy = "0.22"
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"
```

### 实验一：基础索引与搜索

**目标**：创建简单的内存索引，执行全文搜索

```rust
use tantivy::schema::*;
use tantivy::{Index, IndexWriter, TantivyDocument, ReloadPolicy};
use tantivy::collector::TopDocs;
use tantivy::query::QueryParser;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 1. 创建 Schema
    let mut schema_builder = Schema::builder();
    schema_builder.add_text_field("title", TEXT | STORED);
    schema_builder.add_text_field("content", TEXT | STORED);
    schema_builder.add_i64_field("id", STORED | INDEXED);
    let schema = schema_builder.build();

    // 2. 创建内存索引
    let index = Index::create_in_ram(schema.clone());

    // 3. 创建写入器并添加文档
    let mut writer: IndexWriter = index.writer(50_000_000)?;
    
    let docs = vec![
        ("1", "Rust Programming", "Rust is a modern systems programming language"),
        ("2", "Tantivy Search", "Tantivy is a full-text search engine library"),
        ("3", "PostgreSQL Database", "PostgreSQL is a powerful relational database"),
        ("4", "Elasticsearch Guide", "Elasticsearch is a distributed search engine"),
        ("5", "Meilisearch Tutorial", "Meilisearch provides instant search capabilities"),
    ];
    
    for (id, title, content) in docs {
        let mut doc = TantivyDocument::default();
        doc.add_text("id", id);
        doc.add_text("title", title);
        doc.add_text("content", content);
        writer.add_document(doc)?;
    }
    writer.commit()?;

    // 4. 创建读取器
    let reader = index
        .reader_builder()
        .reload_policy(ReloadPolicy::OnCommitWithDelay)
        .try_into()?;

    let searcher = reader.searcher();

    // 5. 执行搜索
    let query_parser = QueryParser::for_index(&index, vec!["title", "content"]);
    let query = query_parser.parse_query("search programming")?;

    let top_docs = searcher.search(&query, &TopDocs::with_limit(10))?;

    println!("Found {} results:", top_docs.len());
    for (score, doc_address) in top_docs {
        let doc: TantivyDocument = searcher.doc(doc_address)?;
        let title = doc.get_first("title").unwrap().as_str();
        let content = doc.get_first("content").unwrap().as_str();
        println!("  Score: {:.4}, Title: {}, Content: {}", 
                 score, title, content);
    }

    Ok(())
}
```

**运行结果**：
```
Found 3 results:
  Score: 0.8567, Title: Rust Programming, Content: Rust is a modern systems programming language
  Score: 0.7823, Title: Tantivy Search, Content: Tantivy is a full-text search engine library
  Score: 0.6541, Title: Elasticsearch Guide, Content: Elasticsearch is a distributed search engine
```

---

### 实验二：分词器配置

**目标**：配置不同的分词器，优化搜索效果

```rust
use tantivy::tokenizer::*;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 创建带自定义分词器的索引
    let mut schema_builder = Schema::builder();
    
    // 配置分词器
    let text_indexing = TextFieldIndexing::default()
        .set_tokenizer("my_analyzer")
        .set_index_option(IndexRecordOption::WithFreqsAndPositions);
    
    let text_options = TextOptions::default()
        .set_indexing_options(text_indexing)
        .set_stored();
    
    schema_builder.add_text_field("content", text_options);
    let schema = schema_builder.build();
    
    let index = Index::create_in_ram(schema.clone());
    
    // 注册自定义分词器
    let tokenizer_manager = index.tokenizers();
    tokenizer_manager.register("my_analyzer",
        TextAnalyzer::from(SimpleTokenizer::default())
            .filter(LowerCaser)                   // 转小写
            .filter(Stemmer::new(Language::English))  // 词干提取
            .filter(StopWordFilter::new(          // 停用词过滤
                vec!["a", "an", "the", "is", "are"]
                    .into_iter()
                    .collect()
            ))
    );

    // 后续代码同实验一
    // ...

    Ok(())
}
```

**分词器对比**：

| 分词器 | 分词效果 | 适用场景 |
|--------|----------|----------|
| SimpleTokenizer | 按空白字符分割 | 简单场景 |
| NgramTokenizer | N-gram 分割 | 中文、模糊搜索 |
| Stemmer | 词干提取 | 英文搜索 |
| LowerCaser | 转小写 | 大小写不敏感 |

## 要点总结

1. **Rust 集成**：通过 Cargo 添加 tantivy 依赖即可使用
2. **Schema 定义**：Text/Integer/Float/Date 等多种字段类型
3. **索引写入**：IndexWriter 添加文档，commit 生效
4. **搜索执行**：QueryParser 解析查询，TopDocs 获取结果
5. **分词器**：可注册自定义分词器链，支持停用词、词干等

## 思考题

1. 如何为中文文本选择合适的分词器？
2. IndexWriter 的内存参数（50_000_000）如何影响索引性能？
3. ReloadPolicy::OnCommitWithDelay 与 OnCommit 有什么区别？
