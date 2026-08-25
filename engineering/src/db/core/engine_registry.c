/**
 * @file engine_registry.c
 * @brief 存储引擎注册实现
 *
 * 根据 multimodal_config.h 中的编译开关，有条件地注册各引擎。
 */
#include "db/engine_registry.h"
#include "db/multimodal_config.h"
#include "db/storage_engine.h"
#include "log.h"
#include <stdio.h>

/* 引用各引擎的 ops 获取函数 */
#ifdef MMDB_ENABLE_KV
extern const storage_ops_t *kv_engine_get_ops(void);
#endif
#ifdef MMDB_ENABLE_VECTOR
extern const storage_ops_t *vector_engine_get_ops(void);
#endif
#ifdef MMDB_ENABLE_GRAPH
extern const storage_ops_t *graph_engine_get_ops(void);
#endif
#ifdef MMDB_ENABLE_DOCUMENT
extern const storage_ops_t *doc_engine_get_ops(void);
#endif
#ifdef MMDB_ENABLE_TIMESERIES
extern const storage_ops_t *ts_engine_get_ops(void);
#endif
#ifdef MMDB_ENABLE_SPATIAL
extern const storage_ops_t *spatial_engine_get_ops(void);
#endif
#ifdef MMDB_ENABLE_TREE
extern const storage_ops_t *yang_engine_get_ops(void);
#endif
/* Stream 和 Columnar 引擎将在后续阶段实现 */
#ifdef MMDB_ENABLE_STREAM
/* extern const storage_ops_t *stream_engine_get_ops(void); */
#endif
#ifdef MMDB_ENABLE_COLUMNAR
/* extern const storage_ops_t *columnar_engine_get_ops(void); */
#endif

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
    int count = 0;

    /* 注册 KV 引擎 */
#ifdef MMDB_ENABLE_KV
    if (kv_engine_get_ops) {
        const storage_ops_t *ops = kv_engine_get_ops();
        if (ops && register_storage_engine(MODEL_KV, ops) == 0) {
            LOG_INFO("Registered KV engine: %s", ops->name);
            count++;
        }
    }
#endif

    /* 注册向量引擎 */
#ifdef MMDB_ENABLE_VECTOR
    if (vector_engine_get_ops) {
        const storage_ops_t *ops = vector_engine_get_ops();
        if (ops && register_storage_engine(MODEL_VECTOR, ops) == 0) {
            LOG_INFO("Registered Vector engine: %s", ops->name);
            count++;
        }
    }
#endif

    /* 注册图引擎 */
#ifdef MMDB_ENABLE_GRAPH
    if (graph_engine_get_ops) {
        const storage_ops_t *ops = graph_engine_get_ops();
        if (ops && register_storage_engine(MODEL_GRAPH, ops) == 0) {
            LOG_INFO("Registered Graph engine: %s", ops->name);
            count++;
        }
    }
#endif

    /* 注册文档引擎 */
#ifdef MMDB_ENABLE_DOCUMENT
    if (doc_engine_get_ops) {
        const storage_ops_t *ops = doc_engine_get_ops();
        if (ops && register_storage_engine(MODEL_DOCUMENT, ops) == 0) {
            LOG_INFO("Registered Document engine: %s", ops->name);
            count++;
        }
    }
#endif

    /* 注册时序引擎 */
#ifdef MMDB_ENABLE_TIMESERIES
    if (ts_engine_get_ops) {
        const storage_ops_t *ops = ts_engine_get_ops();
        if (ops && register_storage_engine(MODEL_TIMESERIES, ops) == 0) {
            LOG_INFO("Registered Timeseries engine: %s", ops->name);
            count++;
        }
    }
#endif

    /* 注册空间引擎 */
#ifdef MMDB_ENABLE_SPATIAL
    if (spatial_engine_get_ops) {
        const storage_ops_t *ops = spatial_engine_get_ops();
        if (ops && register_storage_engine(MODEL_SPATIAL, ops) == 0) {
            LOG_INFO("Registered Spatial engine: %s", ops->name);
            count++;
        }
    }
#endif

    /* 注册树引擎 */
#ifdef MMDB_ENABLE_TREE
    if (yang_engine_get_ops) {
        const storage_ops_t *ops = yang_engine_get_ops();
        if (ops && register_storage_engine(MODEL_TREE, ops) == 0) {
            LOG_INFO("Registered Tree engine: %s", ops->name);
            count++;
        }
    }
#endif

    /* Stream 引擎（预留） */
#ifdef MMDB_ENABLE_STREAM
    /* stream_engine_get_ops 尚未实现 */
#endif

    /* Columnar 引擎（预留） */
#ifdef MMDB_ENABLE_COLUMNAR
    /* columnar_engine_get_ops 尚未实现 */
#endif

    g_registered_count = count;
    LOG_INFO("Engine registry initialized: %d engines registered", count);
    return count;
}
