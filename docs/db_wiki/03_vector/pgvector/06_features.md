# pgvector 关键特性

## 学习目标

- 掌握 pgvector 的 SQL 接口和索引使用
- 理解 pgvector 的 PG 扩展实现机制

## SQL 接口

```sql
-- 启用扩展
CREATE EXTENSION vector;

-- 建表
CREATE TABLE items (
  id bigserial PRIMARY KEY,
  content text,
  embedding vector(128)
);

-- 插入
INSERT INTO items (content, embedding)
VALUES ('文本内容', '[0.1, 0.2, 0.3, ...]');

-- 余弦距离搜索（推荐）
SELECT id, content, 1 - (embedding <=> '[0.1,0.2,...]') AS similarity
FROM items
ORDER BY embedding <=> '[0.1,0.2,...]'
LIMIT 10;

-- L2 距离
ORDER BY embedding <-> '[0.1,0.2,...]'

-- 内积
ORDER BY embedding <#> '[0.1,0.2,...]'
```

## 索引

```sql
-- IVFFlat 索引（需要训练数据，需要先有数据）
CREATE INDEX ON items
USING ivfflat (embedding vector_cosine_ops)
WITH (lists = 100);
-- 搜索参数: lists 控制聚类中心数
SET ivfflat.probes = 10;

-- HNSW 索引（不需要训练，PG 16+ 支持）
CREATE INDEX ON items
USING hnsw (embedding vector_cosine_ops)
WITH (m = 16, ef_construction = 200);
-- 搜索参数
SET hnsw.ef_search = 100;
```

## 距离运算符

| 运算符 | 含义 | 索引操作类 |
|--------|------|-----------|
| `<->` | L2 欧氏距离 | vector_l2_ops |
| `<#>` | 负内积 | vector_ip_ops |
| `<=>` | 余弦距离 | vector_cosine_ops |

## 要点总结

- SQL 接口，零学习成本
- IVFFlat（需训练）和 HNSW（无需训练）两种索引
- 三种距离运算符覆盖主要度量
- 继承 PG 的事务/备份/复制/高可用能力

## 思考题

1. IVFFlat 的 lists 和 probes 参数与 Milvus 的 nlist/nprobe 是相同概念吗？
2. HNSW 索引在 PG 16+ 中才支持，背后的实现难点是什么？
3. pgvector 适合多大规模的向量数据？瓶颈在哪里？