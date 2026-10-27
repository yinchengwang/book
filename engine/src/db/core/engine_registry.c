/**
 * @file engine_registry.c
 * @brief 存储引擎注册实现（注册表反转）
 *
 * 公共层不再逐个 #ifdef 引用引擎符号。各启用的模态导出一个
 * mm_<modality>_register()，由构建期按 profile 生成的
 * mm_builtin_engines.c（cmake/gen_builtin_engines.cmake）集中调用；
 * engine_registry_init() 只负责触发注册并统计结果。
 */
#include "db/engine_registry.h"
#include "db/mm_engine_register.h"
#include "db/multimodal_config.h"
#include "db/storage_engine.h"
#include "log.h"
#include <stdio.h>

/* 已注册引擎计数 */
static int g_registered_count = 0;

int register_storage_engine(DataModel model, const storage_ops_t *ops) {
    if (model < 0 || model >= MODEL_COUNT || ops == NULL) {
        LOG_ERROR("Invalid engine registration: model=%d", model);
        return -1;
    }
    return storage_register_engine(model, ops);
}

int get_registered_engine_count(void) {
    return g_registered_count;
}

int engine_registry_init(void) {
    /* 注册表反转：由构建期生成的 mm_builtin_engines.c 按当前 profile
     * 调用各启用的 mm_<modality>_register()。幂等：重复调用时
     * storage_register_engine 对同一引擎返回 0。 */
    mm_register_builtin_engines();

    /* 计数必须遍历槽位而不是累加：storage_register_engine 对重复注册
     * 同一引擎幂等返回 0，累加会虚增计数。10 号槽是保留空槽，
     * 其槽位恒为 NULL，遍历天然跳过。 */
    int count = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            count++;
        }
    }
    g_registered_count = count;
    LOG_INFO("Engine registry initialized: %d engines registered", count);
    return count;
}

/* ========================================================================
 * 各模态注册函数
 *
 * 每个启用的模态导出一个 mm_<modality>_register()，成功返回 0。
 * 由构建期生成的 mm_builtin_engines.c 调用；未启用的模态不编译。
 * ======================================================================== */

#ifdef MMDB_ENABLE_KV
extern const storage_ops_t *kv_engine_get_ops(void);
int mm_kv_register(void) {
    const storage_ops_t *ops = kv_engine_get_ops();
    if (ops && register_storage_engine(MODEL_KV, ops) == 0) {
        LOG_INFO("Registered KV engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif

#ifdef MMDB_ENABLE_VECTOR
extern const storage_ops_t *vector_engine_get_ops(void);
int mm_vector_register(void) {
    const storage_ops_t *ops = vector_engine_get_ops();
    if (ops && register_storage_engine(MODEL_VECTOR, ops) == 0) {
        LOG_INFO("Registered Vector engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif

#ifdef MMDB_ENABLE_GRAPH
extern const storage_ops_t *graph_engine_get_ops(void);
int mm_graph_register(void) {
    const storage_ops_t *ops = graph_engine_get_ops();
    if (ops && register_storage_engine(MODEL_GRAPH, ops) == 0) {
        LOG_INFO("Registered Graph engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif

#ifdef MMDB_ENABLE_DOCUMENT
extern const storage_ops_t *doc_engine_get_ops(void);
int mm_doc_register(void) {
    const storage_ops_t *ops = doc_engine_get_ops();
    if (ops && register_storage_engine(MODEL_DOCUMENT, ops) == 0) {
        LOG_INFO("Registered Document engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif

#ifdef MMDB_ENABLE_TIMESERIES
extern const storage_ops_t *ts_engine_get_ops(void);
int mm_ts_register(void) {
    const storage_ops_t *ops = ts_engine_get_ops();
    if (ops && register_storage_engine(MODEL_TIMESERIES, ops) == 0) {
        LOG_INFO("Registered Timeseries engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif

#ifdef MMDB_ENABLE_SPATIAL
extern const storage_ops_t *spatial_engine_get_ops(void);
int mm_spatial_register(void) {
    const storage_ops_t *ops = spatial_engine_get_ops();
    if (ops && register_storage_engine(MODEL_SPATIAL, ops) == 0) {
        LOG_INFO("Registered Spatial engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif

#ifdef MMDB_ENABLE_TREE
extern const storage_ops_t *yang_engine_get_ops(void);
int mm_tree_register(void) {
    const storage_ops_t *ops = yang_engine_get_ops();
    if (ops && register_storage_engine(MODEL_TREE, ops) == 0) {
        LOG_INFO("Registered Tree engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif

#ifdef MMDB_ENABLE_STREAM
extern const storage_ops_t *stream_engine_get_ops(void);
int mm_stream_register(void) {
    const storage_ops_t *ops = stream_engine_get_ops();
    if (ops && register_storage_engine(MODEL_STREAM, ops) == 0) {
        LOG_INFO("Registered Stream engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif

#ifdef MMDB_ENABLE_COLUMNAR
extern const storage_ops_t *columnar_engine_get_ops(void);
int mm_columnar_register(void) {
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops && register_storage_engine(MODEL_COLUMNAR, ops) == 0) {
        LOG_INFO("Registered Columnar engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif

#ifdef MMDB_ENABLE_ST
extern const storage_ops_t *st_engine_get_ops(void);
int mm_st_register(void) {
    const storage_ops_t *ops = st_engine_get_ops();
    /* 必须用独立的 MODEL_SPATIOTEMPORAL 槽位：复用 MODEL_SPATIAL 会在
     * 同时开启 SPATIAL 与 ST 时静默顶掉空间引擎。 */
    if (ops && register_storage_engine(MODEL_SPATIOTEMPORAL, ops) == 0) {
        LOG_INFO("Registered ST engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif

#ifdef MMDB_ENABLE_RDF
extern const storage_ops_t *rdf_engine_get_ops(void);
int mm_rdf_register(void) {
    const storage_ops_t *ops = rdf_engine_get_ops();
    /* 同上：复用 MODEL_GRAPH 会顶掉图引擎。 */
    if (ops && register_storage_engine(MODEL_RDF, ops) == 0) {
        LOG_INFO("Registered RDF engine: %s", ops->name);
        return 0;
    }
    return -1;
}
#endif
