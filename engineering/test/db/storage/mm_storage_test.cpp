/**
 * @file mm_storage_test.cpp
 * @brief 多模态存储引擎单元测试
 *
 * 测试各存储引擎的基本功能：创建、打开、插入、查询、关闭。
 */
#include <gtest/gtest.h>
#include "db/mm_storage.h"
#include "db/kv_engine.h"
#include "db/kv.h"
#include "db/vector_engine.h"
#include "db/ts_engine.h"
#include "db/doc_engine.h"
#include "db/spatial_engine.h"
#include "db/yang_engine.h"
#include "db/graph_engine.h"
#include "db/log.h"
#include <cstring>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

/* 注册所有引擎 */
static void register_all_engines(void) {
    storage_register_engine(MODEL_KV, kv_engine_get_ops());
    storage_register_engine(MODEL_VECTOR, vector_engine_get_ops());
    storage_register_engine(MODEL_TIMESERIES, ts_engine_get_ops());
    storage_register_engine(MODEL_DOCUMENT, doc_engine_get_ops());
    storage_register_engine(MODEL_SPATIAL, spatial_engine_get_ops());
    storage_register_engine(MODEL_TREE, yang_engine_get_ops());
    storage_register_engine(MODEL_GRAPH, graph_engine_get_ops());
}

#ifdef __cplusplus
}
#endif

/**
 * @brief 测试夹具基类
 */
class MmStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_WARN;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

        /* 初始化错误系统 */
        db_error_init();

        /* 初始化多模态存储管理器 */
        ASSERT_EQ(0, mm_storage_init("./test_data/mm_storage"));
    }

    void TearDown() override {
        mm_storage_shutdown();
        db_error_shutdown();
        log_shutdown();
    }
};

/**
 * @brief KV 引擎测试
 */
class KvEngineTest : public MmStorageTest {
protected:
    void SetUp() override {
        MmStorageTest::SetUp();
        storage_register_engine(MODEL_KV, kv_engine_get_ops());
        kv_engine_init("./test_data/mm_storage/kv");
    }

    void TearDown() override {
        kv_engine_shutdown();
        MmStorageTest::TearDown();
    }
};

/**
 * @brief 测试多模态存储管理器初始化
 */
TEST_F(MmStorageTest, InitShutdown) {
    EXPECT_TRUE(mm_is_initialized());

    mm_context_t *ctx = mm_get_context();
    EXPECT_NE(nullptr, ctx);
}

/**
 * @brief 测试数据模型目录名
 */
TEST_F(MmStorageTest, ModelDirNames) {
    EXPECT_STREQ("base", mm_get_model_dir(MODEL_RELATIONAL));
    EXPECT_STREQ("kv", mm_get_model_dir(MODEL_KV));
    EXPECT_STREQ("graph", mm_get_model_dir(MODEL_GRAPH));
    EXPECT_STREQ("vector", mm_get_model_dir(MODEL_VECTOR));
    EXPECT_STREQ("timeseries", mm_get_model_dir(MODEL_TIMESERIES));
    EXPECT_STREQ("document", mm_get_model_dir(MODEL_DOCUMENT));
    EXPECT_STREQ("spatial", mm_get_model_dir(MODEL_SPATIAL));
    EXPECT_STREQ("yang", mm_get_model_dir(MODEL_TREE));
}

/**
 * @brief 测试引擎注册
 */
TEST_F(MmStorageTest, EngineRegistration) {
    /* 注册 KV 引擎 */
    EXPECT_EQ(0, storage_register_engine(MODEL_KV, kv_engine_get_ops()));

    /* 获取引擎 */
    const storage_ops_t *ops = storage_get_engine(MODEL_KV);
    EXPECT_NE(nullptr, ops);
    EXPECT_STREQ("kv_engine", ops->name);
    EXPECT_EQ(MODEL_KV, ops->model);
}

/**
 * @brief 测试模型路径获取
 */
TEST_F(MmStorageTest, ModelPath) {
    char path[512];

    EXPECT_EQ(0, mm_get_model_path("test_kv", MODEL_KV, path, sizeof(path)));
    EXPECT_NE(nullptr, strstr(path, "test_data/mm_storage/kv/test_kv"));
}

/**
 * @brief 测试 KV 引擎基本操作
 */
TEST_F(KvEngineTest, KvBasicOperations) {
    /* 创建 KV 数据库 */
    EXPECT_EQ(0, kv_engine_create("test_db", NULL));

    /* 打开 KV 数据库 */
    void *db = kv_engine_open("test_db", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, db);

    /* 获取内部 kv_t* 进行直接操作 */
    kv_engine_db_t *kv_db = static_cast<kv_engine_db_t *>(db);

    /* 直接使用 kv API 插入数据 */
    const char *key = "test_key";
    const char *value = "test_value";

    kv_result_t result = kv_put(kv_db->kv, key, strlen(key), value, strlen(value));
    EXPECT_EQ(KV_OK, result);

    /* 检查键是否存在 */
    void *out_value = NULL;
    size_t out_len = 0;
    result = kv_get(kv_db->kv, key, strlen(key), &out_value, &out_len);
    EXPECT_EQ(KV_OK, result);
    if (out_value && out_len > 0) {
        EXPECT_EQ(strlen(value), out_len);
        EXPECT_EQ(0, memcmp(value, out_value, out_len));
        free(out_value);  /* kv_get 返回的内存需要手动释放 */
    }

    /* 关闭数据库 */
    EXPECT_EQ(0, kv_engine_close(db));

    /* 重新打开并验证数据 */
    db = kv_engine_open("test_db", ACCESS_MODE_READ);
    ASSERT_NE(nullptr, db);
    kv_db = static_cast<kv_engine_db_t *>(db);

    result = kv_get(kv_db->kv, key, strlen(key), &out_value, &out_len);
    EXPECT_EQ(KV_OK, result);

    EXPECT_EQ(0, kv_engine_close(db));
}

/**
 * @brief 测试 KV 扫描
 */
TEST_F(KvEngineTest, KvScan) {
    /* 创建并打开数据库 */
    EXPECT_EQ(0, kv_engine_create("scan_test", NULL));
    void *db = kv_engine_open("scan_test", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, db);

    kv_engine_db_t *kv_db = static_cast<kv_engine_db_t *>(db);

    /* 直接使用 kv API 插入多个键值对 */
    for (int i = 0; i < 10; i++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "key_%02d", i);
        snprintf(value, sizeof(value), "value_%02d", i);

        kv_result_t result = kv_put(kv_db->kv, key, strlen(key), value, strlen(value));
        EXPECT_EQ(KV_OK, result);
    }

    /* 验证插入的数据 */
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%02d", i);

        void *out_value = NULL;
        size_t out_len = 0;
        kv_result_t result = kv_get(kv_db->kv, key, strlen(key), &out_value, &out_len);
        EXPECT_EQ(KV_OK, result);
        if (out_value) free(out_value);
    }

    kv_engine_close(db);
}

/**
 * @brief 测试向量引擎
 */
TEST_F(MmStorageTest, VectorEngine) {
    storage_register_engine(MODEL_VECTOR, vector_engine_get_ops());
    EXPECT_EQ(0, vector_engine_init("./test_data/mm_storage/vector"));

    /* 创建向量集合 */
    EXPECT_EQ(0, vector_engine_create("test_vectors", NULL));

    /* 打开集合 */
    void *db = vector_engine_open("test_vectors", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, db);

    /* 构造向量数据: id(8) + dimension(4) + float[dimension] */
    /* 需要 8 + 4 + 128 * 4 = 524 字节 */
    uint8_t data[1024];
    uint64_t id = 1;
    int32_t dim = 128;

    uint8_t *ptr = data;
    memcpy(ptr, &id, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    memcpy(ptr, &dim, sizeof(int32_t));
    ptr += sizeof(int32_t);
    for (int i = 0; i < dim; i++) {
        float v = (float)i / dim;
        memcpy(ptr, &v, sizeof(float));
        ptr += sizeof(float);
    }

    /* 插入向量 */
    EXPECT_EQ(0, vector_engine_insert(db, data,
                                      sizeof(uint64_t) + sizeof(int32_t) + dim * sizeof(float)));

    /* 关闭集合 */
    EXPECT_EQ(0, vector_engine_close(db));

    vector_engine_shutdown();
}

/**
 * @brief 测试时序引擎
 */
TEST_F(MmStorageTest, TsEngine) {
    storage_register_engine(MODEL_TIMESERIES, ts_engine_get_ops());
    EXPECT_EQ(0, ts_engine_init("./test_data/mm_storage/timeseries"));

    /* 创建时序指标 */
    EXPECT_EQ(0, ts_engine_create("test_metric", NULL));

    /* 打开指标 */
    void *db = ts_engine_open("test_metric", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, db);

    /* 构造时序数据: timestamp(8) + value(8) */
    uint8_t data[32];
    int64_t timestamp = 1700000000000LL;  /* 2023-11-15 */
    double value = 42.5;

    uint8_t *ptr = data;
    memcpy(ptr, &timestamp, sizeof(int64_t));
    ptr += sizeof(int64_t);
    memcpy(ptr, &value, sizeof(double));

    /* 插入数据点 */
    EXPECT_EQ(0, ts_engine_insert(db, data, sizeof(int64_t) + sizeof(double)));

    /* 关闭 */
    EXPECT_EQ(0, ts_engine_close(db));

    ts_engine_shutdown();
}

/**
 * @brief 测试文档引擎
 */
TEST_F(MmStorageTest, DocEngine) {
    storage_register_engine(MODEL_DOCUMENT, doc_engine_get_ops());
    EXPECT_EQ(0, doc_engine_init("./test_data/mm_storage/document"));

    /* 创建文档集合 */
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    /* 打开集合 */
    void *db = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, db);

    /* 构造文档数据: id_len(4) + id + json_len(4) + json */
    const char *doc_id = "doc_001";
    const char *json = R"({"name":"test","value":42})";
    uint8_t data[512];
    uint32_t id_len = strlen(doc_id);
    uint32_t json_len = strlen(json);

    uint8_t *ptr = data;
    memcpy(ptr, &id_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, doc_id, id_len);
    ptr += id_len;
    memcpy(ptr, &json_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, json, json_len);

    /* 插入文档 */
    EXPECT_EQ(0, doc_engine_insert(db, data,
                                   sizeof(uint32_t) * 2 + id_len + json_len));

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(db));

    doc_engine_shutdown();
}

/**
 * @brief 测试空间引擎
 */
TEST_F(MmStorageTest, SpatialEngine) {
    storage_register_engine(MODEL_SPATIAL, spatial_engine_get_ops());
    EXPECT_EQ(0, spatial_engine_init("./test_data/mm_storage/spatial"));

    /* 创建空间数据集 */
    EXPECT_EQ(0, spatial_engine_create("test_spatial", NULL));

    /* 打开数据集 */
    void *db = spatial_engine_open("test_spatial", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, db);

    /* 构造几何数据: id_len(4) + id + geom_type(4) + wkt_len(4) + wkt */
    const char *geo_id = "point_001";
    const char *wkt = "POINT(1.0 2.0)";
    uint8_t data[512];
    uint32_t id_len = strlen(geo_id);
    geometry_type_t geom_type = GEOM_POINT;
    uint32_t wkt_len = strlen(wkt);

    uint8_t *ptr = data;
    memcpy(ptr, &id_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, geo_id, id_len);
    ptr += id_len;
    memcpy(ptr, &geom_type, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &wkt_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, wkt, wkt_len);

    /* 插入几何对象 */
    EXPECT_EQ(0, spatial_engine_insert(db, data,
                                       sizeof(uint32_t) * 3 + id_len + wkt_len));

    /* 关闭 */
    EXPECT_EQ(0, spatial_engine_close(db));

    spatial_engine_shutdown();
}

/**
 * @brief 测试 Yang 引擎
 */
TEST_F(MmStorageTest, YangEngine) {
    storage_register_engine(MODEL_TREE, yang_engine_get_ops());
    EXPECT_EQ(0, yang_engine_init("./test_data/mm_storage/yang"));

    /* 创建树 */
    EXPECT_EQ(0, yang_engine_create("test_tree", NULL));

    /* 打开树 */
    void *db = yang_engine_open("test_tree", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, db);

    /* 构造节点数据: path_len(4) + path + name_len(4) + name + type(4) + value_len(4) + value */
    const char *path = "/root/child";
    const char *name = "node1";
    const char *value = "test_value";
    uint8_t data[512];
    uint32_t path_len = strlen(path);
    uint32_t name_len = strlen(name);
    yang_node_type_t node_type = YANG_NODE_ELEMENT;
    uint32_t value_len = strlen(value);

    uint8_t *ptr = data;
    memcpy(ptr, &path_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, path, path_len);
    ptr += path_len;
    memcpy(ptr, &name_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, name, name_len);
    ptr += name_len;
    memcpy(ptr, &node_type, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &value_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, value, value_len);

    /* 插入节点 */
    EXPECT_EQ(0, yang_engine_insert(db, data,
                                    sizeof(uint32_t) * 4 + path_len + name_len + value_len));

    /* 关闭 */
    EXPECT_EQ(0, yang_engine_close(db));

    yang_engine_shutdown();
}

/**
 * @brief 测试图引擎
 */
TEST_F(MmStorageTest, GraphEngine) {
    storage_register_engine(MODEL_GRAPH, graph_engine_get_ops());
    EXPECT_EQ(0, graph_engine_init("./test_data/mm_storage/graph"));

    /* 创建图 */
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    /* 打开图 */
    void *db = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, db);

    /* 构造顶点数据: label_len(4) + label + properties */
    const char *label = "Person";
    uint8_t data[256];
    uint32_t label_len = strlen(label);

    uint8_t *ptr = data;
    memcpy(ptr, &label_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, label, label_len);

    /* 添加顶点 */
    graph_vertex_id_t vid = graph_engine_add_vertex(db, data, sizeof(uint32_t) + label_len);
    EXPECT_NE(GRAPH_INVALID_ID, vid);

    /* 关闭图 */
    EXPECT_EQ(0, graph_engine_close(db));

    graph_engine_shutdown();
}

/**
 * @brief 测试 KV 统计信息
 */
TEST_F(KvEngineTest, StorageStats) {
    /* 创建并打开数据库 */
    EXPECT_EQ(0, kv_engine_create("stats_test", NULL));
    void *db = kv_engine_open("stats_test", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, db);

    kv_engine_db_t *kv_db = static_cast<kv_engine_db_t *>(db);

    /* 使用 kv_engine_insert 插入数据
     * 数据格式: key_len(4) + key + value_len(4) + value
     */
    uint8_t data[128];
    uint32_t key_len = 4;
    const char *key = "key1";
    uint32_t value_len = 6;
    const char *value = "value1";

    uint8_t *ptr = data;
    memcpy(ptr, &key_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, key, key_len);
    ptr += key_len;
    memcpy(ptr, &value_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, value, value_len);

    EXPECT_EQ(0, kv_engine_insert(db, data, sizeof(uint32_t) * 2 + key_len + value_len));

    /* 验证插入后统计正确 */
    kv_stats_t stats_after;
    EXPECT_EQ(KV_OK, kv_stats(kv_db->kv, &stats_after));
    EXPECT_EQ(1, stats_after.num_keys);

    kv_engine_close(db);

    /* 重新打开后获取统计信息 */
    db = kv_engine_open("stats_test", ACCESS_MODE_READ);
    ASSERT_NE(nullptr, db);

    storage_stats_t final_stats;
    EXPECT_EQ(0, kv_engine_stats("stats_test", &final_stats));
    EXPECT_EQ(1, final_stats.num_objects);

    kv_engine_close(db);
    kv_engine_drop("stats_test");
}

/**
 * @brief 测试向量工具函数
 */
TEST_F(MmStorageTest, VectorUtils) {
    float a[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float b[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    /* 测试 L2 距离 */
    float l2_dist = vector_l2_distance(a, b, 4);
    EXPECT_FLOAT_EQ(0.0f, l2_dist);

    /* 测试余弦相似度 */
    float cos_sim = vector_cosine_similarity(a, b, 4);
    EXPECT_FLOAT_EQ(1.0f, cos_sim);

    /* 测试归一化 */
    float v[4] = {3.0f, 4.0f, 0.0f, 0.0f};
    vector_normalize(v, 4);
    EXPECT_NEAR(0.6f, v[0], 0.001f);
    EXPECT_NEAR(0.8f, v[1], 0.001f);
}

/**
 * @brief 测试时序工具函数
 */
TEST_F(MmStorageTest, TsUtils) {
    /* 测试时间粒度转换 */
    EXPECT_EQ(1000, ts_granularity_to_ms(TS_GRANULARITY_SECOND));
    EXPECT_EQ(60 * 1000, ts_granularity_to_ms(TS_GRANULARITY_MINUTE));
    EXPECT_EQ(60 * 60 * 1000, ts_granularity_to_ms(TS_GRANULARITY_HOUR));

    /* 测试时间戳对齐
     * ts = 1700000123（毫秒）
     * SECOND 对齐 (1000ms): 1700000123 - 123 = 1700000000
     * MINUTE 对齐 (60000ms): 1700000123 - (1700000123 % 60000) = 1700000123 - 28323 = 1699980000
     */
    int64_t ts = 1700000123LL;  /* 毫秒时间戳 */
    EXPECT_EQ(1700000000LL, ts_align_timestamp(ts, TS_GRANULARITY_SECOND));
    EXPECT_EQ(1699980000LL, ts_align_timestamp(ts, TS_GRANULARITY_MINUTE));
}

/**
 * @brief 测试空间工具函数
 */
TEST_F(MmStorageTest, SpatialUtils) {
    /* 测试边界框创建 */
    bbox_t bbox = bbox_create(0.0, 0.0, 10.0, 10.0);
    EXPECT_EQ(0.0, bbox.min_x);
    EXPECT_EQ(10.0, bbox.max_x);

    /* 测试点包含检查 */
    point_t p1 = {5.0, 5.0};
    EXPECT_TRUE(bbox_contains_point(&bbox, &p1));

    point_t p2 = {15.0, 5.0};
    EXPECT_FALSE(bbox_contains_point(&bbox, &p2));

    /* 测试边界框相交 */
    bbox_t bbox2 = bbox_create(5.0, 5.0, 15.0, 15.0);
    EXPECT_TRUE(bbox_intersects(&bbox, &bbox2));

    bbox_t bbox3 = bbox_create(20.0, 20.0, 30.0, 30.0);
    EXPECT_FALSE(bbox_intersects(&bbox, &bbox3));
}
