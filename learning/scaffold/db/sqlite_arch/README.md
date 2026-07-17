# sqlite_arch - SQLite 向量扩展（SQLite VSS）

## 概述

本模块演示 SQLite VSS（Vector Search）扩展的核心概念，用于在 SQLite 数据库中实现向量存储和近似最近邻（ANN）搜索。

## SQLite VSS 简介

SQLite VSS 是一个 SQLite 扩展，将向量数据库功能引入轻量级 SQLite：

- **虚拟表机制**：通过 SQLite 虚拟表接口实现向量存储
- **ANN 索引支持**：支持 HNSW、IVF 等向量索引
- **距离函数**：内置欧氏距离、余弦距离等度量
- **SQL 接口**：通过 SQL 语句操作向量数据

## 编译运行

```bash
make && ./test.exe
```

或直接编译：

```bash
gcc -std=c11 -Wall -Wextra -lm main.c -o test.exe
./test.exe
```

## 核心概念

### 1. 扩展函数注册

SQLite VSS 作为扩展加载后注册以下函数：

```sql
-- 加载扩展
SELECT load_extension('libsqlite_vss.so');

-- ANN 搜索
SELECT * FROM vss_embedding
WHERE vss_search(vector_column, '[0.1, 0.2, 0.3, 0.4]')
ORDER BY distance LIMIT 5;
```

### 2. 向量存储结构

```
vss_embedding 表结构:
  - id: INTEGER PRIMARY KEY      -- 向量唯一 ID
  - rowid: INTEGER               -- 关联原始表行
  - dimensions: INTEGER          -- 向量维度
  - file: BLOB                   -- 存储向量数据

vss_embedding_content 表:
  - embedding_id: INTEGER        -- 向量 ID
  - content: BLOB                 -- 序列化向量数据
```

### 3. 距离函数

SQLite VSS 提供多种距离度量：

| 函数 | 说明 | SQL 用法 |
|------|------|---------|
| `vss_euclidean_distance(a, b)` | 欧氏距离 | `ORDER BY vss_euclidean_distance(v, query)` |
| `vss_cosine_distance(a, b)` | 余弦距离 | `ORDER BY vss_cosine_distance(v, query)` |
| `vss_inner_product(a, b)` | 内积 | `ORDER BY vss_inner_product(v, query)` |

### 4. ANN 索引

支持两种索引类型：

```sql
-- HNSW 索引（高质量、高内存）
CREATE VIRTUAL TABLE vss_articles USING vss_articles(
  article_embedding(384) params('hnsw', 'dim=384,m=16,ef_construction=64')
);

-- IVF 索引（中等质量、较低内存）
CREATE VIRTUAL TABLE vss_articles USING vss_articles(
  article_embedding(384) params('ivf', 'dim=384,nlist=100')
);
```

## 与 pgvector 对比

| 特性 | SQLite VSS | pgvector |
|------|------------|----------|
| 数据库 | SQLite | PostgreSQL |
| 向量存储 | 虚拟表 + BLOB | 原生向量类型 |
| 索引类型 | HNSW, IVF | ivfflat, hnsw |
| 部署复杂度 | 低（单文件） | 中（需要 PG） |
| 适用场景 | 边缘计算、嵌入式 | 生产服务器 |

## 应用场景

- **边缘 AI**：在 IoT 设备上运行轻量向量搜索
- **本地开发**：无需部署完整数据库即可开发向量应用
- **原型验证**：快速验证向量数据库概念
- **移动应用**：端侧向量相似度匹配

## 参考

- SQLite VSS 项目：https://github.com/asg017/sqlite-vss
- SQLite 虚拟表文档：https://www.sqlite.org/vtab.html
- 工程向量存储：`engineering/src/db/storage/vector/vector_engine.c`
- 工程向量索引：`engineering/src/db/index/vector_index/hnsw/`, `ivf/` 等
