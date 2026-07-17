# ivf_variants — IVF 变体原理演示

## 概述

IVF (Inverted File) 是一类基于聚类的近似最近邻索引，本演示涵盖四种变体：

## 索引类型

### IVF-Flat
- **存储**: 原始向量完整存储
- **特点**: 精度最高，无压缩损耗
- **内存**: O(n × d)，n 为向量数，d 为维度

### IVF-PQ
- **存储**: Product Quantization 有损压缩
- **特点**: 通过分段量化大幅降低内存
- **压缩比**: D × 4 bytes → M bytes (M = 分段数)

### IVF-HNSW
- **存储**: IVF 聚类 + HNSW 图搜索复合索引
- **特点**: 兼顾聚类粗筛和图搜索精定位
- **适用**: 超大规模（百万~十亿级）高召回场景

### 倒排索引
- **结构**: Term → Posting List (doc_ids)
- **特点**: 信息检索经典结构，支持 AND/OR/NOT 查询
- **优化**: 跳表、压缩、缓存

## 编译运行

```bash
make && ./test
```

## 核心流程

1. **训练**: K-Means 聚类生成聚类中心
2. **添加**: 向量路由到最近簇，存入对应倒排桶
3. **搜索**: 查询向量找 nprobe 个最近簇，扫描桶内向量

## 参考实现

- `engineering/src/db/index/vector_index/ivf/`
- `engineering/src/db/index/vector_index/ivf_pq/`
- `engineering/src/db/index/vector_index/ivf_hnsw/`
