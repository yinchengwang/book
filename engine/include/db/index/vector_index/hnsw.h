/**
 * @file hnsw.h
 * @brief HNSW 索引类型前向声明（测试用桩）
 */
#ifndef HNSW_H
#define HNSW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明，用于 doc_vector.h 中 void *hnsw_index 成员 */
typedef struct hnsw_index hnsw_index_t;

#ifdef __cplusplus
}
#endif

#endif /* HNSW_H */
