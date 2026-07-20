# ParadeDB 实验指南

## 学习目标
- 掌握 Docker Compose 部署 ParadeDB
- 学会创建 BM25 和 HNSW 索引
- 理解混合搜索的实战操作

## 正文

### 实验环境：Docker Compose 部署

```yaml
# docker-compose.yml
version: '3.8'
services:
  postgres:
    image: paradedb/paradedb:latest
    container_name: paradedb
    environment:
      - POSTGRES_USER=postgres
      - POSTGRES_PASSWORD=password
      - POSTGRES_DB=search_demo
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data
    shm_size: '256mb'  # 向量索引需要共享内存

volumes:
  pgdata:
```

**启动命令**：
```bash
docker-compose up -d
# 等待服务启动
sleep 10
# 连接数据库
psql -h localhost -U postgres -d search_demo
```

---

### 实验一：BM25 全文搜索

**目标**：创建商品搜索索引，执行全文搜索

```sql
-- 1. 创建商品表
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    category TEXT,
    price NUMERIC,
    brand TEXT,
    in_stock BOOLEAN DEFAULT true
);

-- 2. 创建 BM25 索引
CREATE INDEX idx_products_bm25 ON products 
USING bm25 (products) WITH (
    text_search_fields = '{name, description, brand}',
    tokenizer = 'default'
);

-- 3. 插入测试数据
INSERT INTO products (name, description, category, price, brand) VALUES
('iPhone 15 Pro', 'Apple flagship smartphone with A17 chip', 'Electronics', 999.99, 'Apple'),
('Samsung Galaxy S24', 'Android flagship with AI features', 'Electronics', 899.99, 'Samsung'),
('MacBook Pro 14', 'Professional laptop with M3 Pro chip', 'Computers', 1999.99, 'Apple'),
('Dell XPS 15', 'Premium Windows laptop', 'Computers', 1499.99, 'Dell'),
('Sony WH-1000XM5', 'Premium noise-canceling headphones', 'Audio', 349.99, 'Sony'),
('Bose QuietComfort', 'Comfortable noise-canceling headphones', 'Audio', 279.99, 'Bose'),
('Nike Air Max', 'Comfortable running shoes', 'Sports', 129.99, 'Nike'),
('Adidas Ultraboost', 'Premium running shoes', 'Sports', 179.99, 'Adidas');

-- 4. 基本搜索
SELECT 
    id, name, brand, price,
    bm25(products) AS score
FROM products
WHERE bm25(products, query => 'Apple laptop') USING must
ORDER BY score DESC;

-- 5. 带过滤的搜索
SELECT * FROM products
WHERE bm25(products, query => 'headphones', 
                 filter => 'category = ''Audio'' AND price < 300') USING must
ORDER BY bm25(products) DESC;

-- 6. 高亮显示
SELECT 
    id, name,
    ts_headline(name, query => bm25(products, query => 'laptop')) AS highlighted_name
FROM products
WHERE bm25(products, query => 'laptop') USING must;
```

**验证标准**：
- 索引创建成功
- 搜索 "Apple laptop" 返回 MacBook Pro 和 iPhone
- 过滤搜索正确筛选 Audio 类别

---

### 实验二：HNSW 向量搜索

**目标**：创建向量索引，执行相似度搜索

```sql
-- 1. 扩展向量维度（需要支持向量类型）
-- 注意：ParadeDB 使用 pgvector 的向量类型

-- 2. 创建文档表
CREATE TABLE documents (
    id SERIAL PRIMARY KEY,
    content TEXT NOT NULL,
    category TEXT,
    embedding VECTOR(4)  -- 使用 4 维简化演示
);

-- 3. 创建 HNSW 索引
CREATE INDEX idx_documents_hnsw ON documents
USING hnsw (embedding vector_cosine_ops)
WITH (m = 16, ef_construction = 64);

-- 4. 插入向量数据
INSERT INTO documents (content, category, embedding) VALUES
('Apple laptop professional', 'computers', '[0.9, 0.1, 0.2, 0.1]'),
('Budget smartphone affordable', 'electronics', '[0.1, 0.9, 0.1, 0.2]'),
('Running shoes sport comfort', 'sports', '[0.1, 0.1, 0.9, 0.2]'),
('Wireless headphones music', 'audio', '[0.2, 0.1, 0.1, 0.9]'),
('Gaming laptop high performance', 'computers', '[0.8, 0.2, 0.3, 0.1]');

-- 5. 余弦相似度搜索
SELECT 
    id, content, category,
    1 - (embedding <=> '[0.85, 0.1, 0.2, 0.1]') AS similarity
FROM documents
ORDER BY embedding <=> '[0.85, 0.1, 0.2, 0.1]'
LIMIT 3;

-- 6. 带预过滤的向量搜索
SELECT * FROM documents
WHERE category = 'computers'
ORDER BY embedding <=> '[0.85, 0.1, 0.2, 0.1]'
LIMIT 2;
```

**验证标准**：
- HNSW 索引创建成功
- 向量搜索返回最相似的结果
- 预过滤正确工作

---

### 实验三：混合搜索

**目标**：结合 BM25 和向量搜索

```sql
-- 1. 混合搜索查询
SELECT 
    p.id,
    p.name,
    p.category,
    p.price,
    bm25(products, query => 'laptop') AS text_score,
    -- 假设有一个向量列
    (SELECT 1 - (embedding <=> '[0.85, 0.1, 0.2, 0.1]') 
     FROM products WHERE id = p.id) AS vector_score
FROM products p
WHERE bm25(products, query => 'laptop') USING should
ORDER BY 
    -- RRF 融合
    1.0 / (60 + RANK() OVER (ORDER BY bm25(products, query => 'laptop') DESC)) +
    1.0 / (60 + (SELECT RANK() OVER (ORDER BY 1 - (embedding <=> '[0.85, 0.1, 0.2, 0.1]') DESC) 
                 FROM products WHERE id = p.id)) DESC;

-- 2. 搜索性能对比
EXPLAIN ANALYZE
SELECT * FROM products
WHERE bm25(products, query => 'laptop') USING must
LIMIT 10;
```

**验证标准**：
- 混合搜索正确融合文本和向量分数
- RRF 融合使两个搜索的结果都能体现

## 要点总结

1. **Docker 部署**：ParadeDB 以 PostgreSQL 镜像形式分发，一键启动
2. **BM25 索引**：使用 `USING bm25` 创建索引，配置 `text_search_fields`
3. **HNSW 索引**：使用 `USING hnsw` 创建向量索引，配置 `m` 和 `ef_construction`
4. **混合搜索**：通过 RRF 融合算法结合 BM25 和向量搜索
5. **SQL 接口**：所有操作使用标准 SQL，无需学习新语法

## 思考题

1. HNSW 索引的 `m` 和 `ef_construction` 参数如何影响性能和精度？
2. 在混合搜索中，RRF 的 k 值（60）如何选择？
3. 如何监控 BM25 和 HNSW 索引的查询性能？
