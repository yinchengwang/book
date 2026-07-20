# ZincSearch 实验指南

## 学习目标
- 掌握 Docker 部署 ZincSearch
- 学会创建索引和导入数据
- 理解全文搜索和聚合分析的实战操作

## 正文

### 实验环境：Docker 部署

```yaml
# docker-compose.yml
version: '3.8'
services:
  zincsearch:
    image: eczanne/zincsearch:latest
    container_name: zincsearch
    ports:
      - "4080:4080"
    environment:
      - DATA_PATH=/data
      - BASIC_AUTH_USER=admin
      - BASIC_AUTH_PASSWORD=admin123
    volumes:
      - zinc-data:/data
    mem_limit: 512m

volumes:
  zinc-data:
```

**启动命令**：

```bash
docker-compose up -d
# 访问 Web UI: http://localhost:4080
# 默认账号: admin / admin123
```

---

### 实验一：索引创建与数据导入

**目标**：创建电影索引，导入测试数据

```bash
# 1. 创建索引
curl -X PUT 'http://localhost:4080/api/default/movies' \
  -u 'admin:admin123'

# 2. 批量导入文档
curl -X POST 'http://localhost:4080/api/default/movies/_bulk' \
  -u 'admin:admin123' \
  -H 'Content-Type: application/json' \
  -d '{"index": {"_id": "1"}}
{"title": "The Matrix", "genre": "Sci-Fi", "year": 1999, "rating": 8.7, "description": "A computer hacker learns about the true nature of reality."}
{"index": {"_id": "2"}}
{"title": "Inception", "genre": "Sci-Fi", "year": 2010, "rating": 8.8, "description": "A thief who steals corporate secrets through dream-sharing technology."}
{"index": {"_id": "3"}}
{"title": "The Dark Knight", "genre": "Action", "year": 2008, "rating": 9.0, "description": "Batman faces the Joker, a criminal mastermind."}
{"index": {"_id": "4"}}
{"title": "Pulp Fiction", "genre": "Crime", "year": 1994, "rating": 8.9, "description": "Various interconnected stories of criminals in Los Angeles."}
{"index": {"_id": "5"}}
{"title": "Forrest Gump", "genre": "Drama", "year": 1994, "rating": 8.8, "description": "The story of a man with a low IQ who accomplishes great things."}
'

# 3. 验证导入
curl -X GET 'http://localhost:4080/api/default/movies/_count' \
  -u 'admin:admin123'
```

**验证标准**：
- 索引创建成功
- 文档数 = 5
- 查询 `/_count` 返回 `{"count": 5}`

---

### 实验二：全文搜索

**目标**：执行各种搜索操作

```bash
# 1. 基本搜索
curl -X POST 'http://localhost:4080/api/default/movies/_search' \
  -u 'admin:admin123' \
  -H 'Content-Type: application/json' \
  -d '{
    "query": { "match": { "description": "criminal" } }
  }'

# 2. 多字段搜索（title 和 description）
curl -X POST 'http://localhost:4080/api/default/movies/_search' \
  -u 'admin:admin123' \
  -H 'Content-Type: application/json' \
  -d '{
    "query": {
      "multi_match": {
        "query": "batman hero",
        "fields": ["title^2", "description"]
      }
    }
  }'

# 3. 精确词项搜索
curl -X POST 'http://localhost:4080/api/default/movies/_search' \
  -u 'admin:admin123' \
  -H 'Content-Type: application/json' \
  -d '{
    "query": { "term": { "genre": "Sci-Fi" } }
  }'

# 4. 范围查询
curl -X POST 'http://localhost:4080/api/default/movies/_search' \
  -u 'admin:admin123' \
  -H 'Content-Type: application/json' \
  -d '{
    "query": {
      "bool": {
        "must": [
          { "range": { "rating": { "gte": 8.8 } } },
          { "range": { "year": { "gte": 2000 } } }
        ]
      }
    }
  }'

# 5. 高亮显示
curl -X POST 'http://localhost:4080/api/default/movies/_search' \
  -u 'admin:admin123' \
  -H 'Content-Type: application/json' \
  -d '{
    "query": { "match": { "description": "dream" } },
    "highlight": {
      "fields": {
        "description": {}
      }
    }
  }'
```

**验证标准**：
- 搜索 "criminal" 返回 Pulp Fiction
- 搜索 "batman" 在 title 中权重更高
- 范围查询返回高评分近期电影

---

### 实验三：聚合分析

**目标**：统计电影分布和评分情况

```bash
# 1. 按类型统计
curl -X POST 'http://localhost:4080/api/default/movies/_search' \
  -u 'admin:admin123' \
  -H 'Content-Type: application/json' \
  -d '{
    "size": 0,
    "aggs": {
      "genres": {
        "terms": { "field": "genre.keyword", "size": 10 }
      }
    }
  }'

# 2. 评分统计
curl -X POST 'http://localhost:4080/api/default/movies/_search' \
  -u 'admin:admin123' \
  -H 'Content-Type: application/json' \
  -d '{
    "size": 0,
    "aggs": {
      "rating_stats": {
        "stats": { "field": "rating" }
      },
      "avg_rating": {
        "avg": { "field": "rating" }
      }
    }
  }'

# 3. 按年份分布
curl -X POST 'http://localhost:4080/api/default/movies/_search' \
  -u 'admin:admin123' \
  -H 'Content-Type: application/json' \
  -d '{
    "size": 0,
    "aggs": {
      "year_distribution": {
        "histogram": { "field": "year", "interval": 10 }
      }
    }
  }'
```

**验证标准**：
- 类型统计显示各类型的电影数量
- 评分统计显示平均分、最高分等
- 年份分布显示各年代的电影分布

## 要点总结

1. **Docker 部署**：一键启动，无需复杂配置
2. **ES 兼容 API**：使用熟悉的 ES API 操作 ZincSearch
3. **批量导入**：支持 `_bulk` 接口快速导入数据
4. **搜索功能**：match/term/range/multi_match 等查询类型
5. **聚合分析**：terms/stats/avg/histogram 等聚合

## 思考题

1. ZincSearch 的 `_bulk` 导入与 Elasticsearch 有什么差异？
2. 如何监控 ZincSearch 的索引状态和资源使用？
3. 在高写入场景下，ZincSearch 的性能如何？
