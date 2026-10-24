/**
 * @file sparql_engine.h
 * @brief RDF/SPARQL 查询引擎接口
 *
 * Phase12 - 实现 RDF 推理引擎，追赶 Apache Jena 水平。
 */
#ifndef DB_RDF_SPARQL_ENGINE_H
#define DB_RDF_SPARQL_ENGINE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** RDF 图不透明类型 */
typedef struct rdf_graph rdf_graph_t;

/** SPARQL 结果集不透明类型 */
typedef struct sparql_result sparql_result_t;

/** 创建 RDF 图 */
rdf_graph_t *rdf_graph_create(void);

/** 销毁 RDF 图 */
void rdf_graph_destroy(rdf_graph_t *graph);

/** 添加三元组 */
int rdf_add_triple(rdf_graph_t *graph, const char *subject, const char *predicate, const char *object);

/** 删除三元组 */
int rdf_remove_triple(rdf_graph_t *graph, const char *subject, const char *predicate, const char *object);

/** SPARQL SELECT 查询 */
sparql_result_t *sparql_select(rdf_graph_t *graph, const char *query);

/** SPARQL ASK 查询 */
bool sparql_ask(rdf_graph_t *graph, const char *query);

/** 释放结果集 */
void sparql_result_destroy(sparql_result_t *result);

/** 获取结果行数 */
size_t sparql_result_row_count(const sparql_result_t *result);

/** 获取变量名数组 */
const char **sparql_result_variables(const sparql_result_t *result, size_t *out_count);

#ifdef __cplusplus
}
#endif
#endif /* DB_RDF_SPARQL_ENGINE_H */
