/**
 * @file rdf_index.h
 * @brief RDF 索引接口
 */
#ifndef RDF_INDEX_H
#define RDF_INDEX_H

#include "db/rdf_engine.h"

/** 初始化索引 */
int rdf_index_init(void);

/** 关闭索引 */
void rdf_index_shutdown(void);

/** 添加三元组到索引 */
int rdf_index_add_triple(int64_t triple_id, const rdf_triple_t *triple);

#endif /* RDF_INDEX_H */
