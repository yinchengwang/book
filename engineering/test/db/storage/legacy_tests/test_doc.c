/**
 * @file test_doc.c
 * @brief 文档存储模态追赶测试
 *
 * 测试 doc_fts, doc_agg, doc_highlight, doc_inverted 模块
 */
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* 头文件 */
#include "db/storage/doc/doc_fts.h"
#include "db/storage/doc/doc_agg.h"
#include "db/storage/doc/doc_inverted.h"

/* doc_highlight_unified 没有头文件声明，手动声明 */
#ifdef __cplusplus
extern "C" {
#endif
char *doc_highlight_unified(const char *text, size_t text_len,
                            const char *const *terms, size_t n_terms,
                            size_t context_chars);
#ifdef __cplusplus
}
#endif

/* ========================================================================
 * doc_fts 分词器测试
 * ======================================================================== */

class DocFtsTokenizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tokenizer = doc_tokenizer_create(DOC_TOKENIZER_STANDARD, NULL);
    }

    void TearDown() override {
        doc_tokenizer_destroy(tokenizer);
    }

    DocTokenizer *tokenizer;
};

TEST_F(DocFtsTokenizerTest, CreateDestroy) {
    ASSERT_NE(tokenizer, nullptr);
    EXPECT_EQ(tokenizer->type, DOC_TOKENIZER_STANDARD);
}

TEST_F(DocFtsTokenizerTest, TokenizeSimple) {
    DocToken *tokens = NULL;
    size_t count = 0;

    const char *text = "hello world";
    int result = doc_tokenizer_tokenize(tokenizer, text, strlen(text), &tokens, &count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 2u);

    if (count >= 2) {
        EXPECT_STREQ(tokens[0].text, "hello");
        EXPECT_STREQ(tokens[1].text, "world");
    }

    doc_tokenizer_free_tokens(tokens, count);
}

TEST_F(DocFtsTokenizerTest, TokenizeWithPunctuation) {
    DocToken *tokens = NULL;
    size_t count = 0;

    const char *text = "hello, world!";
    int result = doc_tokenizer_tokenize(tokenizer, text, strlen(text), &tokens, &count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 2u);

    if (count >= 2) {
        EXPECT_STREQ(tokens[0].text, "hello");
        EXPECT_STREQ(tokens[1].text, "world");
    }

    doc_tokenizer_free_tokens(tokens, count);
}

TEST_F(DocFtsTokenizerTest, TypeName) {
    EXPECT_STREQ(doc_tokenizer_type_name(DOC_TOKENIZER_STANDARD), "standard");
    EXPECT_STREQ(doc_tokenizer_type_name(DOC_TOKENIZER_WHITESPACE), "whitespace");
    EXPECT_STREQ(doc_tokenizer_type_name(DOC_TOKENIZER_KEYWORD), "keyword");
}

TEST_F(DocFtsTokenizerTest, WhitespaceTokenizer) {
    doc_tokenizer_destroy(tokenizer);
    tokenizer = doc_tokenizer_create(DOC_TOKENIZER_WHITESPACE, NULL);
    ASSERT_NE(tokenizer, nullptr);

    DocToken *tokens = NULL;
    size_t count = 0;

    const char *text = "hello world test";
    int result = doc_tokenizer_tokenize(tokenizer, text, strlen(text), &tokens, &count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 3u);

    doc_tokenizer_free_tokens(tokens, count);
}

TEST_F(DocFtsTokenizerTest, KeywordTokenizer) {
    doc_tokenizer_destroy(tokenizer);
    tokenizer = doc_tokenizer_create(DOC_TOKENIZER_KEYWORD, NULL);
    ASSERT_NE(tokenizer, nullptr);

    DocToken *tokens = NULL;
    size_t count = 0;

    const char *text = "hello world";
    int result = doc_tokenizer_tokenize(tokenizer, text, strlen(text), &tokens, &count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 1u);

    if (count == 1) {
        EXPECT_STREQ(tokens[0].text, "hello world");
    }

    doc_tokenizer_free_tokens(tokens, count);
}

/* ========================================================================
 * doc_fts 停用词测试
 * ======================================================================== */

class DocFtsStopwordsTest : public ::testing::Test {
protected:
    void SetUp() override {
        stopwords = doc_stopwords_create(false);
    }

    void TearDown() override {
        doc_stopwords_destroy(stopwords);
    }

    DocStopwords *stopwords;
};

TEST_F(DocFtsStopwordsTest, CreateDestroy) {
    ASSERT_NE(stopwords, nullptr);
    EXPECT_EQ(stopwords->num_words, 0u);
}

TEST_F(DocFtsStopwordsTest, AddAndContains) {
    doc_stopwords_add(stopwords, "the");
    doc_stopwords_add(stopwords, "a");
    doc_stopwords_add(stopwords, "is");

    EXPECT_TRUE(doc_stopwords_contains(stopwords, "the"));
    EXPECT_TRUE(doc_stopwords_contains(stopwords, "A"));  // case insensitive
    EXPECT_FALSE(doc_stopwords_contains(stopwords, "hello"));
}

TEST_F(DocFtsStopwordsTest, CaseSensitive) {
    doc_stopwords_destroy(stopwords);
    stopwords = doc_stopwords_create(true);

    doc_stopwords_add(stopwords, "The");

    EXPECT_TRUE(doc_stopwords_contains(stopwords, "The"));
    EXPECT_FALSE(doc_stopwords_contains(stopwords, "the"));
}

/* ========================================================================
 * doc_fts 同义词测试
 * ======================================================================== */

class DocFtsSynonymsTest : public ::testing::Test {
protected:
    void SetUp() override {
        synonyms = doc_synonyms_create();
    }

    void TearDown() override {
        doc_synonyms_destroy(synonyms);
    }

    DocSynonyms *synonyms;
};

TEST_F(DocFtsSynonymsTest, CreateDestroy) {
    ASSERT_NE(synonyms, nullptr);
    EXPECT_EQ(synonyms->num_groups, 0u);
}

TEST_F(DocFtsSynonymsTest, AddAndGet) {
    char *terms[] = {(char*)"happy", (char*)"joyful", (char*)"cheerful"};
    doc_synonyms_add_group(synonyms, terms, 3);

    char **out_terms = NULL;
    size_t count = doc_synonyms_get(synonyms, "happy", &out_terms);

    EXPECT_EQ(count, 3u);
    if (count > 0) {
        EXPECT_STREQ(out_terms[0], "happy");
    }

    // free out_terms
    for (size_t i = 0; i < count; i++) {
        free(out_terms[i]);
    }
    free(out_terms);
}

TEST_F(DocFtsSynonymsTest, GetNotFound) {
    char *terms[] = {(char*)"happy", (char*)"joyful"};
    doc_synonyms_add_group(synonyms, terms, 2);

    char **out_terms = NULL;
    size_t count = doc_synonyms_get(synonyms, "sad", &out_terms);

    EXPECT_EQ(count, 0u);
}

/* ========================================================================
 * doc_fts 短语搜索测试
 * ======================================================================== */

class DocFtsPhraseTest : public ::testing::Test {
protected:
    void SetUp() override {
        tokenizer = doc_tokenizer_create(DOC_TOKENIZER_STANDARD, NULL);
    }

    void TearDown() override {
        doc_tokenizer_destroy(tokenizer);
    }

    DocTokenizer *tokenizer;
};

TEST_F(DocFtsPhraseTest, PhraseSearch) {
    const char *documents[] = {
        "the quick brown fox",
        "the lazy dog",
        "quick brown fox jumps"
    };
    size_t num_docs = 3;

    DocPhraseResult *results = NULL;
    size_t count = doc_phrase_search(documents, num_docs, "quick brown",
                                     tokenizer, &results, 10);

    EXPECT_GT(count, 0u);
    doc_phrase_result_free(results, count);
}

TEST_F(DocFtsPhraseTest, PhraseSearchNoMatch) {
    const char *documents[] = {
        "hello world",
        "foo bar"
    };
    size_t num_docs = 2;

    DocPhraseResult *results = NULL;
    size_t count = doc_phrase_search(documents, num_docs, "nonexistent",
                                     tokenizer, &results, 10);

    EXPECT_EQ(count, 0u);
    doc_phrase_result_free(results, count);
}

/* ========================================================================
 * doc_fts 高亮测试
 * ======================================================================== */

class DocFtsHighlightTest : public ::testing::Test {
protected:
    void SetUp() override {
        tokenizer = doc_tokenizer_create(DOC_TOKENIZER_STANDARD, NULL);
    }

    void TearDown() override {
        doc_tokenizer_destroy(tokenizer);
    }

    DocTokenizer *tokenizer;
};

TEST_F(DocFtsHighlightTest, Highlight) {
    DocHighlightFragment *fragments = NULL;
    size_t count = doc_highlight("the quick brown fox jumps over the lazy dog",
                                 "brown", tokenizer, &DOC_HIGHLIGHT_DEFAULT,
                                 1, 256, &fragments);

    EXPECT_GT(count, 0u);
    if (count > 0) {
        EXPECT_NE(fragments[0].text, nullptr);
        EXPECT_GT(fragments[0].num_matches, 0);
    }

    doc_highlight_free(fragments, count);
}

TEST_F(DocFtsHighlightTest, HighlightNoMatch) {
    DocHighlightFragment *fragments = NULL;
    size_t count = doc_highlight("hello world", "nonexistent",
                                 tokenizer, &DOC_HIGHLIGHT_DEFAULT,
                                 1, 256, &fragments);

    EXPECT_EQ(count, 0u);
    doc_highlight_free(fragments, count);
}

/* ========================================================================
 * doc_fts FTS Searcher 测试
 * ======================================================================== */

class DocFtsSearcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        searcher = doc_fts_create(NULL);
    }

    void TearDown() override {
        doc_fts_destroy(searcher);
    }

    DocFtsSearcher *searcher;
};

TEST_F(DocFtsSearcherTest, CreateDestroy) {
    ASSERT_NE(searcher, nullptr);
    EXPECT_NE(searcher->tokenizer, nullptr);
}

TEST_F(DocFtsSearcherTest, Search) {
    const char *documents[] = {
        "the quick brown fox",
        "the lazy dog",
        "quick brown fox jumps"
    };
    size_t num_docs = 3;

    DocPhraseResult *results = NULL;
    size_t count = doc_fts_search(searcher, "quick", documents, num_docs,
                                  &results, 10);

    EXPECT_GT(count, 0u);
    doc_phrase_result_free(results, count);
}

/* ========================================================================
 * doc_fts SQL 函数测试
 * ======================================================================== */

class DocFtsSqlTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 无特殊初始化 */
    }

    void TearDown() override {
        /* 无特殊清理 */
    }
};

TEST_F(DocFtsSqlTest, Match) {
    EXPECT_TRUE(doc_sql_match("the quick brown fox", "brown"));
    EXPECT_TRUE(doc_sql_match("hello world", "hello"));
    EXPECT_FALSE(doc_sql_match("hello world", "nonexistent"));
}

TEST_F(DocFtsSqlTest, Highlight) {
    char *result = doc_sql_highlight("the quick brown fox", "brown");
    ASSERT_NE(result, nullptr);
    /* 高亮结果应包含 <em> 标记 */
    EXPECT_NE(strstr(result, "<em>"), nullptr);
    free(result);
}

TEST_F(DocFtsSqlTest, HighlightNull) {
    char *result = doc_sql_highlight(NULL, "query");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "");
    free(result);
}

/* ========================================================================
 * doc_agg 词条聚合测试
 * ======================================================================== */

class DocAggTermTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 无特殊初始化 */
    }

    void TearDown() override {
        /* 无特殊清理 */
    }
};

TEST_F(DocAggTermTest, TermAgg) {
    void *agg = doc_term_agg_create("category", 10, 0);
    ASSERT_NE(agg, nullptr);

    const char *doc_ids[] = {"1", "2", "3", "4", "5"};
    const char *doc_values[] = {"A", "B", "A", "C", "A"};

    DocTermAggResult *result = doc_term_agg_execute(agg, doc_ids, doc_values, 5);
    ASSERT_NE(result, nullptr);

    EXPECT_GT(result->num_buckets, 0u);
    /* A 出现 3 次，应该是最多的 */
    EXPECT_STREQ(result->buckets[0].term, "A");
    EXPECT_EQ(result->buckets[0].doc_count, 3);

    doc_term_agg_free(result);
    free(agg);
}

/* ========================================================================
 * doc_agg 范围聚合测试
 * ======================================================================== */

class DocAggRangeTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 无特殊初始化 */
    }

    void TearDown() override {
        /* 无特殊清理 */
    }
};

TEST_F(DocAggRangeTest, RangeAgg) {
    DocRange ranges[] = {
        {0.0, 10.0, (char*)"low"},
        {10.0, 20.0, (char*)"mid"},
        {20.0, 30.0, (char*)"high"}
    };

    void *agg = doc_range_agg_create("price", ranges, 3);
    ASSERT_NE(agg, nullptr);

    const char *doc_ids[] = {"1", "2", "3", "4"};
    double values[] = {5.0, 15.0, 25.0, 12.0};

    DocRangeAggResult *result = doc_range_agg_execute(agg, doc_ids, values, 4);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->num_buckets, 3u);
    EXPECT_EQ(result->buckets[0].doc_count, 1);  /* low: 5.0 */
    EXPECT_EQ(result->buckets[1].doc_count, 2);  /* mid: 15.0, 12.0 */
    EXPECT_EQ(result->buckets[2].doc_count, 1);  /* high: 25.0 */

    doc_range_agg_free(result);
    free(agg);
}

/* ========================================================================
 * doc_agg 直方图聚合测试
 * ======================================================================== */

class DocAggHistogramTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 无特殊初始化 */
    }

    void TearDown() override {
        /* 无特殊清理 */
    }
};

TEST_F(DocAggHistogramTest, HistogramAgg) {
    void *agg = doc_histogram_agg_create("price", 10.0, NULL);
    ASSERT_NE(agg, nullptr);

    const char *doc_ids[] = {"1", "2", "3", "4", "5"};
    double values[] = {1.0, 5.0, 11.0, 15.0, 21.0};

    DocHistogramAggResult *result = doc_histogram_agg_execute(agg, doc_ids, values, 5);
    ASSERT_NE(result, nullptr);

    EXPECT_GT(result->num_buckets, 0u);

    doc_histogram_agg_free(result);
    free(agg);
}

/* ========================================================================
 * doc_agg 统计聚合测试
 * ======================================================================== */

class DocAggStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 无特殊初始化 */
    }

    void TearDown() override {
        /* 无特殊清理 */
    }
};

TEST_F(DocAggStatsTest, StatsAgg) {
    void *agg = doc_stats_agg_create("price", true);
    ASSERT_NE(agg, nullptr);

    double values[] = {1.0, 2.0, 3.0, 4.0, 5.0};

    DocStatsAggResult *result = doc_stats_agg_execute(agg, values, 5);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->stats.count, 5);
    EXPECT_DOUBLE_EQ(result->stats.min, 1.0);
    EXPECT_DOUBLE_EQ(result->stats.max, 5.0);
    EXPECT_DOUBLE_EQ(result->stats.sum, 15.0);
    EXPECT_DOUBLE_EQ(result->stats.avg, 3.0);
    EXPECT_TRUE(result->has_percentiles);

    doc_stats_agg_free(result);
    free(agg);
}

/* ========================================================================
 * doc_agg 百分位数聚合测试
 * ======================================================================== */

class DocAggPercentilesTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 无特殊初始化 */
    }

    void TearDown() override {
        /* 无特殊清理 */
    }
};

TEST_F(DocAggPercentilesTest, PercentilesAgg) {
    void *agg = doc_percentiles_agg_create("price", NULL);
    ASSERT_NE(agg, nullptr);

    double values[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};

    DocPercentilesAggResult *result = doc_percentiles_agg_execute(agg, values, 10);
    ASSERT_NE(result, nullptr);

    EXPECT_GT(result->num_buckets, 0u);

    doc_percentiles_agg_free(result);
    free(agg);
}

/* ========================================================================
 * doc_agg 基数聚合测试
 * ======================================================================== */

class DocAggCardinalityTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 无特殊初始化 */
    }

    void TearDown() override {
        /* 无特殊清理 */
    }
};

TEST_F(DocAggCardinalityTest, CardinalityAgg) {
    void *agg = doc_cardinality_agg_create("category", 1000);
    ASSERT_NE(agg, nullptr);

    const char *values[] = {"A", "B", "A", "C", "B"};
    uint64_t result = doc_cardinality_agg_execute(agg, values, 5);

    /* 应该识别出 3 个唯一值 */
    EXPECT_EQ(result, 3u);

    free(agg);
}

/* ========================================================================
 * doc_agg 管道聚合测试
 * ======================================================================== */

class DocAggPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 无特殊初始化 */
    }

    void TearDown() override {
        /* 无特殊清理 */
    }
};

TEST_F(DocAggPipelineTest, PipelineAggCreate) {
    DocPipelineAggConfig config;
    memset(&config, 0, sizeof(config));
    config.type = DOC_PIPELINE_AVG;
    snprintf(config.parent_agg, sizeof(config.parent_agg), "my_agg");

    DocPipelineAgg *agg = doc_pipeline_agg_create("pipeline1", &config);
    ASSERT_NE(agg, nullptr);

    EXPECT_STREQ(agg->name, "pipeline1");
    EXPECT_EQ(agg->config.type, DOC_PIPELINE_AVG);

    doc_pipeline_agg_destroy(agg);
}

TEST_F(DocAggPipelineTest, PipelineAggExecute) {
    DocPipelineAggConfig config;
    memset(&config, 0, sizeof(config));
    config.type = DOC_PIPELINE_AVG;

    DocPipelineAgg *agg = doc_pipeline_agg_create("avg_pipeline", &config);
    ASSERT_NE(agg, nullptr);

    int parent_result = 0;  /* 占位 */
    char *result = doc_pipeline_agg_execute(agg, &parent_result);
    ASSERT_NE(result, nullptr);
    EXPECT_NE(strstr(result, "avg"), nullptr);

    free(result);
    doc_pipeline_agg_destroy(agg);
}

/* ========================================================================
 * doc_agg SQL 函数测试
 * ======================================================================== */

class DocAggSqlTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 无特殊初始化 */
    }

    void TearDown() override {
        /* 无特殊清理 */
    }
};

TEST_F(DocAggSqlTest, TermAggSql) {
    const char *doc_ids[] = {"1", "2", "3"};
    const char *doc_values[] = {"A", "B", "A"};

    char *result = doc_sql_term_agg("category", 10, doc_ids, doc_values, 3);
    ASSERT_NE(result, nullptr);
    EXPECT_NE(strstr(result, "buckets"), nullptr);

    free(result);
}

TEST_F(DocAggSqlTest, StatsAggSql) {
    double values[] = {1.0, 2.0, 3.0};

    char *result = doc_sql_stats_agg("price", values, 3);
    ASSERT_NE(result, nullptr);
    EXPECT_NE(strstr(result, "count"), nullptr);

    free(result);
}

/* ========================================================================
 * doc_highlight 统一高亮器测试
 * ======================================================================== */

class DocHighlightUnifiedTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 无特殊初始化 */
    }

    void TearDown() override {
        /* 无特殊清理 */
    }
};

TEST_F(DocHighlightUnifiedTest, UnifiedHighlight) {
    const char *text = "the quick brown fox jumps over the lazy dog";
    const char *terms[] = {"brown"};

    char *result = doc_highlight_unified(text, strlen(text), terms, 1, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_NE(strstr(result, "brown"), nullptr);

    free(result);
}

TEST_F(DocHighlightUnifiedTest, UnifiedHighlightNotFound) {
    const char *text = "hello world";
    const char *terms[] = {"nonexistent"};

    char *result = doc_highlight_unified(text, strlen(text), terms, 1, 10);
    /* 未找到 term 时应返回前 context_chars 个字符 */
    ASSERT_NE(result, nullptr);

    free(result);
}

/* ========================================================================
 * main
 * ======================================================================== */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
