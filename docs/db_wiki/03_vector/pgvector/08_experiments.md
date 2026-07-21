# 动手实验: pgvector 部署与使用

## 学习目标

- 掌握 pgvector 的安装和基本操作
- 对比 IVFFlat 和 HNSW 索引的性能差异
- 理解召回率与 QPS 的权衡

## 实验环境

### Docker 部署 PostgreSQL + pgvector

```bash
# 启动带 pgvector 的 PostgreSQL
docker run -d \
    --name pgvector \
    -e POSTGRES_PASSWORD=postgres \
    -e POSTGRES_DB=vector_demo \
    -p 5432:5432 \
    pgvector/pgvector:pg16

# 验证安装
docker exec -it pgvector psql -U postgres -d vector_demo -c "SELECT extversion FROM pg_extension WHERE extname = 'vector';"
```

### 或者在已有 PG 上安装

```bash
# Ubuntu/Debian
apt-get install postgresql-16-pgvector

# 或者从源码编译
git clone https://github.com/pgvector/pgvector.git
cd pgvector
make && make install
```

## 基本操作

### 1. 启用扩展

```sql
CREATE EXTENSION IF NOT EXISTS vector;
```

### 2. 建表并插入数据

```sql
-- 创建测试表
CREATE TABLE documents (
    id SERIAL PRIMARY KEY,
    title TEXT,
    content TEXT,
    embedding vector(128)
);

-- 插入测试数据
INSERT INTO documents (title, content, embedding)
SELECT
    '文档 ' || i,
    '这是第 ' || i || ' 篇文档的内容',
    array_agg(random()::float)::vector(128)
FROM generate_series(1, 10000) i
GROUP BY i;

-- 验证数据
SELECT count(*) FROM documents;
SELECT embedding FROM documents LIMIT 1;
```

### 3. 基本向量搜索

```sql
-- 准备查询向量
SET _vectorstored.embedding = '[0.1, 0.2, ...]::vector(128)';
-- 或者用 Python 生成

-- 余弦距离搜索
SELECT id, title,
       1 - (embedding <=> $1::vector) AS similarity
FROM documents
ORDER BY embedding <=> $1::vector
LIMIT 10;
```

## 索引对比实验

### 创建不同类型索引

```sql
-- 无索引（基准）
SET enable_indexscan = off;
SET enable_seqscan = on;

-- IVFFlat 索引
CREATE INDEX idx_ivfflat ON documents
USING ivfflat (embedding vector_cosine_ops)
WITH (lists = 100);

-- HNSW 索引
CREATE INDEX idx_hnsw ON documents
USING hnsw (embedding vector_cosine_ops)
WITH (m = 16, ef_construction = 200);
```

### 召回率测试

```python
import psycopg2
import numpy as np
from datetime import datetime

conn = psycopg2.connect(host="localhost", database="vector_demo")

def get_ground_truth(cursor, query_vec, limit=100):
    """获取精确搜索结果（无索引）"""
    cursor.execute("""
        SET enable_seqscan = off;
        SELECT id FROM documents
        ORDER BY embedding <=> %s::vector
        LIMIT %s
    """, (query_vec, limit))
    return set([row[0] for row in cursor.fetchall()])

def get_candidates(cursor, query_vec, index_name, limit=100):
    """获取索引搜索结果"""
    cursor.execute(f"""
        SET max_parallel_workers_per_gather = 0;
        SELECT id FROM documents
        ORDER BY embedding <=> %s::vector
        LIMIT %s
    """, (query_vec, limit))
    return [row[0] for row in cursor.fetchall()]

def recall(result, ground_truth):
    """计算召回率"""
    result_set = set(result[:len(ground_truth)])
    return len(result_set & ground_truth) / len(ground_truth)

# 生成随机查询向量
query = np.random.rand(128).tolist()
query_vec = "[" + ",".join(map(str, query)) + "]"

# 精确结果
cursor = conn.cursor()
ground_truth = get_ground_truth(cursor, query_vec, limit=100)

# 各索引召回率
print(f"Ground truth: {len(ground_truth)} items")
print(f"IVFFlat recall@100: {recall(get_candidates(cursor, query_vec, 'ivfflat', 100), ground_truth):.4f}")
print(f"HNSW recall@100: {recall(get_candidates(cursor, query_vec, 'hnsw', 100), ground_truth):.4f}")
```

### 性能基准测试

```python
import time

def benchmark(cursor, query_vec, index_name, iterations=100):
    """QPS 基准测试"""
    times = []
    for _ in range(iterations):
        start = time.time()
        cursor.execute(f"""
            SELECT id FROM documents
            ORDER BY embedding <=> %s::vector
            LIMIT 20
        """, (query_vec,))
        cursor.fetchall()
        times.append(time.time() - start)

    avg_time = sum(times) / len(times)
    qps = 1 / avg_time
    print(f"{index_name}: avg={avg_time*1000:.2f}ms, QPS={qps:.0f}")
    return qps

# 测试不同索引
benchmark(cursor, query_vec, "No Index")
benchmark(cursor, query_vec, "IVFFlat (lists=100, probes=10)")
benchmark(cursor, query_vec, "HNSW (m=16, ef=200)")
```

## 索引参数调优实验

### IVFFlat 参数影响

```sql
-- 不同 lists 值
CREATE INDEX idx_ivfflat_50 ON documents
USING ivfflat (embedding vector_cosine_ops)
WITH (lists = 50);

CREATE INDEX idx_ivfflat_200 ON documents
USING ivfflat (embedding vector_cosine_ops)
WITH (lists = 200);

CREATE INDEX idx_ivfflat_500 ON documents
USING ivfflat (embedding vector_cosine_ops)
WITH (lists = 500);

-- 查询时调整 probes
SET ivfflat.probes = 10;   -- 默认
SET ivfflat.probes = 50;
SET ivfflat.probes = 100;
```

### HNSW 参数影响

```sql
-- 不同 m 值
CREATE INDEX idx_hnsw_m8 ON documents
USING hnsw (embedding vector_cosine_ops)
WITH (m = 8, ef_construction = 200);

CREATE INDEX idx_hnsw_m32 ON documents
USING hnsw (embedding vector_cosine_ops)
WITH (m = 32, ef_construction = 200);

-- 不同 ef_construction
CREATE INDEX idx_hnsw_ef100 ON documents
USING hnsw (embedding vector_cosine_ops)
WITH (m = 16, ef_construction = 100);

CREATE INDEX idx_hnsw_ef400 ON documents
USING hnsw (embedding vector_cosine_ops)
WITH (m = 16, ef_construction = 400);

-- 查询时调整 ef_search
SET hnsw.ef_search = 40;   -- 快速
SET hnsw.ef_search = 100;  -- 平衡
SET hnsw.ef_search = 200;  -- 高召回
```

## PostgreSQL 配置参数

### 内存相关

```ini
# postgresql.conf

# 共享缓冲区（建议设为系统内存的 25%）
shared_buffers = 256MB

# 工作内存（影响排序和哈希操作）
work_mem = 64MB

# 维护操作内存（影响索引创建）
maintenance_work_mem = 128MB
```

### 并行相关

```ini
# 向量索引创建时的并行度
max_parallel_workers_per_gather = 4

# 最大并行 workers
max_worker_processes = 8
```

### pgvector 专用

```sql
-- 查看当前配置
SHOW ivfflat.probes;
SHOW hnsw.ef_search;

-- 设置会话级参数
SET ivfflat.probes = 50;
SET hnsw.ef_search = 100;

-- 设置数据库级默认值
ALTER DATABASE vector_demo SET hnsw.ef_search = 100;
```

## 实验结果记录模板

```
| 数据量    | 索引类型 | 参数           | 召回率 | 平均延迟 | QPS   |
|-----------|----------|----------------|--------|----------|-------|
| 10,000    | 无索引   | -              | 100%   | 45ms     | 22    |
| 10,000    | IVFFlat  | lists=100      | 95%    | 3ms      | 333   |
| 10,000    | HNSW     | m=16,ef=200    | 98%    | 5ms      | 200   |
| 100,000   | IVFFlat  | lists=500      | 90%    | 8ms      | 125   |
| 100,000   | HNSW     | m=16,ef=200    | 99%    | 6ms      | 166   |
```

## Python 完整示例

```python
#!/usr/bin/env python3
"""pgvector 完整实验脚本"""
import psycopg2
import numpy as np
import time
from typing import List, Set

class PGVectorBenchmark:
    def __init__(self, host="localhost", database="vector_demo"):
        self.conn = psycopg2.connect(host=host, database=database)
        self.cursor = self.conn.cursor()

    def create_table(self, dim=128):
        """创建测试表"""
        self.cursor.execute("DROP TABLE IF EXISTS documents")
        self.cursor.execute(f"""
            CREATE TABLE documents (
                id SERIAL PRIMARY KEY,
                embedding vector({dim})
            )
        """)
        self.conn.commit()

    def insert_data(self, count=10000, dim=128):
        """插入测试数据"""
        vectors = []
        for _ in range(count):
            vec = np.random.rand(dim).tolist()
            vectors.append(f"[{','.join(map(str, vec))}]")

        self.cursor.execute("""
            INSERT INTO documents (embedding)
            SELECT * FROM unnest(%s::vector[])
        """, ([vectors]))
        self.conn.commit()

    def create_index(self, index_type="hnsw", **params):
        """创建索引"""
        params_str = ", ".join(f"{k} = {v}" for k, v in params.items())
        self.cursor.execute(f"""
            CREATE INDEX ON documents
            USING {index_type} (embedding vector_cosine_ops)
            WITH ({params_str})
        """)
        self.conn.commit()

    def search(self, query_vec: str, limit=10) -> List[int]:
        """执行搜索"""
        self.cursor.execute("""
            SELECT id FROM documents
            ORDER BY embedding <=> %s::vector
            LIMIT %s
        """, (query_vec, limit))
        return [row[0] for row in self.cursor.fetchall()]

    def benchmark(self, iterations=100) -> dict:
        """性能基准测试"""
        query = np.random.rand(128)
        query_vec = f"[{','.join(map(str, query.tolist()))}]"

        times = []
        for _ in range(iterations):
            start = time.time()
            self.search(query_vec)
            times.append(time.time() - start)

        return {
            "avg_ms": sum(times) / len(times) * 1000,
            "qps": iterations / sum(times)
        }

if __name__ == "__main__":
    bench = PGVectorBenchmark()

    # 准备数据
    bench.create_table()
    bench.insert_data(10000)

    # 测试无索引
    result = bench.benchmark()
    print(f"No index: {result['avg_ms']:.2f}ms, QPS={result['qps']:.0f}")

    # 测试 HNSW
    bench.create_index("hnsw", m=16, ef_construction=200)
    result = bench.benchmark()
    print(f"HNSW: {result['avg_ms']:.2f}ms, QPS={result['qps']:.0f}")
```

## 要点总结

- pgvector 通过 Docker 或源码编译安装
- IVFFlat 需训练数据分布，HNSW 无需训练
- 召回率：HNSW > IVFFlat > 无索引
- QPS：IVFFlat > HNSW > 无索引（取决于参数）
- pgvector 参数可在会话级动态调整

## 思考题

1. 在 100 万条数据上，IVFFlat 的 `lists` 设置为多少合适？与 HNSW 相比哪个内存占用更小？
2. pgvector 的 `<=>` 运算符返回的是距离还是相似度？如何转换为相似度？
3. 在高并发场景下，pgvector 的 QPS 瓶颈在哪里？如何优化？
