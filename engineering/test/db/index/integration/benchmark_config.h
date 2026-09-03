/* 基准测试配置 */
#ifndef BENCHMARK_CONFIG_H
#define BENCHMARK_CONFIG_H

#include <stdint.h>

/* SIFT Small 数据集配置（真实数据，10K 向量） */
#define BENCHMARK_DIMS 128
#define BENCHMARK_N_VECTORS 10000
#define BENCHMARK_N_QUERIES 100
#define BENCHMARK_K 10

/* 召回率阈值（SIFT 聚类数据，ANN 算法可达到 90%+） */
#define HNSW_RECALL_THRESHOLD 0.90f
#define DISKANN_RECALL_THRESHOLD 0.85f
#define IVF_RECALL_THRESHOLD 0.85f
/* LSH：多探针配置下可达 70%+ */
#define LSH_RECALL_THRESHOLD 0.70f

/* 性能阈值 */
#define MAX_QUERY_LATENCY_MS 5.0

#endif /* BENCHMARK_CONFIG_H */
