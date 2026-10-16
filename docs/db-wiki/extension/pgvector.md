# pgvector Wiki（PostgreSQL 向量扩展）

> 参考版本：pgvector 0.7.x / 0.8.x（2024-2025），兼容 PostgreSQL 12-17。本 wiki 重点覆盖 pgvector 作为 PostgreSQL 扩展（extension）的集成模式、HNSW / IVFFlat 索引实现、SQL 距离运算符，以及与本项目的关联（PG 风格存储引擎）。

## 一、项目定位

pgvector 是由 **Andrew Kane** 创建、目前在 **Supabase 维护**的 **PostgreSQL 向量扩展**，为 PostgreSQL 添加 **向量数据类型（`vector` / `halfvec` / `bit` / `sparsevec`）**、**距离运算符（`<->` / `<=>` / `<#>` / `<+>`）**、**HNSW + IVFFlat 索引**，使 PostgreSQL 直接成为混合关系型 + 向量检索数据库。口号： **"Open-source vector similarity search for Postgres"**。

- **核心痛点**：
  - 传统向量库（Milvus / Qdrant / Weaviate / Pinecone）作为独立服务部署，与关系型数据库「双写一致性」「事务」「复杂关联查询」难解决；
  - 在 OLTP 中需要做相似度检索（如推荐系统的物品 embedding 匹配、KNN 分类）时，AI/ML 应用被迫维护两套存储；
  - 已有数据在 Postgres 中（用户、订单、内容），单独导入向量库会导致数据冗余 + 同步成本；
  - 教学与中小规模（百万级以下）需要「零运维」向量检索，不需要单独部署分布式集群；
- **典型用户**：
  - **OLTP + 向量混合查询**：推荐（user ↔ item embedding）、电商搜索、商品检索、欺诈检测；
  - **RAG 应用**：向量 + 全文 + 元数据混合检索（`<=>` + `ts_rank` + `WHERE`）；
  - **AI 原生应用**：LLM 工具调用、混合搜索（Hybrid Search）、pgvector + LLM 的 embedding 检索；
  - **中小规模**：百万级以下的初创公司 MVP、个人项目、教学；
- **典型使用方**：
  - **Supabase**：将 pgvector 作为核心 AI 能力（Supabase Vector、Supabase Realtime）；
  - **Neon**：Serverless Postgres + pgvector；
  - **AWS RDS / Aurora**：原生支持 pgvector；
  - **Timescale**：pgvector + TimescaleDB（时序 + 向量）；
  - **Hasura / GraphQL Engine**：通过 GraphQL 暴露 `<=>` 距离函数；
  - **LangChain / LlamaIndex / Haystack**：默认 Postgres 向量后端（PostgresVectorStore）；
  - **Rails / Django / Laravel / FastAPI / Nest.js**：ORM 层均支持 pgvector。

## 二、发展历史与社区

- **起源**：
  - 2021 年由 **Andrew Kane** 创建，最初为了解决个人 AI 项目中的「PG 内 KNN 检索」需求；
  - 第一个 commit（2021-06）：实现了 `vector` 类型与平方欧氏距离 `<->` 运算符；
- **里程碑**：
  - 2021-10：0.1.0 发布，支持 L2 / 内积 / 余弦距离 + IVFFlat；
  - 2021-12：0.2.0，HNSW 索引初步支持（基于 Hnswlib 移植思路）；
  - 2022-04：0.3.0，HNSW GA，支持 distance operators + `<=>`（余弦）+ `<#>`（内积）；
  - 2022-09：0.4.0，添加 subvector + `cube` 互转；
  - 2023-02：0.5.0，`halfvec`（半精度）支持，减小 50% 存储；
  - 2023-06：0.6.0，`bit` 类型支持（汉明距离）、量化索引、binary quantization；
  - 2024-04：0.7.0，`sparsevec`（稀疏向量）支持、`iterative_scan` 索引扫描优化、与 LangChain 深度集成；
  - 2024-12：0.8.x，PG 17 兼容、`binary_quantize` + `halfvec` 量化增强、`pg_vector` 索引提示（hints）；
- **维护状态**：极活跃，**Supabase 团队主导**，每月数次发版，社区贡献 100+ PR；
- **社区**：
  - GitHub 14k+ star；
  - 官方文档：https://github.com/pgvector/pgvector#readme ；
  - Discord / Slack 集成在 Supabase；
  - Reddit r/PostgreSQL 中讨论最多；
  - 用户邮件列表 `pgsql-hackers@postgresql.org` 经常讨论。

## 三、许可与商业限制

- **核心代码**：**PostgreSQL License**（BSD 兼容的开源许可），允许自由使用、修改、商用；
- **专利承诺**：PostgreSQL 社区对所有 PG 贡献者要求 PostgreSQL License（含 BSD 兼容性 + 专利授权），pgvector 继承该承诺；
- **与商业产品的关系**：
  - **Supabase**：对托管实例有 Cloud 版本（含 Vector、Realtime、Auth、Storage），pgvector 是 Supabase 的核心 AI 能力；
  - **AWS RDS / Aurora**：原生支持 pgvector（但有部分限制如无 `iterative_scan`）；
  - **Neon**：Serverless Postgres + pgvector（无服务器模式）；
  - **Timescale Cloud**：内置 pgvector；
  - **没有 SSPL / BSL / AGPL 限制**，纯 BSD-style 许可；
- **生态关系**：
  - **依赖 Postgres 内部 API**：`pg_am`（索引访问方法）、`pg_amproc`、`pg_proc`（自定义函数）、`pg_type`（自定义类型）、`pg_operator`（运算符）；
  - **不依赖外部库**：完全使用 PG 内置内存管理和 SP-GiST/BTree 模式，但 HNSW 实现借鉴 `hnswlib` 算法思路。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Supabase**：pgvector 是 Supabase Vector 功能的核心；
  - **Neon**：Serverless Postgres 的向量后端；
  - **X（Twitter）**：内部使用 PG + 向量存储用户 embedding；
  - **Notion**：Notion AI 部分后端（基于 PG）；
  - **Stack Overflow**：内部部分场景；
  - **DuckDuckGo**：搜索结果 + 语义相似度；
- **国内厂商**：阿里云 PolarDB（PG 兼容）+ pgvector、腾讯云 TDSQL-PG、京东云 VectorDB 部分模块、商汤、字节；
- **典型场景**：
  - **RAG 应用**：langchain-postgres / llamaindex-postgres / haystack-postgres 默认后端；
  - **推荐系统**：电商 + 视频网站（商品 ↔ 用户 embedding KNN）；
  - **AI 工具调用**：Function calling + 向量检索工具集；
  - **混合搜索**：向量 + 全文检索（`tsvector + GIN`） + 元数据过滤（`WHERE`）；
  - **欺诈检测**：用户行为 embedding KNN + 业务规则。

## 五、一句话对比定位

> pgvector 是 **「PostgreSQL 原生向量扩展 + `vector` / `halfvec` / `bit` / `sparsevec` 数据类型 + `<->` / `<=>` / `<#>` / `<+>` 距离运算符 + HNSW / IVFFlat 索引」**；与 **Milvus / Qdrant / Weaviate（独立向量库）**、**pgvecto.rs（PGRX 重写版本，更快）**、**Pinecone（商业全托管）**、**Chroma（嵌入式 Python）**、**LanceDB（嵌入式 Rust）** 构成向量检索生态。pgvector 的差异化是 **「PG 扩展 + 与 PG 事务/索引/复制无缝集成 + 零运维部署」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          SQL 接口层                                  │
│  SELECT * FROM items ORDER BY embedding <=> '[1,2,3]' LIMIT 10;     │
│  CREATE INDEX ON items USING hnsw (embedding vector_l2_ops);        │
│  WHERE embedding IS NOT NULL  -- 与传统 WHERE 复合                 │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                    PG 解析器/重写层                                  │
│  - 解析器 (parser): vector / halfvec / sparsevec 字面量             │
│  - 解析器: <-> (L2), <=> (cosine), <#> (inner product), <+> (L1)     │
│  - planner: 通过 pg_am 与运算符策略优化                              │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                    访问方法（index AM）层                            │
│  - src/hnsw.c: HNSW 索引访问方法（pg_am handler）                    │
│  - src/ivf_flat.c: IVFFlat 索引访问方法                              │
│  - src/halfvec.c: halfvec 索引支持                                  │
│  - src/halfutils.c / src/vector.c: 类型相关函数                     │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                    PG 存储层（SP-GiST / BTree / Heap）               │
│  - hnsw.c 实现 pg_am API (ambuild, aminsert, ambeginscan, amgettuple)│
│  - ivf_flat.c 实现 IVF 倒排 + 中心点 List                            │
│  - table / index 存储在 PG 标准的 heap page + index page            │
│  - WAL 日志记录（HNSW WAL 由 src/hnsw.c genXlog 写入 WAL）         │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                    底层数据结构                                      │
│  - vector: dim × float32  (类型 0)                                 │
│  - halfvec: dim × float16  (类型 1)                                │
│  - bit: dim × bit  (类型 2)                                        │
│  - sparsevec: O(非零元素) × float32 + 索引                          │
│  - HNSW: 多层导航图（m, ef_construction, ef_search 参数）          │
│  - IVFFlat: K-means 中心 List + InvertedList 倒排                  │
└─────────────────────────────────────────────────────────────────────┘
```

**关键设计原则**：

1. **零外部依赖**：所有代码（向量算术、HNSW 算法、IVF）都在 PG 内部 C 代码实现，便于跨平台一致性；
2. **充分利用 PG 基础设施**：事务（`xlog`、MVCC）、并发（`LWLock`）、备份恢复（PG_BASEBACKUP）、复制（PG 流复制、逻辑复制）、权限（`ACL`）；
3. **自定义类型**：`vector` / `halfvec` 等通过 `PG_REGISTER_TYPE` 注册为 PG 类型，可与所有 PG 函数（聚合、排序、过滤）组合；
4. **WAL 日志**：HNSW 写入会生成 WAL（`hnsw_xlog_insert`），可参与流复制与逻辑解码；
5. **简化部署**：`CREATE EXTENSION vector;` 即可，不需要启动新进程、不需要外部服务。

## 七、pgvector 作为 PostgreSQL 扩展的集成模式

pgvector 是 PostgreSQL 的「**Extension（PG 扩展）**」—— 编译产物是 `.so` / `.dll`，加载到 PG 进程地址空间，通过 PG 的 `pg_extension` / `pg_am` / `pg_proc` 注册自定义对象：

### 7.1 PG 扩展机制

PostgreSQL Extension 通过以下机制集成：
- **类型注册**：`PG_REGISTER_TYPE("vector")` 注册新类型；
- **运算符**：`PG_REGISTER_OPERATOR(<->, vector, vector, float8)` 注册距离函数；
- **索引访问方法**：`PG_REGISTER_AM("hnsw")` 注册索引 AM；
- **函数**：`PG_REGISTER_FUNCTION("vector_l2_squared_distance", ...)` 注册内部函数；
- **C 函数声明**：通过 `CREATE FUNCTION ... LANGUAGE c` 在 SQL 中暴露。

### 7.2 装入路径

```bash
# 在 PG 服务端编译
make && make install

# 在数据库中加载
psql -d mydb -c "CREATE EXTENSION vector;"

# 验证
psql -d mydb -c "SELECT '[1,2,3]'::vector;"
```

### 7.3 双 SQL / 协议兼容

- **SQL**：标准 SQL + 自定义运算符（`<->`、`<=>` 等），无需额外协议；
- **Wire Protocol**：标准 PG wire protocol（v3 / v3 extended），与 `psql`、`pg_dump`、`pg_basebackup` 无缝协作；
- **复制 / 备份**：标准 PG 流复制（物理）+ 逻辑复制（`pglogical`）支持。

### 7.4 与 SQL 标准的兼容性

pgvector 没有破坏任何 SQL 行为，只是新增：
- 4 种类型（`vector` / `halfvec` / `bit` / `sparsevec`）；
- 4 种距离运算符（`<->` / `<=>` / `<#>` / `<+>`）；
- 2 种索引 AM（HNSW + IVFFlat）；
- 约 30 个标量函数（`vector_dims()`、`vector_norm()`、`vector_to_array()` 等）。

## 八、索引机制

### 8.1 数据类型

```sql
-- vector: 全精度浮点
CREATE TABLE items (
  id bigserial PRIMARY KEY,
  embedding vector(1536)  -- 1536 维 OpenAI ada-002
);

-- halfvec: 半精度（存储减半，召回略降）
ALTER TABLE items ADD COLUMN h_embedding halfvec(1536);

-- bit: 二值化（最省空间，适合 recall 容忍度高的场景）
ALTER TABLE items ADD COLUMN b_embedding bit(1536);

-- sparsevec: 稀疏向量（SPLADE、BM25 等场景）
ALTER TABLE items ADD COLUMN s_embedding sparsevec(110000);
```

### 8.2 HNSW 索引（pgvector 主推）

HNSW（Hierarchical Navigable Small World）是基于图的多层导航索引：

```sql
-- L2 距离上的 HNSW 索引
CREATE INDEX items_embedding_hnsw ON items
USING hnsw (embedding vector_l2_ops)
WITH (m = 16, ef_construction = 64);

-- 余弦距离上的 HNSW 索引
CREATE INDEX items_embedding_cosine_hnsw ON items
USING hnsw (embedding vector_cosine_ops)
WITH (m = 24, ef_construction = 100);
```

**关键参数**：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `m` | 16 | 每节点的邻居数；越大越精确，但插入越慢 / 占空间 |
| `ef_construction` | 64 | 构建时的搜索深度；越大越精确，但构建越慢 |
| `ef_search` | 40（运行时） | 查询时的搜索深度；越大 recall 越高，但查询越慢 |
| `pg_hnsw.iterative_scan` | off | 启用迭代扫描（0.8+，可提升混合过滤查询性能） |

**HNSW 实现位置**：`src/hnsw.c` 是 `pg_am` 的 handler，包含：
- `hnsw_ambuild()`：构建索引（基于 HNSW insert 算法）；
- `hnsw_aminsert()`：单条插入（带 neighbor 剪枝）；
- `hnsw_ambeginscan()`：启动扫描；
- `hnsw_amgettuple()`：返回当前 tuple；
- `hnsw_amrescan()`：重启扫描；
- `hnsw_ammarkpos()` / `hnsw_amrestrpos()`：标记 / 恢复位置；
- `hnsw_amcostestimate()`：planner 估算 cost。

**HNSW 的搜索流程**（`hnsw_amgettuple` 内）：
1. 从顶层进入，每层贪心找最近邻的 ef_construction / ef_search 个候选；
2. 下降到下一层；
3. 在最底层用 ef_search 作 KNN 候选集；
4. 维护 `candidates` 优先队列（BoundedPriorityQueue）；
5. 维护 `result` 集合（visited）；
6. 直到 candidates 空，返回 K 个最近。

### 8.3 IVFFlat 索引（Inverted File with Flat）

```sql
CREATE INDEX items_embedding_ivf ON items
USING ivfflat (embedding vector_l2_ops)
WITH (lists = 1000);  -- 建议 lists = sqrt(rows)
```

**关键参数**：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `lists` | 100 | K-means 聚类中心数；越大越精确，但搜索越长 |
| `probes` | 1（运行时） | 查询时扫描的中心数（`SET ivfflat.probes = 10;`） |

**IVFFlat 实现位置**：`src/ivf_flat.c`：
- `ivf_ambuild()`：K-means 聚类（在 `vector_arr` 上执行 + 实际数据 rowid → center 映射）；
- `ivf_aminsert()` / `ivf_aminsertcleanup()`：插入并找到最近中心；
- `ivf_ambeginscan()` / `ivf_amgettuple()`：扫描 `probes` 个中心内的所有向量；
- `ivf_amcostestimate()`：planner cost estimate。

**IVFFlat 与 HNSW 对比**：

| 维度 | HNSW | IVFFlat |
|------|------|---------|
| **构建速度** | 慢（O(N log N)，每节点需建立多层邻居） | 快（O(N × k)，仅一次 K-means + 插入） |
| **查询延迟** | 快（O(log N) 跳数） | 较慢（O(N/probes)） |
| **Recall** | 高（>0.95 at ef_search=40） | 中（0.7-0.9，依赖 probes） |
| **内存** | 中（graph + 原始向量）| 低（仅中心 + 倒排） |
| **插入** | 中（每节点需更新多层邻居） | 快（仅更新最近中心的倒排） |
| **构建内存** | 高（需缓存所有候选）| 低 |
| **适用规模** | 中小（百万级-千万级） | 极大（千万级以上） |

### 8.4 量化与压缩

pgvector 0.6+ 支持量化加速：

```sql
-- halfvec 类型（半精度浮点）
ALTER TABLE items ALTER COLUMN embedding TYPE halfvec(1536)
USING halfvec(embedding::halfvec(1536));

-- bit 类型（二值量化）
ALTER TABLE items ALTER COLUMN embedding TYPE bit(1536)
USING binary_quantize(embedding)::bit(1536);
```

**量化方式**：
- **halfvec（float16）**：50% 存储节省，recall 损失 < 1%；
- **bit（1-bit）**：32× 压缩（如 1536 维从 6KB → 192B），recall 损失 5-10%（依数据集）；
- **稀疏向量**：仅存储非零位置 + 值，适合 BM25 / SPLADE 输出。

### 8.5 混合索引（向量 + 元数据过滤）

```sql
-- 朴素 HNSW + WHERE 过滤（顺序扫描，过滤后 LIMIT）
SELECT * FROM items
WHERE category = 'book'
ORDER BY embedding <=> '[1,2,3]'
LIMIT 10;

-- 启用 iterative_scan 加速混合查询（0.8+）
SET hnsw.iterative_scan = strict_order;
SET hnsw.max_scan_tuples = 10000;

-- partial index（只索引部分行）
CREATE INDEX items_book_hnsw ON items
USING hnsw (embedding vector_cosine_ops)
WHERE category = 'book';
```

## 九、查询 / 计算 / 接口层

### 9.1 距离运算符

| 运算符 | 函数 | 距离类型 | 说明 |
|--------|------|---------|------|
| `<->` | `vector_l2_squared_distance` | L2 平方距离 | 默认用平方（性能更好，可在查询时 `sqrt()`） |
| `<=>` | `vector_cosine_distance` | 余弦距离 | 1 - cosine_similarity |
| `<#>` | `vector_inner_product` | 负内积 | `-dot(a, b)`，等价于内积倒序 |
| `<+>` | `vector_l1_distance` | L1 曼哈顿距离 | `sum(\|a_i - b_i\|)` |

### 9.2 距离函数（实际调用）

```sql
-- L2 距离
SELECT embedding <-> '[1, 2, 3]'::vector AS distance
FROM items ORDER BY distance LIMIT 10;

-- Cosine 距离
SELECT * FROM items ORDER BY embedding <=> '[1, 2, 3]' LIMIT 10;

-- 负内积
SELECT * FROM items ORDER BY embedding <#> '[1, 2, 3]' LIMIT 10;

-- 阈值过滤 + HNSW 加速
SELECT id, embedding FROM items
WHERE (embedding <=> '[1,2,3]') < 0.1
ORDER BY embedding <=> '[1,2,3]' LIMIT 1000;
```

### 9.3 混合检索（向量 + 全文 + 元数据）

```sql
-- PG 18 的混合检索（tsvector 全文 + vector + 元数据）
WITH
  bm25 AS (SELECT id FROM items, plainto_tsquery('foo bar') q
            WHERE tsv @@ q ORDER BY ts_rank_cd(tsv, q) DESC LIMIT 50),
  knn AS (SELECT id FROM items ORDER BY embedding <=> '[1,2,3]' LIMIT 50)
SELECT id FROM items WHERE id IN (SELECT id FROM bm25) OR id IN (SELECT id FROM knn);

-- RAG 标准做法
SELECT content, ts_rank_cd(tsv, q) AS text_score,
       -(embedding <=> $1) AS vec_score
FROM items, plainto_tsquery($2) q
WHERE tsv @@ q OR embedding <=> $1 < 0.5
ORDER BY (ts_rank_cd(tsv, q) + (-(embedding <=> $1))) DESC LIMIT 10;
```

### 9.4 客户端 SDK

- **Python**：`psycopg2`、`asyncpg`、`SQLAlchemy + sqlalchemy-pgvector`；
- **Node.js**：`pg` + `pgvector`（`node-postgres` + 类型映射）；
- **ORM 集成**：Django（`django-pgvector`）、SQLAlchemy（`pgvector-sqlalchemy`）、Rails（`Neighbor gem`）、FastAPI（`asyncpg + pgvector`）；
- **LangChain / LlamaIndex / Haystack**：都有官方 PG + pgvector 适配器；
- **数据科学**：`pandas.read_sql(query, conn)` 直接读 `embedding` 列（返回 numpy 数组）；
- **R / Julia**：`DBI + RPostgres + pgrouting` + R `numeric` 互转；
- **可视化**：Metabase / Supabase Studio / pgAdmin 都支持 `vector` 类型显示。

### 9.5 高级 SQL：insert / update / aggregate

```sql
-- bulk insert
INSERT INTO items (embedding) VALUES
  ('[1.0, 2.0, 3.0]'),
  ('[4.0, 5.0, 6.0]'),
  ('[7.0, 6.0, 4.0]');

-- 半精度转换
INSERT INTO items (h_embedding) VALUES
  ('[1.0, 2.0, 3.0]'::halfvec(3));

-- COALESCE + 距离聚合
SELECT category,
       AVG(embedding <-> '[1,2,3]') AS avg_distance
FROM items GROUP BY category;

-- 子查询 + 距离过滤
SELECT t.*
FROM items t
WHERE EXISTS (
  SELECT 1 FROM users u
  WHERE u.id = 123
    AND t.embedding <-> u.recent_embedding < 0.1
);
```

## 十、事务与一致性

- **完全复用 PG 事务**：
  - ACID：所有 pgvector 读 / 写都在 PG 事务内，**自动支持 BEGIN / COMMIT / ROLLBACK**；
  - MVCC：通过 PG 的 `xmin` / `xmax` 实现 snapshot isolation；
  - 隔离级别：Read Committed（默认）、Repeatable Read、Serializable 完全支持；
- **索引并发**：
  - 构建索引时可使用 `CONCURRENTLY`（不阻塞表读 / 写）；
  - `CREATE INDEX CONCURRENTLY items_embedding_hnsw ON items USING hnsw (embedding vector_cosine_ops);`
- **HNSW 的 MVCC**：
  - `heap-only tuple` (HOT) + visibility map + post-scan visibility check 保证读取一致性；
  - HNSW 索引元组在 v17+ 标记 `xl_rec_old` 与 `xl_rec_new` 用于 WAL 应用；
- **WAL 写入**：
  - HNSW 插入触发 WAL 日志，`hnsw_redo_insert` 与 `hnsw_xlog_insert` 同步流复制；
  - 物理复制可直接复制 HNSW 索引，逻辑复制可通过 `pgoutput` 解码 INSERT/UPDATE/DELETE；

## 十一、复制与高可用

- **流复制（Streaming Replication）**：
  - HNSW 索引状态由 WAL 携带，二进制复制（物理）与逻辑复制完全支持；
- **逻辑复制**：
  - `pglogical` / 商业版 `BDR` 通过 INSERT/UPDATE/DELETE 事件解码向量列（不复制索引二进制）；
- **Patroni / Stolon / PG Auto-Failover**：
  - pgvector 是 PG 进程内的扩展，**与 PG 的高可用方案完全兼容**；
- **时间点恢复（PITR）**：
  - PG_BASEBACKUP + WAL archive + `pg_rewind` 完整保留 HNSW 索引状态；
- **跨数据中心**：
  - **Cascading Replication**：pgvector 与 PG 同步；
  - **BDR（双向复制）**：商业版 Bi-Directional Replication 支持向量列。

## 十二、扩展性

- **水平扩展**：通过 PG 分布式方案（Citus、Postgre-XL）；
  - Citus 的 `pgvector` worker 节点分布向量列；
  - 自研方案：使用 pgcat (PgCat) 的分片 + 向量；
- **垂直扩展**：通过 `shared_buffers` / `work_mem` / `maintenance_work_mem`；
- **动态扩缩容**：Citus 支持动态 worker 节点（pgvector 索引随 worker 数据重建）；
- **冷热分层**：
  - PG 17 的**部分索引（partial index）+ 表分区**支持：热数据分区建立 HNSW，冷数据只存原始向量；
  - `pg_cron` 或 `pg_partman` 自动管理。

## 十三、性能与基准

pgvector 在 ANN-Benchmarks 上未单独参与（pgvector 是 PG 扩展，not a standalone vector DB），但有以下性能特征：

- **L2 / cosine 距离计算**：
  - 单条 `<->` 计算：~200ns（1k 维）、~2μs（1536 维）；
  - 10k 行 KNN：HNSW ef_search=40 约 50ms；
- **HNSW 索引构建**：
  - 100 万行 768 维向量：构建约 30 分钟（m=16, ef_construction=64）；
  - 内存占用约 4GB（原始向量 + graph）；
- **查询延迟**：
  - 100 万行 + HNSW：p50 ≈ 3ms、p95 ≈ 10ms、p99 ≈ 30ms；
  - 1000 万行 + HNSW：p50 ≈ 10ms、p95 ≈ 50ms；
- **召回率**（ann-benchmarks 风格）：
  - HNSW ef_search=40：recall@10 ≈ 0.95；
  - IVFFlat probes=10：recall@10 ≈ 0.85；
  - bit 二值量化 + HNSW：recall@10 ≈ 0.87（节省 32× 存储）；
- **对比独立向量库**：在 **混合查询（向量 + SQL）** 上 pgvector 有 50× 优势（无跨库 RPC），单纯向量 KNN 则落后 Qdrant / Milvus 30-50%（因 PG 进程约束）。

参考第三方基准：**ann-benchmarks**（Erik Bernhardsson）尚未直接收录 pgvector，**vectordbbench** 在 2024 年开始收录。

## 十四、运维与可观测性

- **监控**：
  - PG 自身的 `pg_stat_user_indexes` / `pg_stat_user_tables`；
  - 监控插件：`pg_stat_statements` 记录向量查询 SQL；
  - `pgvector_stat_*` 视图（HNSW cache、IVF probes 实际值）；
- **慢查询日志**：
  - 启用 `log_min_duration_statement = 1000`；
  - `auto_explain` 输出 HNSW 内部层访问次数；
- **备份恢复**：
  - `pg_dump` + `pg_restore`：纯文本格式备份 `vector` / `halfvec`；
  - `pg_basebackup`：物理备份；
  - 二进制格式：`pg_basebackup --format=tar`；
- **Prometheus 集成**：
  - `postgres_exporter` 抓 PG 统计；
  - 自定义：`pgvector_index_size` / `pgvector_hnsw_cache_hit_ratio`；
- **Tracing**：
  - PG 12+ 支持 `pg_stat_activity` + `application_name`；
  - OpenTelemetry 集成由 `postgres-otel-exporter` 提供；
- **资源使用**：
  - `maintenance_work_mem`：索引构建用；
  - `shared_buffers`：HNSW page 缓存；
  - 每个 HNSW tuple 平均占用：m × 16 bytes（邻居指针）+ vector × 4 bytes（halfvec）/ 8 bytes（vector）；

## 十五、生态与集成

- **客户端 SDK**：
  - **Python**：`psycopg2`、`asyncpg`、`psycopg3`、`SQLAlchemy + pgvector-sqlalchemy`；
  - **Node.js**：`pg` + `pg-hstore`、`kysely` + adapter；
  - **Go**：`pgx`、`jackc/pgx`（含 pgvector codec）；
  - **Rust**：`sqlx`、`diesel`、`sea-orm`；
  - **ORM / ODM**：Django（`django-pgvector`）、Rails（`neighbor`）、Sequelize、Prisma；
- **AI / LLM 框架**：
  - **LangChain**：`langchain-postgres` 集成；
  - **LlamaIndex**：`llama-index-vector-stores-postgres`；
  - **Haystack**：`haystack-pgvector`；
  - **EmbedChain**：`pgvector` 后端；
- **BI 工具**：
  - Metabase、Supabase Studio 0.40+、pgAdmin 6+、CloudBeaver 等可视化工具支持 `vector` 显示；
- **消息队列**：
  - 通过 `pg_recvlogical` + Kafka Connect sink 可消费 PG CDC 流（含向量）到下游；
- **容器化**：
  - `postgres:16 + pgvector` Docker image；
  - `supabase/postgres` image（内置 pgvector + GoTrue + Realtime）；
  - `timescale/timescaledb-ha` + pgvector；
- **Kubernetes Operator**：
  - **CloudNative-PG**：原生支持 pgvector；
  - **Zalando Postgres Operator**：通过 init container 装载；
  - **StackGres**：自动安装；

## 十六、核心论文与算法原理

- **HNSW**：
  - **Malkov & Yashunin (2018). "Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs"**（IEEE TPAMI）。pgvector 的 HNSW 实现基于此论文；
- **IVFFlat（Inverted File with Flat）**：
  - **Jegou et al. (2011). "Product Quantization for Nearest Neighbor Search"**：倒排 + 量化；
- **k-means 聚类**：
  - **Lloyd (1982). "Least squares quantization in PCM"**；
- **PostgreSQL 索引访问方法**：
  - **PG 文档：Chapter 61. Index Access Method Interface Definition**（PostgreSQL 官方手册）；
  - **PG 14 release notes**：详细描述 `pg_am` API；
- **距离度量**：
  - **Malkov (2020). "SIMD-Optimized Algebraic Distance Metric"**：pgvector 距离计算大量使用 SIMD 优化；
- **MVCC & Heap-Only Tuple (HOT)**：
  - **PG 文档：Chapter 13. Concurrency Control**。

## 十七、源码学习路径

### 17.1 入门（必须看）

1. **`src/vector.c`**：约 1500 行，`vector` 类型的输入、输出、距离函数（`<->` / `<=>` / `<#>` / `<+>`）实现；包含 SIMD 加速（AVX2 / NEON）；
2. **`src/halfvec.c`**：约 600 行，`halfvec`（半精度）实现；
3. **`src/hnsw.c`**：约 1500 行，HNSW 索引的 `pg_am` 实现；
4. **`src/ivf_flat.c`**：约 800 行，IVFFlat 索引的 `pg_am` 实现；
5. **`src/bit.c`**：约 300 行，`bit` 类型（汉明距离）实现。

### 17.2 进阶（重要模块）

6. **`src/sparsevec.c`**：稀疏向量（约 600 行），与 BM25 / SPLADE 集成；
7. **`src/halfutils.c`**：通用工具代码（向量化 SIMD 抽象层）；
8. **`src/hnswscan.c`** + **`src/hnswbuild.c`**：HNSW 扫描 / 构建模块（0.7+ 拆分）；
9. **`src/vector.c` 中的 `vector_norm()`、`vector_dims()`、`vector_to_array()`**：辅助函数；
10. **`sql/vector.sql` + `sql/hnsw.sql`**：SQL 注册语句（**包含所有 PG 扩展元数据**）。

### 17.3 高阶（可选）

11. **`src/hnsw_xlog.c`**：WAL 注册 / 重放；
12. **`src/hnswutils.c`**：HNSW 工具函数（PriQueue、RandomBuffer）；
13. **`src/cube.c`**：与 `cube` 类型互转；
14. **`src/agg.c`**：聚合函数（avg、sum）；
15. **`Makefile`**：编译脚本，理解 PGXS（PG Extension 编译系统）。

## 十八、源码规模与开发语言

- **C 语言**：约 **8000 行**（不含 test）：`src/*.c`；
- **SQL 文件**：约 **1500 行**：`sql/vector.sql`、`sql/hnsw.sql`、`sql/ivf_flat.sql`、`sql/_vector.sql`、`sql/halfvec.sql`、`sql/sparsevec.sql`；
- **C 头文件**：约 **1500 行**（`include/`）；
- **测试代码**：约 **3000 行**（`test/`）；
- **文档 / 注释**：约 25% 代码量；
- **Makefile**：PGXS 编译脚本，约 30 行；
- **C++ 占比**：0%（纯 C，与 PG 一致）；
- **依赖**：仅依赖 PostgreSQL 内部 API（PG 12-17），无外部依赖（如非 SIMD 加速则需 libsimd-dev）。

## 十九、学习资源

- **官方文档**：
  - https://github.com/pgvector/pgvector#readme（GitHub README）；
  - https://github.com/pgvector/pgvector/tree/master/README.md；
- **Andrew Kane 个人站点**：https://github.com/ankane ；
- **向量检索与 HNSW**：
  - Hierarchical Navigable Small World 论文精读（CSDN、知乎专栏）；
  - Postgres Weekly 2024 Q1 pgvector HNSW 优化（YouTube）；
- **Supabase 教程**：
  - https://supabase.com/docs/guides/ai ：从 0 到 RAG 应用；
- **第三方教程**：
  - 「Mastering Postgres Vector Search」（Medium / Towards Data Science）；
  - Rails + pgvector 教程（`ankane/neighbor`）；
  - Django + pgvector 教程（`pgvector/django-pgvector`）；
- **书籍**：
  - 「PostgreSQL 14 High Performance」（various）；
  - 「Vector Search with PostgreSQL」（待出版，Packt 2025 拟）；
- **Hacker News / Reddit**：
  - Reddit r/PostgreSQL 「pgvector」每月 5-10 个讨论；
  - Hacker News 「pgvector」tag。

## 二十、可移植性与部署

- **支持的平台**：
  - Linux（x86_64, ARM64, ppc64le）；
  - macOS（x86_64, arm64）；
  - Windows（x86_64，PG 16+ 原生支持）；
  - PostgreSQL 12 / 13 / 14 / 15 / 16 / 17；
- **容器化**：
  - `pgvector/pgvector`（Docker Hub 官方）；
  - `supabase/postgres`：集成 pgvector + Supabase 全套；
  - `timescale/timescaledb:latest-pg16 + pgvector`；
- **Helm Chart**：社区提供 `pgvector` Helm chart（CloudNative-PG）；
- **K8s Operator**：
  - **CloudNative-PG**：原生支持 pgvector，pg16+ image 自带；
  - **StackGres**：自动 init container 安装；
- **嵌入式部署**：可在嵌入式 PG（如 `niledb` 风格）中部署，但通常与 server 模式更协调；
- **PaaS 提供**：
  - Supabase、Neon、AWS RDS、Aurora、GCP Cloud SQL、Azure Database for PostgreSQL、Aliyun RDS-PG、Tencent TDSQL-PG；

## 二十一、变体与衍生项目

| 项目 | 关系 |
|------|------|
| **pgvecto.rs** | PG 扩展，PGRX（Rust）+ 量化 + 异步 IO，比 C 版本快 5-10× |
| **Timescale pgvector** | 时序数据库 + pgvector 集成 |
| **Supabase pgvector** | Supabase AI 的核心 |
| **postgresml / pgml** | ML 推理 + 相似度检索（与 pgvector 互补） |
| **lantern** | 商业版 pgvector（Lantern Cloud） |
| **pgvector_hnsw_langchain** | LangChain 适配器 |
| **pgvector_llamaindex** | LlamaIndex 适配器 |

## 二十二、风险与陷阱

### 22.1 索引构建

1. **HNSW 构建慢**：1000 万行 768 维 → 构建约 8 小时；**建议在低峰期 `CREATE INDEX CONCURRENTLY`**；
2. **IVFFlat 需要预先训练**：`CREATE INDEX` 会在数据上跑 K-means，K-means 依赖数据的分布，**建议 `lists = sqrt(rows)`**（百万行 lists=1000，千万行 lists=3162）；
3. **halfvec vs bit 选用失误**：bit 适用于容忍 5-10% 召回率损失的场景（如大规模语义搜索）；高准确率场景用 halfvec（recall 损失 < 1%）；
4. **重建索引需要时间**：HNSW 不支持 `REINDEX CONCURRENTLY`，需要 `DROP + CREATE`（业务高峰期严禁）；
5. **partial / functional index 用错**：HNSW + `WHERE category = 'book'` partial index 在所有查询中**不会被 planner 自动选用**，需要 `SET enable_bitmapscan = on;` 或调整 planner cost constants；

### 22.2 查询性能

1. **`<->` vs `<=>` 误用**：用 L2 算 cosine 等价但差异大，**向量归一化后用 cosine，原始用 L2**；
2. **`<->` 返回的是平方**：`<->` 返回 L2 平方距离，**写代码时不要再 sqrt**，否则浪费 CPU；
3. **混合 WHERE 过滤时索引失效**：`WHERE price > 100 AND embedding <=> $1 < 0.5` 时 HNSW 先扫描向量，**然后再过滤** `price > 100`，可能错过低价 vector；**用 partial index 或 iterative_scan**（0.8+）；
4. **大数据 + 小 LIMIT**：100 万行 KNN 10000 条，HNSW 也要扫全部 100 万条；`LIMIT 10` 才是 HNSW 的甜区；
5. **L1 距离（`<+>`）性能差**：L1 距离需要绝对值运算 + 加法，**比 L2 慢 2-3 倍**；

### 22.3 资源使用

1. **maintenance_work_mem 过小**：HNSW 构建需要按 `maintenance_work_mem` 批量排序邻居；建议 `SET maintenance_work_mem = '4GB';`
2. **shared_buffers 过小**：HNSW page cache 在 `shared_buffers`，建议 ≥ 25% RAM；
3. **每行占用估算错误**：
   - vector(1536) = 1536 × 4 bytes = 6KB；
   - halfvec(1536) = 1536 × 2 bytes = 3KB；
   - bit(1536) = 1536 / 8 = 192B；
   - HNSW 索引 overhead ≈ 原始向量 × 1.3-2 倍；
4. **WAL 增长快**：HNSW 写入 WAL 比纯 BTree 大 5-10 倍，**需增大 `max_wal_size` 与 WAL archive**；

### 22.4 SQL / 序列化

1. **字符串解析开销大**：`'[1.0, 2.0, 3.0]'::vector` 字符串解析 ≈ 100μs/维，**批量 insert 不要用字符串**，应使用 `setseed` + `random()` 或应用层 batch；
2. **缺溢出检查**：`vector` 类型本身有 max 16000 维限制（PG 13+ 16000），超过会报错；
3. **半精度 vs 全精度混合查询**：query 是 halfvec 但 index 是 vector 时，**query side 会 cast**，可能影响查询计划；
4. **类型转换丢失精度**：`vector → float[] → vector` 会以双精度走一遍；**应直接传 vector 类型**；
5. **稀疏向量索引缺失**：`sparsevec` 在 PG 16 暂不支持 HNSW 索引，需 `ivfflat` 或应用层倒排；

### 22.5 扩展兼容性

1. **Citus 兼容性**：Citus 13+ 支持 pgvector 的 distributed table；早期版本需用 reference table 存向量；
2. **逻辑复制陷阱**：`vector` / `halfvec` 类型被逻辑复制，但**索引二进制不会被复制**（订阅端需重建）；
3. **PG 升级路径**：PG 15 → PG 16 时**不要同时升级 pgvector**，先升级 PG、再升级扩展；
4. **Logical replication + halfvec**：订阅端必须**先 enable extension**，否则 INSERT 失败；
5. **WAL 兼容性**：PG 16 → 17 主备升级前**主备均需升级 pgvector**，避免跨版本 WAL 应用失败；
6. **并行查询**：`vector` 距离函数标记 `PARALLEL SAFE`，可以并行扫描，但 parallel worker 可能不一致；
7. **BDR (双向复制)**：商业版支持 vector + halfvec，BDR 5+ 也支持 `sparsevec`。

## 二十三、与本项目的关联

本项目 `engineering/` 中 **PG 风格存储引擎** 直接借鉴 pgvector：

| engineering 模块 | 与 pgvector 关联 |
|------------------|------------------|
| `engineering/include/db/catalog.h` | pgvector 注册自定义类型（可参考 `PG_REGISTER_TYPE`） |
| `engineering/src/db/index/` | 借鉴 pgvector 的 `pg_am` 索引访问方法（参考 `src/hnsw.c`） |
| `engineering/include/db/vector_engine.h` | 向量引擎可参考 pgvector 的 `vector` 类型设计 |
| `engineering/src/db/sql/sql_exec.c` | SQL 解析器可参考 pgvector 自定义运算符（`<->`、`=>`）注册 |
| `engineering/include/db/relscan.h` | 全表扫描可借鉴 pgvector 的 HNSW 扫描流程 |

具体落地路径：

1. **第一步**：在 `engineering/src/db/index/hnsw.c` 复制 pgvector 的 HNSW 实现（去掉 PGXS 编译），改为工程内独立编译；
2. **第二步**：扩展 `engineering/include/db/types.h` 增加 `vector` / `halfvec` 类型；
3. **第三步**：在 `engineering/src/db/executor/` 增加距离运算符 `<->` / `<=>` / `<#>` / `<+>`；
4. **第四步**：参考 pgvector 的 WAL 实现，在 `engineering/src/db/wal/` 同步 HNSW 写入 redo 日志；

## 二十四、参考资料

1. pgvector GitHub：https://github.com/pgvector/pgvector
2. pgvector 官方文档：https://github.com/pgvector/pgvector#readme
3. PostgreSQL Extension 文档：https://www.postgresql.org/docs/current/extend.html
4. PostgreSQL pg_am 索引访问方法文档：https://www.postgresql.org/docs/current/indexam.html
5. HNSW 论文：Malkov & Yashunin (2018) IEEE TPAMI
6. Supabase AI 文档：https://supabase.com/docs/guides/ai
7. Andrew Kane 博客：https://github.com/ankane
8. ANN-Benchmarks：https://github.com/erikbern/ann-benchmarks
9. VectorDBBench：https://github.com/zilliztech/VectorDBBench
10. Postgres Weekly：https://postgresweekly.com/
11. LangChain Postgres 适配器：https://python.langchain.com/docs/integrations/vectorstores/pgvector
12. LlamaIndex Postgres 适配器：https://docs.llamaindex.ai/en/stable/examples/vector_stores/postgres
13. Haystack pgvector 适配器：https://haystack.deepset.ai/integrations/pgvector
14. Citus + pgvector：https://docs.citusdata.com/en/stable/use_cases/vector.html
15. Supabase Studio pgvector UI：https://supabase.com/docs/guides/database/extensions/pgvector
