# Meilisearch 实验指南

## 学习目标
- 掌握 Docker 部署 Meilisearch
- 学会索引管理和搜索操作
- 了解中文分词和高亮的实战用法

## 正文

### 实验环境：Docker 部署

```yaml
# docker-compose.yml
version: '3.8'
services:
  meilisearch:
    image: getmeili/meilisearch:v1.6
    container_name: meilisearch
    ports:
      - "7700:7700"
    environment:
      - MEILI_ENV=development
      - MEILI_MASTER_KEY=masterKey123
      - MEILI_DB_PATH=/meili_data
    volumes:
      - meili-data:/meili_data
    mem_limit: 512m

volumes:
  meili-data:
```

**启动命令**：
```bash
docker-compose up -d
# 验证服务
curl http://localhost:7700/health
# 响应: {"status":"available"}
```

---

### 实验一：索引创建与文档管理

**目标**：创建图书索引，导入测试数据

```bash
# 1. 创建索引（指定主键）
curl -X POST 'http://localhost:7700/indexes' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "uid": "books",
    "primaryKey": "id"
  }'

# 2. 配置可搜索字段
curl -X PATCH 'http://localhost:7700/indexes/books/settings' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "searchableAttributes": ["title", "author", "description", "tags"],
    "filterableAttributes": ["category", "price", "in_stock"],
    "sortableAttributes": ["price", "published_year"],
    "synonyms": {
      "programming": ["coding", "development"],
      "database": ["db", "data storage"]
    }
  }'

# 3. 批量导入文档
curl -X POST 'http://localhost:7700/indexes/books/documents' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '[
    {"id": 1, "title": "Python 编程入门", "author": "张三", "category": "编程", "price": 59.9, "in_stock": true, "published_year": 2023, "tags": ["python", "编程", "入门"]},
    {"id": 2, "title": "Rust 实战", "author": "李四", "category": "编程", "price": 89.9, "in_stock": true, "published_year": 2024, "tags": ["rust", "系统编程"]},
    {"id": 3, "title": "机器学习导论", "author": "王五", "category": "AI", "price": 129.9, "in_stock": false, "published_year": 2023, "tags": ["机器学习", "AI", "算法"]},
    {"id": 4, "title": "数据库原理", "author": "赵六", "category": "数据库", "price": 79.9, "in_stock": true, "published_year": 2022, "tags": ["数据库", "SQL"]},
    {"id": 5, "title": "Web 开发指南", "author": "张三", "category": "编程", "price": 69.9, "in_stock": true, "published_year": 2024, "tags": ["web", "前端", "后端"]}
  ]'

# 4. 查看索引状态
curl -X GET 'http://localhost:7700/indexes/books' \
  -H 'Authorization: Bearer masterKey123'
```

**验证标准**：
- 索引创建成功，文档数 = 5
- 可搜索字段包含 title, author, description, tags
- 过滤字段包含 category, price, in_stock

---

### 实验二：全文搜索

**目标**：执行各种搜索操作

```bash
# 1. 基本搜索
curl -X POST 'http://localhost:7700/indexes/books/search' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "q": "编程"
  }'

# 2. 多字段搜索
curl -X POST 'http://localhost:7700/indexes/books/search' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "q": "python 数据库",
    "attributesToSearchOn": ["title", "tags"]
  }'

# 3. 精确短语搜索
curl -X POST 'http://localhost:7700/indexes/books/search' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "q": "\"机器学习\"",
    "matchingStrategy": "all"
  }'

# 4. 前缀搜索
curl -X POST 'http://localhost:7700/indexes/books/search' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "q": "pyt"
  }'

# 5. 过滤搜索
curl -X POST 'http://localhost:7700/indexes/books/search' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "q": "编程",
    "filter": "in_stock = true AND price < 100",
    "sort": ["price:asc"]
  }'

# 6. 分页搜索
curl -X POST 'http://localhost:7700/indexes/books/search' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "q": "编程",
    "limit": 2,
    "offset": 0
  }'
```

**验证标准**：
- 搜索 "编程" 返回 3 本书（Python、Rust、Web）
- 过滤 "in_stock = true" 返回有货的书籍
- 排序按价格升序

---

### 实验三：高亮显示

**目标**：使用高亮功能显示匹配位置

```bash
# 1. 高亮显示
curl -X POST 'http://localhost:7700/indexes/books/search' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "q": "编程",
    "attributesToHighlight": ["title", "description"],
    "highlightPreTag": "<em>",
    "highlightPostTag": "</em>"
  }'

# 2. 仅返回格式化字段
curl -X POST 'http://localhost:7700/indexes/books/search' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "q": "数据库",
    "attributesToHighlight": ["title", "tags"],
    "showMatchesPosition": true
  }'

# 响应示例
{
  "hits": [
    {
      "id": 4,
      "title": "数据库原理",
      "tags": ["数据库", "SQL"],
      "_formatted": {
        "title": "<em>数据库</em>原理",
        "tags": ["<em>数据库</em>", "SQL"]
      },
      "_matchesPosition": {
        "title": [{"start": 0, "length": 3, "match": "数据库"}],
        "tags": [{"start": 0, "length": 3, "match": "数据库"}]
      }
    }
  ]
}
```

**验证标准**：
- `_formatted` 字段包含高亮后的文本
- `<em>` 标签正确包裹匹配词
- `_matchesPosition` 显示匹配位置

---

### 实验四：中文分词

**目标**：配置中文分词器（使用 jieba）

> **注意**：Meilisearch 默认使用 N-Gram 分词，对中文支持有限。以下是使用第三方分词器的方案。

```bash
# 方案一：使用 N-Gram 分词（默认）
# Meilisearch 会将中文按字符分割

# 1. 查看分词效果
curl -X POST 'http://localhost:7700/indexes/books/search' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "q": "机器"
  }'

# 方案二：自定义分词器（需要修改源码）
# Meilisearch 官方计划在 v2.0 支持自定义分词器

# 方案三：预分词后导入
# 1. 预分词处理
# 原始: "机器学习导论"
# 预分词: "机器学习 导论"
# 导入字段: "title": "机器学习导论", "title_tokens": ["机器学习", "导论"]

# 添加预分词字段
curl -X POST 'http://localhost:7700/indexes/books/documents' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '[
    {"id": 6, "title": "深度学习实战", "author": "孙七", "category": "AI", "price": 149.9, "in_stock": true, "published_year": 2024, "tags": ["深度学习", "神经网络", "AI"], "title_tokens": ["深度学习", "实战"]},
    {"id": 7, "title": "自然语言处理", "author": "周八", "category": "AI", "price": 119.9, "in_stock": true, "published_year": 2024, "tags": ["NLP", "自然语言"], "title_tokens": ["自然语言处理", "自然语言"]}
  ]'

# 2. 更新可搜索字段
curl -X PATCH 'http://localhost:7700/indexes/books/settings' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "searchableAttributes": ["title", "title_tokens", "author", "tags"]
  }'

# 3. 搜索验证
curl -X POST 'http://localhost:7700/indexes/books/search' \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer masterKey123' \
  -d '{
    "q": "自然语言"
  }'
```

**验证标准**：
- 默认 N-Gram 按字符分割
- 预分词方案能搜索词组

## 要点总结

1. **索引配置**：searchableAttributes、filterableAttributes、sortableAttributes 三大配置
2. **搜索语法**：基础搜索、过滤、排序、分页组合使用
3. **高亮功能**：_formatted 字段返回高亮文本，_matchesPosition 显示位置
4. **中文支持**：当前版本依赖 N-Gram，预分词是可行的 workaround
5. **API 设计**：RESTful API，简洁易用，SDK 覆盖多语言

## 思考题

1. Meilisearch 的 N-Gram 分词对中文搜索有什么影响？如何优化？
2. 拼写容错在中文场景下是否有效？为什么？
3. 如何实现搜索词的历史记录和热门搜索推荐？
