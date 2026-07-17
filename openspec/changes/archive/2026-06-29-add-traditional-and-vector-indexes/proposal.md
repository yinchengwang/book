## Why

当前 index 模块已有 B-Tree/B+Tree、Hash、HNSW、DiskANN、IVF 等索引实现，但传统索引缺少 GIN/GiST/FULLTEXT/BRIN/Bitmap，向量索引缺少 IVF-PQ/LSH/OPQ/多模态索引。这些是数据库领域的经典索引类型，掌握它们对于理解数据库内核和向量检索技术至关重要。

## What Changes

新增 9 个索引模块，涵盖传统索引和向量索引两大类：

### 传统索引
- **GIN (Generalized Inverted Index)**：通用倒排索引，支持 JSONB、数组、全文搜索
- **GiST (Generalized Search Tree)**：广义搜索树，支持自定义操作符
- **FULLTEXT (Full-Text Index)**：全文索引，增强中文分词
- **BRIN (Block Range Index)**：块范围索引，适合大表顺序扫描
- **Bitmap (Bitmap Index)**：位图索引，适合低基数列

### 向量索引
- **IVF-PQ (IVF-Product Quantization)**：倒排索引 + 乘积量化，工业级大规模向量搜索
- **LSH (Locality-Sensitive Hashing)**：局部敏感哈希，适合二值向量
- **OPQ (Optimized Product Quantization)**：优化乘积量化，提升 PQ 效果
- **多模态索引 (Multi-modal Index)**：跨模态联合检索

## Capabilities

### New Capabilities

| Capability ID | 说明 |
|--------------|------|
| `index-gin` | GIN 通用倒排索引 |
| `index-gist` | GiST 广义搜索树索引 |
| `index-fulltext` | FULLTEXT 全文索引 |
| `index-brin` | BRIN 块范围索引 |
| `index-bitmap` | Bitmap 位图索引 |
| `vector-ivf-pq` | IVF-PQ 倒排量化索引 |
| `vector-lsh` | LSH 局部敏感哈希索引 |
| `vector-opq` | OPQ 优化乘积量化 |
| `vector-multimodal` | 多模态向量索引 |

### Modified Capabilities

无。

## Impact

### 代码影响

| 目录 | 影响 |
|------|------|
| `src/index/inverted/` | 新增 GIN、GiST、FULLTEXT、Bitmap 实现 |
| `src/index/block/` | 新增 BRIN 实现 |
| `src/index/vector_index/` | 新增 IVF-PQ、LSH、OPQ、多模态索引 |
| `include/index/` | 新增对应头文件 |

### 依赖关系

- GIN/GiST 依赖已有的 B-Tree/B+Tree 实现
- IVF-PQ 复用现有的 IVF 和 PQ 量化器
- OPQ 基于 PQ 扩展
- FULLTEXT 可复用现有的 BM25 模块
- LSH 独立实现，复用距离计算函数
