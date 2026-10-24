/**
 * @file rdf_engine_test.cpp
 * @brief RDF 知识图谱引擎综合测试
 *
 * 测试范围:
 * 1. RDF engine lifecycle (init, shutdown)
 * 2. Triple store operations (insert, delete, match)
 * 3. RDF engine lifecycle (create, open, close, drop)
 * 4. SPARQL query execution (SELECT, ASK)
 * 5. Triple pattern matching
 * 6. Term creation and comparison
 */
#include "db/rdf_engine.h"
#include "rdf_index.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

class RdfEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "test-results/engineering/rdf_test";
        mkdir(test_dir.c_str());
        rdf_engine_init(test_dir.c_str());
        rdf_index_init();
    }

    void TearDown() override {
        rdf_index_shutdown();
        rdf_engine_shutdown();
    }

    std::string test_dir;
};

/* ========================================================================
 * 生命周期测试
 * ======================================================================== */

TEST_F(RdfEngineTest, InitShutdown) {
    // 已经在 SetUp/TearDown 中测试
    SUCCEED();
}

TEST_F(RdfEngineTest, InitWithNullDir) {
    rdf_engine_shutdown();
    // 使用 NULL 应该使用默认目录
    int ret = rdf_engine_init(NULL);
    EXPECT_EQ(ret, 0);
    rdf_engine_shutdown();
    // 重新初始化以便后续测试
    rdf_engine_init(test_dir.c_str());
}

TEST_F(RdfEngineTest, DoubleInit) {
    // 重复初始化应该返回成功
    int ret = rdf_engine_init(test_dir.c_str());
    EXPECT_EQ(ret, 0);
}

/* ========================================================================
 * 图创建/打开/关闭/删除测试
 * ======================================================================== */

TEST_F(RdfEngineTest, GraphCreateOpenClose) {
    const char *graph_name = "test_graph_basic";

    // 获取引擎操作表
    const storage_ops_t *ops = rdf_engine_get_ops();
    ASSERT_NE(ops, nullptr);
    EXPECT_STREQ(ops->name, "rdf_engine");

    // 创建图
    int ret = ops->table_create(graph_name, nullptr);
    EXPECT_EQ(ret, 0);

    // 打开图
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 关闭图
    ret = ops->table_close(db);
    EXPECT_EQ(ret, 0);

    // 清理
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, GraphCreateDuplicate) {
    const char *graph_name = "test_dup";

    const storage_ops_t *ops = rdf_engine_get_ops();

    // 创建第一个图
    int ret = ops->table_create(graph_name, nullptr);
    EXPECT_EQ(ret, 0);

    // 尝试重复创建应该成功（EEXIST）
    ret = ops->table_create(graph_name, nullptr);
    EXPECT_EQ(ret, 0);

    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, GraphOpenNonExistent) {
    const storage_ops_t *ops = rdf_engine_get_ops();

    // 打开不存在的图应该返回 NULL
    void *db = ops->table_open("non_existent_graph", ACCESS_MODE_READ);
    EXPECT_EQ(db, nullptr);
}

TEST_F(RdfEngineTest, GraphDrop) {
    const char *graph_name = "test_drop";
    const storage_ops_t *ops = rdf_engine_get_ops();

    // 创建图
    ops->table_create(graph_name, nullptr);

    // 删除图
    int ret = ops->table_drop(graph_name);
    EXPECT_EQ(ret, 0);

    // 再次删除应该也成功
    ret = ops->table_drop(graph_name);
    EXPECT_EQ(ret, 0);
}

TEST_F(RdfEngineTest, GraphDropNull) {
    const storage_ops_t *ops = rdf_engine_get_ops();

    // 删除 NULL 应该返回错误
    int ret = ops->table_drop(NULL);
    EXPECT_NE(ret, 0);
}

/* ========================================================================
 * 三元组插入和匹配测试
 * ======================================================================== */

TEST_F(RdfEngineTest, InsertTriples) {
    const char *graph_name = "test_insert";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 创建三元组
    rdf_triple_t triple1;
    triple1.subject = rdf_term_uri("http://example.org/person1");
    triple1.predicate = rdf_term_uri("http://example.org/name");
    triple1.object = rdf_term_literal("Alice", "en", NULL);
    triple1.timestamp = 0;

    rdf_triple_t triple2;
    triple2.subject = rdf_term_uri("http://example.org/person2");
    triple2.predicate = rdf_term_uri("http://example.org/name");
    triple2.object = rdf_term_literal("Bob", "en", NULL);
    triple2.timestamp = 0;

    // 插入三元组
    int ret = ops->tuple_insert(db, &triple1, sizeof(triple1));
    EXPECT_EQ(ret, 0);

    ret = ops->tuple_insert(db, &triple2, sizeof(triple2));
    EXPECT_EQ(ret, 0);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, InsertNullHandle) {
    const storage_ops_t *ops = rdf_engine_get_ops();

    rdf_triple_t triple;
    memset(&triple, 0, sizeof(triple));

    int ret = ops->tuple_insert(NULL, &triple, sizeof(triple));
    EXPECT_NE(ret, 0);
}

TEST_F(RdfEngineTest, TripleMatchAll) {
    const char *graph_name = "test_match_all";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 插入多个三元组
    rdf_triple_t triples[] = {
        {rdf_term_uri("http://example.org/s1"), rdf_term_uri("http://example.org/p1"), rdf_term_literal("obj1", NULL, NULL), 0},
        {rdf_term_uri("http://example.org/s2"), rdf_term_uri("http://example.org/p2"), rdf_term_literal("obj2", NULL, NULL), 0},
        {rdf_term_uri("http://example.org/s3"), rdf_term_uri("http://example.org/p1"), rdf_term_literal("obj3", NULL, NULL), 0},
    };

    for (size_t i = 0; i < sizeof(triples)/sizeof(triples[0]); i++) {
        ops->tuple_insert(db, &triples[i], sizeof(triples[i]));
    }

    // 匹配所有三元组
    rdf_triple_t results[10];
    int32_t num_results = 0;
    int ret = rdf_engine_match(db, NULL, NULL, NULL, results, 10, &num_results);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 3);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, TripleMatchBySubject) {
    const char *graph_name = "test_match_s";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    rdf_triple_t triple = {rdf_term_uri("http://example.org/specific"), rdf_term_uri("http://example.org/p"), rdf_term_literal("o", NULL, NULL), 0};
    ops->tuple_insert(db, &triple, sizeof(triple));

    rdf_term_t subject_pattern = rdf_term_uri("http://example.org/specific");

    rdf_triple_t results[10];
    int32_t num_results = 0;
    int ret = rdf_engine_match(db, &subject_pattern, NULL, NULL, results, 10, &num_results);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 1);
    EXPECT_TRUE(rdf_term_equals(&results[0].subject, &subject_pattern));

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, TripleMatchByPredicate) {
    const char *graph_name = "test_match_p";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 插入不同谓语的三元组
    rdf_triple_t t1 = {rdf_term_uri("http://example.org/s1"), rdf_term_uri("http://example.org/type"), rdf_term_uri("http://example.org/Person"), 0};
    rdf_triple_t t2 = {rdf_term_uri("http://example.org/s2"), rdf_term_uri("http://example.org/name"), rdf_term_literal("test", NULL, NULL), 0};
    ops->tuple_insert(db, &t1, sizeof(t1));
    ops->tuple_insert(db, &t2, sizeof(t2));

    rdf_term_t pred_pattern = rdf_term_uri("http://example.org/type");

    rdf_triple_t results[10];
    int32_t num_results = 0;
    int ret = rdf_engine_match(db, NULL, &pred_pattern, NULL, results, 10, &num_results);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 1);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, TripleMatchByObject) {
    const char *graph_name = "test_match_o";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    rdf_triple_t t1 = {rdf_term_uri("http://example.org/s1"), rdf_term_uri("http://example.org/p"), rdf_term_literal("target", NULL, NULL), 0};
    rdf_triple_t t2 = {rdf_term_uri("http://example.org/s2"), rdf_term_uri("http://example.org/p"), rdf_term_literal("other", NULL, NULL), 0};
    ops->tuple_insert(db, &t1, sizeof(t1));
    ops->tuple_insert(db, &t2, sizeof(t2));

    rdf_term_t obj_pattern = rdf_term_literal("target", NULL, NULL);

    rdf_triple_t results[10];
    int32_t num_results = 0;
    int ret = rdf_engine_match(db, NULL, NULL, &obj_pattern, results, 10, &num_results);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 1);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, TripleMatchFullPattern) {
    const char *graph_name = "test_match_full";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    rdf_triple_t triple = {rdf_term_uri("http://example.org/s"), rdf_term_uri("http://example.org/p"), rdf_term_literal("o", NULL, NULL), 0};
    ops->tuple_insert(db, &triple, sizeof(triple));

    rdf_triple_t results[10];
    int32_t num_results = 0;
    int ret = rdf_engine_match(db, &triple.subject, &triple.predicate, &triple.object, results, 10, &num_results);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 1);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, TripleMatchNoResults) {
    const char *graph_name = "test_match_none";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    rdf_triple_t results[10];
    int32_t num_results = 0;
    int ret = rdf_engine_match(db, NULL, NULL, NULL, results, 10, &num_results);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 0);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, TripleMatchMaxResults) {
    const char *graph_name = "test_match_limit";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 插入5个三元组
    for (int i = 0; i < 5; i++) {
        char s[128], p[128], o[32];
        snprintf(s, sizeof(s), "http://example.org/s%d", i);
        snprintf(p, sizeof(p), "http://example.org/p%d", i);
        snprintf(o, sizeof(o), "obj%d", i);
        rdf_triple_t t = {rdf_term_uri(s), rdf_term_uri(p), rdf_term_literal(o, NULL, NULL), 0};
        ops->tuple_insert(db, &t, sizeof(t));
    }

    // 只请求3个结果
    rdf_triple_t results[3];
    int32_t num_results = 0;
    int ret = rdf_engine_match(db, NULL, NULL, NULL, results, 3, &num_results);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 3);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

/* ========================================================================
 * 出边/入边查询测试
 * ======================================================================== */

TEST_F(RdfEngineTest, GetOutgoingEdges) {
    const char *graph_name = "test_outgoing";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    rdf_term_t subject = rdf_term_uri("http://example.org/person1");

    // 插入同一个主语的多个三元组
    rdf_triple_t t1 = {subject, rdf_term_uri("http://example.org/name"), rdf_term_literal("Alice", "en", NULL), 0};
    rdf_triple_t t2 = {subject, rdf_term_uri("http://example.org/age"), rdf_term_literal("30", NULL, NULL), 0};
    rdf_triple_t t3 = {rdf_term_uri("http://example.org/other"), rdf_term_uri("http://example.org/name"), rdf_term_literal("Bob", "en", NULL), 0};

    ops->tuple_insert(db, &t1, sizeof(t1));
    ops->tuple_insert(db, &t2, sizeof(t2));
    ops->tuple_insert(db, &t3, sizeof(t3));

    rdf_triple_t results[10];
    int32_t num_results = 0;
    int ret = rdf_engine_get_outgoing(db, &subject, results, 10, &num_results);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 2);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, GetIncomingEdges) {
    const char *graph_name = "test_incoming";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    rdf_term_t object = rdf_term_uri("http://example.org/target");

    // 插入指向同一个宾语的多个三元组
    rdf_triple_t t1 = {rdf_term_uri("http://example.org/s1"), rdf_term_uri("http://example.org/knows"), object, 0};
    rdf_triple_t t2 = {rdf_term_uri("http://example.org/s2"), rdf_term_uri("http://example.org/knows"), object, 0};
    rdf_triple_t t3 = {rdf_term_uri("http://example.org/s3"), rdf_term_uri("http://example.org/likes"), rdf_term_uri("http://example.org/other"), 0};

    ops->tuple_insert(db, &t1, sizeof(t1));
    ops->tuple_insert(db, &t2, sizeof(t2));
    ops->tuple_insert(db, &t3, sizeof(t3));

    rdf_triple_t results[10];
    int32_t num_results = 0;
    int ret = rdf_engine_get_incoming(db, &object, results, 10, &num_results);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 2);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

/* ========================================================================
 * 三元组删除测试
 * ======================================================================== */

TEST_F(RdfEngineTest, DeleteTriple) {
    const char *graph_name = "test_delete";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    rdf_triple_t triple = {rdf_term_uri("http://example.org/s"), rdf_term_uri("http://example.org/p"), rdf_term_literal("o", NULL, NULL), 0};
    ops->tuple_insert(db, &triple, sizeof(triple));

    // 验证插入成功
    rdf_triple_t results[10];
    int32_t num_results = 0;
    rdf_engine_match(db, NULL, NULL, NULL, results, 10, &num_results);
    EXPECT_EQ(num_results, 1);

    // 删除
    int ret = rdf_engine_delete(db, &triple);
    EXPECT_EQ(ret, 0);

    // 验证删除成功
    num_results = 0;
    rdf_engine_match(db, NULL, NULL, NULL, results, 10, &num_results);
    EXPECT_EQ(num_results, 0);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, DeleteTripleNotFound) {
    const char *graph_name = "test_delete_notfound";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    rdf_triple_t triple = {rdf_term_uri("http://example.org/s"), rdf_term_uri("http://example.org/p"), rdf_term_literal("o", NULL, NULL), 0};

    // 删除不存在的三元组
    int ret = rdf_engine_delete(db, &triple);
    EXPECT_NE(ret, 0);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, DeleteWithNullHandle) {
    rdf_triple_t triple = {rdf_term_uri("http://example.org/s"), rdf_term_uri("http://example.org/p"), rdf_term_literal("o", NULL, NULL), 0};
    int ret = rdf_engine_delete(NULL, &triple);
    EXPECT_NE(ret, 0);
}

/* ========================================================================
 * 统计信息测试
 * ======================================================================== */

TEST_F(RdfEngineTest, GetStats) {
    const char *graph_name = "test_stats";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 插入一些三元组
    for (int i = 0; i < 3; i++) {
        char s[128], p[128], o[32];
        snprintf(s, sizeof(s), "http://example.org/s%d", i);
        snprintf(p, sizeof(p), "http://example.org/p%d", i);
        snprintf(o, sizeof(o), "obj%d", i);
        rdf_triple_t t = {rdf_term_uri(s), rdf_term_uri(p), rdf_term_literal(o, NULL, NULL), 0};
        ops->tuple_insert(db, &t, sizeof(t));
    }

    ops->table_close(db);

    // 获取统计信息
    storage_stats_t stats;
    int ret = ops->get_stats(graph_name, &stats);
    EXPECT_EQ(ret, 0);

    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, GetStatsNullName) {
    const storage_ops_t *ops = rdf_engine_get_ops();
    storage_stats_t stats;
    int ret = ops->get_stats(NULL, &stats);
    EXPECT_NE(ret, 0);
}

/* ========================================================================
 * SPARQL SELECT 查询测试
 * ======================================================================== */

TEST_F(RdfEngineTest, SparqlSelectBasic) {
    const char *graph_name = "test_sparql_select";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 插入测试数据
    rdf_triple_t t1 = {rdf_term_uri("http://example.org/person1"), rdf_term_uri("http://example.org/name"), rdf_term_literal("Alice", "en", NULL), 0};
    rdf_triple_t t2 = {rdf_term_uri("http://example.org/person2"), rdf_term_uri("http://example.org/name"), rdf_term_literal("Bob", "en", NULL), 0};
    ops->tuple_insert(db, &t1, sizeof(t1));
    ops->tuple_insert(db, &t2, sizeof(t2));

    // 执行 SPARQL SELECT 查询
    sparql_result_t result;
    const char *query = "SELECT ?s ?p ?o WHERE { ?s ?p ?o }";
    int ret = rdf_engine_sparql_select(db, query, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.row_count, 2);

    rdf_engine_free_result(&result);
    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, SparqlSelectWithFilter) {
    const char *graph_name = "test_sparql_filter";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 插入测试数据
    rdf_triple_t t1 = {rdf_term_uri("http://example.org/person1"), rdf_term_uri("http://example.org/age"), rdf_term_literal("25", NULL, NULL), 0};
    rdf_triple_t t2 = {rdf_term_uri("http://example.org/person2"), rdf_term_uri("http://example.org/age"), rdf_term_literal("35", NULL, NULL), 0};
    ops->tuple_insert(db, &t1, sizeof(t1));
    ops->tuple_insert(db, &t2, sizeof(t2));

    // 执行 SPARQL SELECT with LIMIT
    sparql_result_t result;
    const char *query = "SELECT ?s ?p ?o WHERE { ?s ?p ?o } LIMIT 1";
    int ret = rdf_engine_sparql_select(db, query, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_LE(result.row_count, 1);

    rdf_engine_free_result(&result);
    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, SparqlSelectInvalidQuery) {
    const char *graph_name = "test_sparql_invalid";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    sparql_result_t result;
    int ret = rdf_engine_sparql_select(db, "INVALID QUERY", &result);
    EXPECT_NE(ret, 0);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, SparqlSelectNullParams) {
    sparql_result_t result;
    int ret = rdf_engine_sparql_select(NULL, "SELECT ?x WHERE { ?x ?y ?z }", &result);
    EXPECT_NE(ret, 0);

    const storage_ops_t *ops = rdf_engine_get_ops();
    ops->table_create("test_null", nullptr);
    void *db = ops->table_open("test_null", ACCESS_MODE_READ_WRITE);
    ret = rdf_engine_sparql_select(db, NULL, &result);
    EXPECT_NE(ret, 0);
    ops->table_close(db);
    ops->table_drop("test_null");
}

/* ========================================================================
 * SPARQL ASK 查询测试
 * ======================================================================== */

TEST_F(RdfEngineTest, SparqlAskTrue) {
    const char *graph_name = "test_sparql_ask";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 插入测试数据
    rdf_triple_t t1 = {rdf_term_uri("http://example.org/person1"), rdf_term_uri("http://example.org/name"), rdf_term_literal("Alice", "en", NULL), 0};
    ops->tuple_insert(db, &t1, sizeof(t1));

    // ASK 查询存在
    const char *query = "SELECT ?s ?p ?o WHERE { <http://example.org/person1> <http://example.org/name> ?o }";
    bool exists = rdf_engine_sparql_ask(db, query);
    EXPECT_TRUE(exists);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, SparqlAskFalse) {
    const char *graph_name = "test_sparql_ask_false";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    // 不插入任何数据

    // ASK 查询不存在
    const char *query = "SELECT ?s ?p ?o WHERE { <http://example.org/nonexistent> ?p ?o }";
    bool exists = rdf_engine_sparql_ask(db, query);
    EXPECT_FALSE(exists);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

TEST_F(RdfEngineTest, SparqlAskNullParams) {
    bool exists = rdf_engine_sparql_ask(NULL, "SELECT ?x WHERE { ?x ?y ?z }");
    EXPECT_FALSE(exists);
}

/* ========================================================================
 * 术语创建和比较测试
 * ======================================================================== */

TEST_F(RdfEngineTest, TermUri) {
    rdf_term_t term = rdf_term_uri("http://example.org/resource");
    EXPECT_EQ(term.type, RDF_URI);
    EXPECT_STREQ(term.value, "http://example.org/resource");
    EXPECT_STREQ(term.lang, "");
    EXPECT_STREQ(term.datatype, "");
}

TEST_F(RdfEngineTest, TermBlank) {
    rdf_term_t term = rdf_term_blank("node123");
    EXPECT_EQ(term.type, RDF_BLANK);
    EXPECT_STREQ(term.value, "node123");
}

TEST_F(RdfEngineTest, TermLiteral) {
    rdf_term_t term = rdf_term_literal("Hello", "en", "http://www.w3.org/2001/XMLSchema#string");
    EXPECT_EQ(term.type, RDF_LITERAL);
    EXPECT_STREQ(term.value, "Hello");
    EXPECT_STREQ(term.lang, "en");
    EXPECT_STREQ(term.datatype, "http://www.w3.org/2001/XMLSchema#string");
}

TEST_F(RdfEngineTest, TermLiteralWithoutLang) {
    rdf_term_t term = rdf_term_literal("42", NULL, "http://www.w3.org/2001/XMLSchema#integer");
    EXPECT_EQ(term.type, RDF_LITERAL);
    EXPECT_STREQ(term.value, "42");
    EXPECT_STREQ(term.lang, "");
    EXPECT_STREQ(term.datatype, "http://www.w3.org/2001/XMLSchema#integer");
}

TEST_F(RdfEngineTest, TermEquals) {
    rdf_term_t t1 = rdf_term_uri("http://example.org/res");
    rdf_term_t t2 = rdf_term_uri("http://example.org/res");
    rdf_term_t t3 = rdf_term_uri("http://example.org/other");

    EXPECT_TRUE(rdf_term_equals(&t1, &t2));
    EXPECT_FALSE(rdf_term_equals(&t1, &t3));
}

TEST_F(RdfEngineTest, TermEqualsLiteral) {
    rdf_term_t t1 = rdf_term_literal("value", "en", NULL);
    rdf_term_t t2 = rdf_term_literal("value", "en", NULL);
    rdf_term_t t3 = rdf_term_literal("value", "fr", NULL);

    EXPECT_TRUE(rdf_term_equals(&t1, &t2));
    EXPECT_FALSE(rdf_term_equals(&t1, &t3));
}

TEST_F(RdfEngineTest, TermEqualsDifferentTypes) {
    rdf_term_t t1 = rdf_term_uri("http://example.org/res");
    rdf_term_t t2 = rdf_term_blank("res");

    EXPECT_FALSE(rdf_term_equals(&t1, &t2));
}

TEST_F(RdfEngineTest, TermMatches) {
    rdf_term_t term = rdf_term_uri("http://example.org/value");
    rdf_term_t wildcard;
    memset(&wildcard, 0, sizeof(wildcard));  // 全零表示通配符

    // 通配符匹配
    EXPECT_TRUE(rdf_term_matches(&term, NULL));
    EXPECT_TRUE(rdf_term_matches(&term, &wildcard));

    // 精确匹配
    rdf_term_t pattern = rdf_term_uri("http://example.org/value");
    EXPECT_TRUE(rdf_term_matches(&term, &pattern));

    // 不匹配
    rdf_term_t no_match = rdf_term_uri("http://example.org/other");
    EXPECT_FALSE(rdf_term_matches(&term, &no_match));
}

TEST_F(RdfEngineTest, TermMatchesEmptyPattern) {
    rdf_term_t term = rdf_term_uri("http://example.org/any");
    rdf_term_t empty_pattern = rdf_term_uri("");  // 空字符串应该匹配任意

    // 空值表示通配符
    EXPECT_TRUE(rdf_term_matches(&term, &empty_pattern));
}

/* ========================================================================
 * 结果释放测试
 * ======================================================================== */

TEST_F(RdfEngineTest, FreeResultNull) {
    // 释放 NULL 结果应该安全
    rdf_engine_free_result(NULL);
    SUCCEED();
}

TEST_F(RdfEngineTest, FreeResultTwice) {
    const char *graph_name = "test_free_twice";
    const storage_ops_t *ops = rdf_engine_get_ops();

    ops->table_create(graph_name, nullptr);
    void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
    ASSERT_NE(db, nullptr);

    sparql_result_t result;
    const char *query = "SELECT ?s ?p ?o WHERE { ?s ?p ?o }";
    int ret = rdf_engine_sparql_select(db, query, &result);
    EXPECT_EQ(ret, 0);

    // 第一次释放
    rdf_engine_free_result(&result);

    // 第二次释放应该安全
    rdf_engine_free_result(&result);

    ops->table_close(db);
    ops->table_drop(graph_name);
}

/* ========================================================================
 * 持久化测试
 * ======================================================================== */

TEST_F(RdfEngineTest, Persistence) {
    const char *graph_name = "test_persist";

    {
        const storage_ops_t *ops = rdf_engine_get_ops();

        // 创建并插入数据
        ops->table_create(graph_name, nullptr);
        void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
        ASSERT_NE(db, nullptr);

        rdf_triple_t triple = {rdf_term_uri("http://example.org/s"), rdf_term_uri("http://example.org/p"), rdf_term_literal("o", NULL, NULL), 0};
        ops->tuple_insert(db, &triple, sizeof(triple));

        ops->table_close(db);
    }

    {
        const storage_ops_t *ops = rdf_engine_get_ops();

        // 重新打开并验证数据
        void *db = ops->table_open(graph_name, ACCESS_MODE_READ_WRITE);
        ASSERT_NE(db, nullptr);

        rdf_triple_t results[10];
        int32_t num_results = 0;
        rdf_engine_match(db, NULL, NULL, NULL, results, 10, &num_results);
        EXPECT_EQ(num_results, 1);

        ops->table_close(db);
        ops->table_drop(graph_name);
    }
}

/* ========================================================================
 * 索引初始化测试
 * ======================================================================== */

TEST_F(RdfEngineTest, IndexInitShutdown) {
    // 索引已在 SetUp 中初始化
    SUCCEED();
}

TEST_F(RdfEngineTest, IndexDoubleInit) {
    // 重复初始化应该安全
    int ret = rdf_index_init();
    EXPECT_EQ(ret, 0);
    rdf_index_shutdown();
    rdf_index_init();
}
