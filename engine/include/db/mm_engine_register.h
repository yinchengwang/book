/**
 * @file mm_engine_register.h
 * @brief 内置引擎注册入口（注册表反转）
 *
 * 公共层不再直接引用任何引擎符号。每个启用的模态导出一个
 * mm_<modality>_register()，由构建期按当前 profile 生成的
 * mm_builtin_engines.c（见 cmake/gen_builtin_engines.cmake）集中调用。
 *
 * 生成的定义每次构建都存在（空 profile 产出空函数体），
 * 因此不依赖弱符号（MSVC 不支持弱符号）。
 */
#ifndef DB_MM_ENGINE_REGISTER_H
#define DB_MM_ENGINE_REGISTER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册当前 profile 启用的全部内置存储引擎
 *
 * 由生成的 mm_builtin_engines.c 提供。幂等：重复调用时
 * storage_register_engine 对同一引擎返回 0。
 */
void mm_register_builtin_engines(void);

/**
 * @brief 注册当前 profile 启用的全部模态 SQL 函数集
 *
 * 由生成的 mm_builtin_engines.c 提供；模态通过
 * mm_<modality>_register_sql_functions()（func_registry 拆分后）
 * 向 SQL 函数注册表登记自己的函数。任一模态注册失败返回 -1。
 */
int mm_register_builtin_sql_functions(void);

/* ========================================================================
 * 各模态注册函数
 *
 * 定义随模态编译：模态禁用时无定义（生成文件也只为启用模态生成调用，
 * 因此不存在未定义符号被引用的问题）。统一在此声明，供生成文件与
 * 各模态未来的注册实现共用。
 * ======================================================================== */
int mm_kv_register(void);
int mm_vector_register(void);
int mm_graph_register(void);
int mm_doc_register(void);
int mm_ts_register(void);
int mm_spatial_register(void);
int mm_tree_register(void);
int mm_stream_register(void);
int mm_columnar_register(void);
int mm_st_register(void);
int mm_rdf_register(void);

/* 各模态内置 SQL 函数注册（定义随模态编译，Task 3 起） */
int mm_vector_register_sql_functions(void);
int mm_ts_register_sql_functions(void);
int mm_graph_register_sql_functions(void);
int mm_doc_register_sql_functions(void);
int mm_spatial_register_sql_functions(void);
int mm_kv_register_sql_functions(void);
int mm_tree_register_sql_functions(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_MM_ENGINE_REGISTER_H */
