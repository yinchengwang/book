# Elasticsearch 实验指南

## 学习目标
- 掌握 Docker Compose 部署 ES 集群
- 学会创建索引和配置分词器
- 理解全文搜索和聚合分析的实战操作

## 正文

### 实验环境：Docker Compose 部署

```yaml
# docker-compose.yml
version: '3.8'
services:
  elasticsearch:
    image: elasticsearch:8.11.0
    container_name: es-node-1
    environment:
      - node.name=es-node-1
      - cluster.name=es-cluster
      - discovery.type=single-node
      - xpack.security.enabled=false
      - "ES_JAVA_OPTS=-Xms512m -Xmx512m"
    ports:
      - "9200:9200"
      - "9300:9300"
    volumes:
      - es-data:/usr/share/elasticsearch/data
    mem_limit: 1g

  kibana:
    image: kibana:8.11.0
    container_name: kibana
    environment:
      - ELASTICSEARCH_HOSTS=http://es-node-1:9200
    ports:
      - "5601:5601"
    depends_on:
      - elasticsearch

volumes:
  es-data:
```

**启动命令**：
```bash
docker-compose up -d
# 等待 30 秒启动
sleep 30
# 验证集群状态
curl http://localhost:9200/_cluster/health?pretty
```

---

### 实验一：索引创建与分词器配置

**目标**：创建一个支持中英文搜索的博客索引

```bash
# 1. 创建索引并配置中文分词器
curl -X PUT "localhost:9200/blogs" -H 'Content-Type: application/json' -d'
{
  "settings": {
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "analysis": {
      "analyzer": {
        "ik_analyzer": {
          "type": "custom",
          "tokenizer": "ik_max_word",
          "filter": ["lowercase"]
        },
        "pinyin_analyzer": {
          "type": "custom",
          "tokenizer": "pinyin_tokenizer",
          "filter": ["lowercase"]
        }
      }
    }
  },
  "mappings": {
    "properties": {
      "title": {
        "type": "text",
        "analyzer": "ik_analyzer",
        "fields": {
          "keyword": { "type": "keyword" },
          "pinyin": { "type": "text", "analyzer": "pinyin_analyzer" }
        }
      },
      "content": {
        "type": "text",
        "analyzer": "ik_analyzer"
      },
      "author": { "type": "keyword" },
      "tags": { "type": "keyword" },
      "views": { "type": "integer" },
      "published_at": { "type": "date" }
    }
  }
}
'

# 2. 查看索引信息
curl "localhost:9200/blogs?pretty"

# 3. 查看分词效果
curl -X POST "localhost:9200/blogs/_analyze" -H 'Content-Type: application/json' -d'
{
  "analyzer": "ik_analyzer",
  "text": "Elasticsearch 是最流行的搜索引擎"
}
'
```

**验证标准**：
- 索引创建成功
- 分词结果包含 "Elasticsearch"、"流行"、"搜索引擎" 等词项

---

### 实验二：全文搜索实战

**目标**：实现多字段搜索、高亮显示、结果排序

```bash
# 1. 批量导入测试数据
curl -X POST "localhost:9200/blogs/_bulk" -H 'Content-Type: application/json' -d'
{"index":{"_id":"1"}}
{"title":"Elasticsearch 入门指南","content":"本文介绍 Elasticsearch 的基本概念和使用方法，包括索引创建、文档操作和搜索查询。","author":"张三","tags":["elasticsearch","搜索"],"views":1500,"published_at":"2024-01-10"}
{"index":{"_id":"2"}}
{"title":"全文搜索引擎对比","content":"对比 Elasticsearch、Solr、Meilisearch 等主流全文搜索引擎的特点和适用场景。","author":"李四","tags":["搜索引擎","对比"],"views":2300,"published_at":"2024-01-15"}
{"index":{"_id":"3"}}
{"title":"分布式搜索架构设计","content":"讨论如何设计高可用、高性能的分布式搜索系统，包括分片策略和副本机制。","author":"王五","tags":["架构","分布式"],"views":3200,"published_at":"2024-01-20"}
{"index":{"_id":"4"}}
{"title":"电商搜索优化实践","content":"分享电商平台搜索功能优化的经验，包括相关性调优、性能优化和用户体验改进。","author":"张三","tags":["电商","优化"],"views":4100,"published_at":"2024-01-25"}
{"index":{"_id":"5"}}
{"title":"日志分析 ELK 实战","content":"使用 ELK Stack 构建企业级日志分析平台，包括 Logstash 配置和 Kibana 可视化。","author":"赵六","tags":["elk","日志"],"views":2800,"published_at":"2024-01-30"}
'

# 2. 多字段搜索
curl -X POST "localhost:9200/blogs/_search" -H 'Content-Type: application/json' -d'
{
  "query": {
    "multi_match": {
      "query": "搜索 引擎",
      "fields": ["title^2", "content", "tags"]
    }
  },
  "highlight": {
    "fields": {
      "title": {},
      "content": {}
    },
    "pre_tags": ["<em>"],
    "post_tags": ["</em>"]
  }
}
'

# 3. 带过滤的搜索
curl -X POST "localhost:9200/blogs/_search" -H 'Content-Type: application/json' -d'
{
  "query": {
    "bool": {
      "must": [
        { "match": { "content": "搜索" } }
      ],
      "filter": [
        { "term": { "author": "张三" } },
        { "range": { "views": { "gte": 1000 } } }
      ]
    }
  },
  "sort": [
    { "views": "desc" },
    { "_score": "desc" }
  ]
}
'
```

**验证标准**：
- 搜索 "搜索 引擎" 返回相关结果
- 高亮标签正确包裹匹配词
- 张三的博客排序靠前（因为 title 权重 2x）

---

### 实验三：聚合分析实战

**目标**：统计标签分布、作者贡献、浏览量分布

```bash
# 1. 标签聚合（桶聚合 + 指标聚合）
curl -X POST "localhost:9200/blogs/_search" -H 'Content-Type: application/json' -d'
{
  "size": 0,
  "aggs": {
    "tag_distribution": {
      "terms": {
        "field": "tags",
        "size": 10
      },
      "aggs": {
        "total_views": {
          "sum": { "field": "views" }
        },
        "avg_views": {
          "avg": { "field": "views" }
        }
      }
    }
  }
}
'

# 2. 作者贡献统计
curl -X POST "localhost:9200/blogs/_search" -H 'Content-Type: application/json' -d'
{
  "size": 0,
  "aggs": {
    "by_author": {
      "terms": {
        "field": "author",
        "size": 10
      },
      "aggs": {
        "blog_count": { "value_count": { "field": "_id" } },
        "total_views": { "sum": { "field": "views" } },
        "max_views": { "max": { "field": "views" } }
      }
    }
  }
}
'

# 3. 浏览量分布（直方图）
curl -X POST "localhost:9200/blogs/_search" -H 'Content-Type: application/json' -d'
{
  "size": 0,
  "aggs": {
    "views_distribution": {
      "histogram": {
        "field": "views",
        "interval": 1000
      }
    }
  }
}
'
```

**验证标准**：
- 标签聚合显示各标签的文章数和总浏览量
- 作者聚合显示每人发布的文章数和总浏览量
- 直方图显示浏览量的分布区间

---

### 实验四：集群故障模拟

**目标**：了解 ES 的容错机制和恢复过程

```bash
# 1. 查看集群健康状态
curl "localhost:9200/_cluster/health?pretty"

# 2. 查看分片分配情况
curl "localhost:9200/_cat/shards?v"

# 3. 模拟主分片故障（停止容器）
docker stop es-node-1

# 4. 再次检查集群状态（应为 yellow/red）
curl "localhost:9200/_cluster/health?pretty"

# 5. 恢复容器
docker start es-node-1

# 6. 观察分片重新分配
watch -n 2 'curl -s "localhost:9200/_cat/shards?v" | grep -v "UNASSIGNED"'
```

**验证标准**：
- 单节点模式下，副本分片为 unassigned（黄色）
- 节点恢复后，分片自动重新分配
- 无数据丢失

## 要点总结

1. **环境部署**：Docker Compose 快速搭建开发环境，注意内存限制配置
2. **索引设计**：分词器选择决定搜索质量，字段类型影响聚合能力
3. **搜索优化**：多字段权重、高亮展示、结果排序是用户体验关键
4. **聚合价值**：不只是统计，还能用于 facet 过滤、发现数据模式
5. **集群容错**：副本机制保障高可用，分片重分配自动进行

## 思考题

1. 为什么单节点 ES 集群总是 yellow 状态？如何解决？
2. 如何设计索引来支持近实时搜索（写入后秒级可查）？
3. 在高写入场景下，如何配置 Refresh Interval 来平衡写入和查询性能？
