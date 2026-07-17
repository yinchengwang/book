/*
 * diskann_private.h
 *
 * DiskANN 内部结构。
 * 包括图搜索候选、块式持久化页头以及内存态索引主结构。
 */

#ifndef DISKANN_PRIVATE_H
#define DISKANN_PRIVATE_H

#include <db/index/vector_index/diskann/diskann.h>
#include <db/index/vector_index/diskann/diskann_merge.h>
#include <db/index/vector_index/diskann/diskann_spann.h>
#include <db/index/vector_index/diskann/diskann_fresh.h>
#include <db/index/heap/heap_vector_store.h>
#include <db/index/vector_ref.h>

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISKANN_FILE_MAGIC 0x44414B44u
#define DISKANN_FILE_VERSION 2u
#define DISKANN_INVALID_BLOCK INT32_MAX

typedef enum diskann_page_type {
    DISKANN_PAGE_META = 1,
    DISKANN_PAGE_VECTOR = 2,
    DISKANN_PAGE_CODE = 3,
    DISKANN_PAGE_NODE = 4,
    DISKANN_PAGE_EDGE = 5,
    DISKANN_PAGE_FROZEN = 6,
    DISKANN_PAGE_PQMODEL = 7,
} diskann_page_type_t;

typedef struct diskann_scored {
    int32_t id;         /* 候选节点 id。 */
    float distance;     /* 到查询点的距离。 */
    bool expanded;      /* 搜索过程中是否已展开其邻居。 */
} diskann_scored_t;

/* 数组二叉最小堆，直接包装 diskann_scored_t 数组，无需额外内存分配。 */
typedef struct diskann_heap {
    diskann_scored_t *data;
    int32_t capacity;
    int32_t size;
} diskann_heap_t;

/* 文件中每个逻辑页/块的通用头。 */
typedef struct diskann_block_header {
    uint32_t magic;
    uint32_t version;
    uint32_t page_size;
    uint32_t block_no;
    uint32_t next_block;
    uint32_t prev_block;
    uint32_t page_type;
    uint32_t payload_size;
    uint32_t item_count;
} diskann_block_header_t;

/* 某一段连续数据在磁盘块文件中的定位信息。 */
typedef struct diskann_section_locator {
    int32_t first_block;
    int32_t block_count;
    int32_t bytes;
    int32_t item_count;
} diskann_section_locator_t;

/* 元信息页，集中记录索引配置与各 section 位置。 */
typedef struct diskann_meta_page {
    uint32_t magic;
    uint32_t version;
    int32_t meta_block;
    int32_t dims;
    int32_t index_size;
    int32_t build_search_list_size;
    int32_t metric;
    float alpha;
    int32_t n_total;
    int32_t active_count;
    int32_t entry_point;
    int32_t built;
    int32_t page_size;
    int32_t frozen_point_capacity;
    int32_t frozen_point_count;
    int32_t pq_enabled;
    int32_t pq_m;
    int32_t pq_bits;
    int32_t pq_train_max_vectors;
    int32_t pq_code_size;
    int32_t pq_trained_codebook_size;
    int32_t pq_model_float_count;
    diskann_section_locator_t vector_section;
    diskann_section_locator_t code_section;
    diskann_section_locator_t node_section;
    diskann_section_locator_t edge_section;
    diskann_section_locator_t frozen_section;
    diskann_section_locator_t pqmodel_section;
} diskann_meta_page_t;

/* 图节点落盘时的紧凑元数据。 */
typedef struct diskann_node_record {
    int32_t id;
    int32_t neighbor_count;
    uint8_t deleted;
    uint8_t reserved[3];
} diskann_node_record_t;

/* 动态缓冲区，用于分段读写。 */
typedef struct diskann_buffer {
    uint8_t *data;
    int32_t size;
    int32_t capacity;
} diskann_buffer_t;

/* 前向声明 */
typedef struct diskann_merge_context diskann_merge_context_t;
typedef struct diskann_spann_context diskann_spann_context_t;
typedef struct diskann_fresh_context diskann_fresh_context_t;

struct diskann {
    float *vectors;                               /* 原始向量数组（heap_store 为空时使用）。 */
    float *norms;                                 /* 预计算的每个向量平方范数。 */
    uint8_t *codes;                               /* PQ 编码数组。 */
    uint8_t *deleted;                             /* 逻辑删除标记。 */
    int32_t *neighbors;                           /* 扁平化邻接表。 */
    int32_t *neighbor_counts;                     /* 每个节点邻居数量。 */
    int32_t *frozen_points;                       /* 冻结点集合。 */
    int32_t n_total;                              /* 已分配节点数。 */
    int32_t active_count;                         /* 活跃节点数。 */
    int32_t capacity;                             /* 当前容量上限。 */
    int32_t dims;                                 /* 向量维度。 */
    int32_t index_size;                           /* 每个节点的目标邻居数。 */
    int32_t build_search_list_size;               /* 构图时搜索候选大小。 */
    distance_metric_t metric;                /* 距离度量。 */
    float alpha;                                  /* robust prune 参数。 */
    int32_t entry_point;                          /* 搜索入口节点。 */
    bool built;                                   /* 是否完成构图。 */
    diskann_quantization_params_t quantization_params; /* PQ 参数。 */
    diskann_storage_params_t storage_params;      /* 持久化参数。 */
    quantizer_config_t quantizer_config;     /* quantizer 配置。 */
    quantizer_t *quantizer;                  /* 已训练 quantizer。 */
    int32_t pq_code_size;                         /* 单向量 PQ 编码字节数。 */
    int32_t pq_trained_codebook_size;             /* 导出的 PQ 码本大小。 */

    /* Phase 2：Heap 向量存储集成 */
    heap_vector_store_t *heap_store;       /* 向量主存储（Single Source of Truth） */
    vector_ref_t *vector_refs;             /* 向量引用数组 */
    uint32_t vector_refs_size;             /* 引用数组大小 */

    /* 扩展配置（可选） */
    diskann_config_t *config;                /* 统一配置副本（如果使用） */
    diskann_merge_context_t *merge_ctx;      /* Merge-Vamana 上下文 */
    diskann_spann_context_t *spann_ctx;      /* SPANN 上下文 */
    diskann_fresh_context_t *fresh_ctx;      /* FreshDiskANN 上下文 */
};

int32_t diskann_max_i32(int32_t lhs, int32_t rhs);
int32_t diskann_min_i32(int32_t lhs, int32_t rhs);
bool diskann_float_is_valid(float value);
int32_t diskann_payload_bytes_per_block(int32_t page_size);
int32_t diskann_block_count_for_bytes(int32_t page_size, int32_t bytes);
long long diskann_block_offset(int32_t block_no, int32_t page_size);
bool diskann_pq_enabled(const diskann_t *index);
bool diskann_pq_ready(const diskann_t *index);
float diskann_distance_between_ids(const diskann_t *index, int32_t lhs, int32_t rhs);
float diskann_exact_distance_from_query(const diskann_t *index, const float *query, int32_t id);
int diskann_compare_scored(const void *lhs, const void *rhs);
int diskann_ensure_frozen_capacity(diskann_t *index);
int diskann_ensure_vector_capacity(diskann_t *index, int32_t target);
int diskann_ensure_code_storage(diskann_t *index);
void diskann_reset_graph(diskann_t *index);
int diskann_collect_active_ids(const diskann_t *index, int32_t **active_ids, int32_t *count);
int32_t diskann_choose_entry_point_from_active(const diskann_t *index, const int32_t *active_ids, int32_t count);
int diskann_rebuild_frozen_points(diskann_t *index);
int diskann_has_neighbor(const diskann_t *index, int32_t node_id, int32_t neighbor_id);
int diskann_append_unique_candidate(diskann_scored_t *pool,
                                          int32_t *count,
                                          int32_t capacity,
                                          int32_t id,
                                          float distance);
int diskann_robust_prune(const diskann_t *index,
                               int32_t node_id,
                               diskann_scored_t *pool,
                               int32_t pool_count,
                               int32_t *result_ids,
                               int32_t *result_count,
                               int32_t occlude_rounds);
void diskann_set_neighbors_for_node(diskann_t *index, int32_t node_id, const int32_t *neighbors, int32_t count);
int diskann_add_reverse_edge(diskann_t *index, int32_t from_id, int32_t to_id);

/* 二叉堆操作。 */
void diskann_heap_push(diskann_heap_t *heap, diskann_scored_t scored);
diskann_scored_t diskann_heap_pop(diskann_heap_t *heap);
diskann_scored_t diskann_heap_peek(const diskann_heap_t *heap);
void diskann_heap_skip_expanded(diskann_heap_t *heap);
void diskann_heapify(diskann_heap_t *heap);

/* 增量 Vamana Link 流程。 */
int diskann_iterate_to_fixed_point(const diskann_t *index,
                                    int32_t node_id,
                                    diskann_scored_t *pool,
                                    int32_t *pool_count);
int diskann_inter_insert(diskann_t *index,
                          int32_t node_id,
                          const int32_t *result_ids,
                          int32_t result_count);
int diskann_link_node(diskann_t *index, int32_t node_id);
int diskann_build_repair_candidate_pool(const diskann_t *index,
                                              int32_t node_id,
                                              const int32_t *victim_neighbors,
                                              int32_t victim_neighbor_count,
                                              diskann_scored_t *pool,
                                              int32_t pool_capacity,
                                              int32_t *pool_count);
int diskann_repair_neighbors_after_delete(diskann_t *index,
                                                const bool *affected,
                                                int32_t affected_count,
                                                const int32_t *victim_neighbors,
                                                int32_t victim_neighbor_count);
int diskann_maybe_encode_vector(diskann_t *index, int32_t id);
int diskann_encode_all_vectors(diskann_t *index);
int diskann_ensure_quantizer(diskann_t *index);
int diskann_gather_training_vectors(const diskann_t *index, float **vectors_out, int32_t *count_out);
float diskann_candidate_distance_from_query(const diskann_t *index,
                                                  const float *query,
                                                  const float *distance_table,
                                                  int32_t id);
float diskann_fast_l2_distance(const diskann_t *index, int32_t id_a, int32_t id_b);
int diskann_search_candidates(const diskann_t *index,
                                    const float *query,
                                    int32_t search_list_size,
                                    diskann_scored_t *results,
                                    int32_t *result_count);
int diskann_incremental_insert_node(diskann_t *index, int32_t node_id);
void diskann_remove_id_from_row(diskann_t *index, int32_t row_id, int32_t victim_id);
int diskann_buffer_append(diskann_buffer_t *buffer, const void *data, int32_t size);
void diskann_buffer_free(diskann_buffer_t *buffer);
int diskann_write_block(FILE *file,
                              int32_t page_size,
                              int32_t block_no,
                              int32_t prev_block,
                              int32_t next_block,
                              diskann_page_type_t page_type,
                              const void *payload,
                              int32_t payload_size,
                              int32_t item_count);
int diskann_write_section_blocks(FILE *file,
                                       int32_t page_size,
                                       int32_t first_block,
                                       diskann_page_type_t page_type,
                                       const void *data,
                                       int32_t bytes,
                                       int32_t item_count,
                                       diskann_section_locator_t *locator);
int diskann_read_block(FILE *file,
                             int32_t page_size,
                             int32_t block_no,
                             diskann_page_type_t expected_type,
                             diskann_block_header_t *header,
                             uint8_t *payload);
int diskann_read_section_blocks(FILE *file,
                                      int32_t page_size,
                                      diskann_page_type_t page_type,
                                      const diskann_section_locator_t *locator,
                                      diskann_buffer_t *buffer);

/* 统一配置管理 */
diskann_config_t *diskann_config_default(int32_t dims, distance_metric_t metric);
diskann_config_t *diskann_config_create(int32_t dims,
                                        int32_t index_size,
                                        int32_t build_search_list_size,
                                        distance_metric_t metric);
void diskann_config_init_defaults(diskann_config_t *config, int32_t dims, distance_metric_t metric);
void diskann_config_free(diskann_config_t *config);

/* 使用统一配置创建索引 */
diskann_t *diskann_index_create_with_config(const diskann_config_t *config);

/* 扩展搜索函数 */
int32_t diskann_search_in_partitions(diskann_t *index,
                                     const float *query,
                                     const int32_t *selected_partitions,
                                     int32_t selected_count,
                                     const int32_t *partition_ids,
                                     int32_t k,
                                     int32_t search_list_size,
                                     float *distances,
                                     int32_t *labels);
int32_t diskann_search_dual_zone(diskann_t *index,
                                 const float *query,
                                 int32_t k,
                                 int32_t search_list_size,
                                 float *distances,
                                 int32_t *labels);

#endif
