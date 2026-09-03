/**
 * @file index_manager_test.cpp
 * @brief Unit tests for IndexCatalog, IndexManager, IndexCost, and IndexSelector
 */

#include <gtest/gtest.h>
#include "db/index/index_catalog.h"
#include "db/index/index_manager.h"
#include "db/index/index_cost.h"
#include "db/index/index_selector.h"
#include "db/index/index_config.h"

#include <cstring>

// ============================================================
// IndexCatalog Tests
// ============================================================

class IndexCatalogTest : public ::testing::Test {
protected:
    void SetUp() override {
        catalog = index_catalog_create(16);
    }

    void TearDown() override {
        if (catalog) {
            index_catalog_destroy(catalog);
            catalog = nullptr;
        }
    }

    index_catalog_t *catalog = nullptr;
};

TEST_F(IndexCatalogTest, CreateAndDestroy)
{
    index_catalog_t *cat = index_catalog_create(16);
    ASSERT_NE(cat, nullptr);
    EXPECT_EQ(cat->count, 0);
    EXPECT_GE(cat->capacity, 16);
    index_catalog_destroy(cat);
}

TEST_F(IndexCatalogTest, CreateWithInvalidCapacity)
{
    /* Should use default capacity of 16 for invalid values */
    index_catalog_t *cat = index_catalog_create(0);
    ASSERT_NE(cat, nullptr);
    EXPECT_GE(cat->capacity, 16);
    index_catalog_destroy(cat);

    cat = index_catalog_create(-1);
    ASSERT_NE(cat, nullptr);
    EXPECT_GE(cat->capacity, 16);
    index_catalog_destroy(cat);
}

TEST_F(IndexCatalogTest, AddAndGet)
{
    index_entry_t *entry = (index_entry_t *)calloc(1, sizeof(index_entry_t));
    ASSERT_NE(entry, nullptr);
    entry->index_id = 1;
    entry->type = INDEX_TYPE_BTREE;
    entry->table_id = 100;
    entry->column_count = 1;
    entry->columns = (int *)malloc(sizeof(int));
    entry->columns[0] = 5;

    EXPECT_EQ(index_catalog_add(catalog, entry), 0);
    EXPECT_EQ(catalog->count, 1);

    index_entry_t *found = index_catalog_get(catalog, 1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->index_id, 1);
    EXPECT_EQ(found->type, INDEX_TYPE_BTREE);
    EXPECT_EQ(found->table_id, 100);
    EXPECT_EQ(found->columns[0], 5);

    /* Catalog owns the entry now, don't free entry separately */
}

TEST_F(IndexCatalogTest, AddDuplicate)
{
    index_entry_t *entry1 = (index_entry_t *)calloc(1, sizeof(index_entry_t));
    ASSERT_NE(entry1, nullptr);
    entry1->index_id = 1;
    entry1->type = INDEX_TYPE_BTREE;
    entry1->table_id = 100;
    entry1->column_count = 1;
    entry1->columns = (int *)malloc(sizeof(int));
    entry1->columns[0] = 5;

    index_entry_t *entry2 = (index_entry_t *)calloc(1, sizeof(index_entry_t));
    ASSERT_NE(entry2, nullptr);
    entry2->index_id = 1;  /* Same ID */
    entry2->type = INDEX_TYPE_HASH;
    entry2->table_id = 200;

    EXPECT_EQ(index_catalog_add(catalog, entry1), 0);
    EXPECT_EQ(index_catalog_add(catalog, entry2), -1);  /* Duplicate should fail */
    EXPECT_EQ(catalog->count, 1);

    /* entry1 is owned by catalog now, entry2 was not added so free it */
    free(entry2);
}

TEST_F(IndexCatalogTest, Remove)
{
    index_entry_t *entry = (index_entry_t *)calloc(1, sizeof(index_entry_t));
    ASSERT_NE(entry, nullptr);
    entry->index_id = 1;
    entry->type = INDEX_TYPE_BTREE;
    entry->table_id = 100;
    entry->column_count = 1;
    entry->columns = (int *)malloc(sizeof(int));
    entry->columns[0] = 5;

    EXPECT_EQ(index_catalog_add(catalog, entry), 0);
    EXPECT_EQ(catalog->count, 1);

    EXPECT_EQ(index_catalog_remove(catalog, 1), 0);
    EXPECT_EQ(catalog->count, 0);

    EXPECT_EQ(index_catalog_get(catalog, 1), nullptr);

    /* entry is owned by catalog, don't free here */
}

TEST_F(IndexCatalogTest, RemoveNonExistent)
{
    EXPECT_EQ(index_catalog_remove(catalog, 999), -1);
}

TEST_F(IndexCatalogTest, GetByTable)
{
    index_entry_t *entries[3];
    for (int i = 0; i < 3; i++) {
        entries[i] = (index_entry_t *)calloc(1, sizeof(index_entry_t));
        ASSERT_NE(entries[i], nullptr);
        entries[i]->index_id = i + 1;
        entries[i]->type = INDEX_TYPE_BTREE;
        entries[i]->table_id = (i == 0) ? 100 : 200;
        entries[i]->column_count = 1;
        entries[i]->columns = (int *)malloc(sizeof(int));
        entries[i]->columns[0] = i;
        index_catalog_add(catalog, entries[i]);
    }

    index_entry_t *results[10];
    int count = index_catalog_get_by_table(catalog, 100, results, 10);
    EXPECT_EQ(count, 1);
    if (count > 0) {
        EXPECT_EQ(results[0]->index_id, 1);
    }

    count = index_catalog_get_by_table(catalog, 200, results, 10);
    EXPECT_EQ(count, 2);

    count = index_catalog_get_by_table(catalog, 999, results, 10);
    EXPECT_EQ(count, 0);

    /* entries are owned by catalog, don't free them here */
}

TEST_F(IndexCatalogTest, TypeConversion)
{
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_BTREE), "BTREE");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_HASH), "HASH");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_HNSW), "HNSW");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_IVF), "IVF");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_FULLTEXT), "FULLTEXT");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_GIN), "GIN");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_COUNT), "UNKNOWN");
    EXPECT_STREQ(index_type_to_string((index_type_t)100), "UNKNOWN");

    EXPECT_EQ(index_type_from_string("BTREE"), INDEX_TYPE_BTREE);
    EXPECT_EQ(index_type_from_string("HASH"), INDEX_TYPE_HASH);
    EXPECT_EQ(index_type_from_string("HNSW"), INDEX_TYPE_HNSW);
    EXPECT_EQ(index_type_from_string("IVF"), INDEX_TYPE_IVF);
    EXPECT_EQ(index_type_from_string("FULLTEXT"), INDEX_TYPE_FULLTEXT);
    EXPECT_EQ(index_type_from_string("GIN"), INDEX_TYPE_GIN);
    EXPECT_EQ(index_type_from_string("UNKNOWN"), INDEX_TYPE_COUNT);
    EXPECT_EQ(index_type_from_string(nullptr), INDEX_TYPE_COUNT);
    EXPECT_EQ(index_type_from_string("INVALID"), INDEX_TYPE_COUNT);
}

// ============================================================
// IndexManager Tests
// ============================================================

class IndexManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = index_manager_create();
    }

    void TearDown() override {
        if (mgr) {
            index_manager_destroy(mgr);
            mgr = nullptr;
        }
    }

    index_manager_t *mgr = nullptr;
};

TEST_F(IndexManagerTest, CreateAndDestroy)
{
    index_manager_t *m = index_manager_create();
    ASSERT_NE(m, nullptr);
    EXPECT_NE(m->catalog, nullptr);
    EXPECT_EQ(m->owns_catalog, true);
    index_manager_destroy(m);
}

TEST_F(IndexManagerTest, CreateIndex)
{
    int columns[] = {1, 2};
    index_config_t config = index_config_default();

    int result = index_manager_create_index(mgr, "test_idx", INDEX_TYPE_BTREE,
                                            100, columns, 2, &config);
    EXPECT_EQ(result, 0);

    const index_entry_t *entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->name, "test_idx");
    EXPECT_EQ(entry->type, INDEX_TYPE_BTREE);
    EXPECT_EQ(entry->table_id, 100);
    EXPECT_EQ(entry->column_count, 2);
    EXPECT_EQ(entry->state, INDEX_STATE_READY);
}

TEST_F(IndexManagerTest, CreateIndexInvalidParams)
{
    int columns[] = {1};

    /* NULL manager */
    EXPECT_EQ(index_manager_create_index(nullptr, "idx", INDEX_TYPE_BTREE,
                                        100, columns, 1, nullptr), -1);

    /* NULL name */
    EXPECT_EQ(index_manager_create_index(mgr, nullptr, INDEX_TYPE_BTREE,
                                        100, columns, 1, nullptr), -1);

    /* Invalid column count */
    EXPECT_EQ(index_manager_create_index(mgr, "idx", INDEX_TYPE_BTREE,
                                        100, columns, 0, nullptr), -1);

    /* NULL columns */
    EXPECT_EQ(index_manager_create_index(mgr, "idx", INDEX_TYPE_BTREE,
                                        100, nullptr, 1, nullptr), -1);

    /* Invalid type */
    EXPECT_EQ(index_manager_create_index(mgr, "idx", INDEX_TYPE_COUNT,
                                        100, columns, 1, nullptr), -1);
}

TEST_F(IndexManagerTest, DropIndex)
{
    int columns[] = {1};
    index_config_t config = index_config_default();

    EXPECT_EQ(index_manager_create_index(mgr, "test_idx", INDEX_TYPE_BTREE,
                                        100, columns, 1, &config), 0);

    const index_entry_t *entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);

    EXPECT_EQ(index_manager_drop_index(mgr, 1), 0);

    EXPECT_EQ(index_manager_get_index(mgr, 1), nullptr);
}

TEST_F(IndexManagerTest, DropIndexNonExistent)
{
    EXPECT_EQ(index_manager_drop_index(mgr, 999), -1);
}

TEST_F(IndexManagerTest, GetTableIndexes)
{
    int cols1[] = {1};
    int cols2[] = {2};
    int cols3[] = {3};
    index_config_t config = index_config_default();

    EXPECT_EQ(index_manager_create_index(mgr, "idx1", INDEX_TYPE_BTREE, 100, cols1, 1, &config), 0);
    EXPECT_EQ(index_manager_create_index(mgr, "idx2", INDEX_TYPE_HASH, 100, cols2, 1, &config), 0);
    EXPECT_EQ(index_manager_create_index(mgr, "idx3", INDEX_TYPE_BTREE, 200, cols3, 1, &config), 0);

    index_entry_t *results[10];
    int count = index_manager_get_table_indexes(mgr, 100, results, 10);
    EXPECT_EQ(count, 2);

    count = index_manager_get_table_indexes(mgr, 200, results, 10);
    EXPECT_EQ(count, 1);

    count = index_manager_get_table_indexes(mgr, 999, results, 10);
    EXPECT_EQ(count, 0);
}

TEST_F(IndexManagerTest, RebuildIndex)
{
    int columns[] = {1};
    index_config_t config = index_config_default();

    EXPECT_EQ(index_manager_create_index(mgr, "test_idx", INDEX_TYPE_BTREE,
                                        100, columns, 1, &config), 0);

    const index_entry_t *entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, INDEX_STATE_READY);

    /* Rebuild should succeed */
    EXPECT_EQ(index_manager_rebuild_index(mgr, 1), 0);

    entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, INDEX_STATE_READY);
}

TEST_F(IndexManagerTest, GetCatalog)
{
    EXPECT_NE(index_manager_get_catalog(mgr), nullptr);
    EXPECT_EQ(index_manager_get_catalog(nullptr), nullptr);
}

// ============================================================
// IndexCost Tests
// ============================================================

class IndexCostTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Create an index entry for testing */
        memset(&entry, 0, sizeof(entry));
        entry.index_id = 1;
        entry.type = INDEX_TYPE_BTREE;
        entry.table_id = 100;
        entry.column_count = 1;
        entry.columns = (int *)malloc(sizeof(int));
        entry.columns[0] = 1;

        /* Create table stats */
        memset(&stats, 0, sizeof(stats));
        stats.row_count = 10000;
        stats.page_count = 100;
        stats.avg_row_width = 100.0;
        stats.distinct_values = 1000;

        /* Create query condition */
        memset(&cond, 0, sizeof(cond));
        cond.type = COND_EQ;
        cond.column_id = 1;
    }

    void TearDown() override {
        free(entry.columns);
    }

    index_entry_t entry;
    table_stats_t stats;
    query_condition_t cond;
};

TEST_F(IndexCostTest, CostEstimateBasic)
{
    const index_cost_t *cost = index_cost_estimate(&entry, &cond, &stats);
    ASSERT_NE(cost, nullptr);
    EXPECT_EQ(cost->index_id, 1);
    EXPECT_GT(cost->total_cost, 0.0);
    EXPECT_GT(cost->rows_estimated, 0);
}

TEST_F(IndexCostTest, CostEstimateNullParams)
{
    const index_cost_t *cost = index_cost_estimate(nullptr, &cond, &stats);
    ASSERT_NE(cost, nullptr);
    EXPECT_EQ(cost->total_cost, 0.0);

    cost = index_cost_estimate(&entry, nullptr, &stats);
    ASSERT_NE(cost, nullptr);
    EXPECT_EQ(cost->total_cost, 0.0);

    cost = index_cost_estimate(&entry, &cond, nullptr);
    ASSERT_NE(cost, nullptr);
    EXPECT_EQ(cost->total_cost, 0.0);
}

TEST_F(IndexCostTest, CostEstimateEmptyTable)
{
    table_stats_t empty_stats = stats;
    empty_stats.row_count = 0;

    const index_cost_t *cost = index_cost_estimate(&entry, &cond, &empty_stats);
    ASSERT_NE(cost, nullptr);
    EXPECT_EQ(cost->rows_estimated, 0);
}

TEST_F(IndexCostTest, SelectivityEquality)
{
    cond.type = COND_EQ;
    double sel = index_selectivity(&entry, &cond, &stats);
    EXPECT_DOUBLE_EQ(sel, 0.001);  /* 1/1000 */
}

TEST_F(IndexCostTest, SelectivityRange)
{
    cond.type = COND_RANGE;
    double sel = index_selectivity(&entry, &cond, &stats);
    EXPECT_DOUBLE_EQ(sel, 0.10);
}

TEST_F(IndexCostTest, SelectivityLessThan)
{
    cond.type = COND_LT;
    double sel = index_selectivity(&entry, &cond, &stats);
    EXPECT_DOUBLE_EQ(sel, 0.25);
}

TEST_F(IndexCostTest, SelectivityGreaterThan)
{
    cond.type = COND_GT;
    double sel = index_selectivity(&entry, &cond, &stats);
    EXPECT_DOUBLE_EQ(sel, 0.25);
}

TEST_F(IndexCostTest, SelectivityText)
{
    cond.type = COND_TEXT;
    double sel = index_selectivity(&entry, &cond, &stats);
    EXPECT_DOUBLE_EQ(sel, 0.01);
}

TEST_F(IndexCostTest, SelectivityColumnNotInIndex)
{
    cond.column_id = 999;  /* Column not in index */
    double sel = index_selectivity(&entry, &cond, &stats);
    EXPECT_DOUBLE_EQ(sel, 1.0);  /* Full scan */
}

TEST_F(IndexCostTest, SelectivityNullParams)
{
    double sel = index_selectivity(nullptr, &cond, &stats);
    EXPECT_DOUBLE_EQ(sel, 1.0);

    sel = index_selectivity(&entry, nullptr, &stats);
    EXPECT_DOUBLE_EQ(sel, 1.0);

    sel = index_selectivity(&entry, &cond, nullptr);
    EXPECT_DOUBLE_EQ(sel, 1.0);
}

TEST_F(IndexCostTest, ConditionString)
{
    EXPECT_STREQ(index_cost_condition_string(COND_EQ), "EQ");
    EXPECT_STREQ(index_cost_condition_string(COND_LT), "LT");
    EXPECT_STREQ(index_cost_condition_string(COND_LE), "LE");
    EXPECT_STREQ(index_cost_condition_string(COND_GT), "GT");
    EXPECT_STREQ(index_cost_condition_string(COND_GE), "GE");
    EXPECT_STREQ(index_cost_condition_string(COND_RANGE), "RANGE");
    EXPECT_STREQ(index_cost_condition_string(COND_TEXT), "TEXT");
    EXPECT_STREQ(index_cost_condition_string((condition_type_t)100), "UNKNOWN");
}

TEST_F(IndexCostTest, CostEstimateDifferentIndexTypes)
{
    /* Test HASH index */
    entry.type = INDEX_TYPE_HASH;
    const index_cost_t *cost = index_cost_estimate(&entry, &cond, &stats);
    ASSERT_NE(cost, nullptr);
    EXPECT_GT(cost->total_cost, 0.0);

    /* Test HNSW index */
    entry.type = INDEX_TYPE_HNSW;
    entry.config.ef_search = 100;
    cost = index_cost_estimate(&entry, &cond, &stats);
    ASSERT_NE(cost, nullptr);
    EXPECT_GT(cost->total_cost, 0.0);

    /* Test FULLTEXT index */
    entry.type = INDEX_TYPE_FULLTEXT;
    cost = index_cost_estimate(&entry, &cond, &stats);
    ASSERT_NE(cost, nullptr);
    EXPECT_GT(cost->total_cost, 0.0);
}

// ============================================================
// IndexSelector Tests
// ============================================================

class IndexSelectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = index_manager_create();

        int columns[] = {1};
        index_config_t config = index_config_default();

        /* Create a B-tree index */
        index_manager_create_index(mgr, "btree_idx", INDEX_TYPE_BTREE,
                                   100, columns, 1, &config);

        /* Create table stats */
        memset(&stats, 0, sizeof(stats));
        stats.row_count = 10000;
        stats.page_count = 100;
        stats.avg_row_width = 100.0;
        stats.distinct_values = 1000;

        /* Create query condition */
        memset(&cond, 0, sizeof(cond));
        cond.type = COND_EQ;
        cond.column_id = 1;

        sel = index_selector_create(mgr);
    }

    void TearDown() override {
        if (sel) {
            index_selector_destroy(sel);
            sel = nullptr;
        }
        if (mgr) {
            index_manager_destroy(mgr);
            mgr = nullptr;
        }
    }

    index_manager_t *mgr = nullptr;
    index_selector_t *sel = nullptr;
    table_stats_t stats;
    query_condition_t cond;
};

TEST_F(IndexSelectorTest, CreateAndDestroy)
{
    index_selector_t *s = index_selector_create(mgr);
    ASSERT_NE(s, nullptr);
    index_selector_destroy(s);
}

TEST_F(IndexSelectorTest, CreateWithNullManager)
{
    EXPECT_EQ(index_selector_create(nullptr), nullptr);
}

TEST_F(IndexSelectorTest, DestroyNull)
{
    /* Should not crash */
    index_selector_destroy(nullptr);
}

TEST_F(IndexSelectorTest, FindBest)
{
    index_cost_t best_cost;
    int result = index_selector_find_best(sel, 100, &cond, &stats, &best_cost);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(best_cost.index_id, 1);
    EXPECT_GT(best_cost.total_cost, 0.0);
}

TEST_F(IndexSelectorTest, FindBestNoIndexes)
{
    index_cost_t best_cost;
    int result = index_selector_find_best(sel, 999, &cond, &stats, &best_cost);
    EXPECT_EQ(result, -1);
}

TEST_F(IndexSelectorTest, FindBestNullParams)
{
    index_cost_t best_cost;
    EXPECT_EQ(index_selector_find_best(nullptr, 100, &cond, &stats, &best_cost), -1);
    EXPECT_EQ(index_selector_find_best(sel, 100, nullptr, &stats, &best_cost), -1);
    EXPECT_EQ(index_selector_find_best(sel, 100, &cond, nullptr, &best_cost), -1);
    EXPECT_EQ(index_selector_find_best(sel, 100, &cond, &stats, nullptr), -1);
}

TEST_F(IndexSelectorTest, EvaluateAll)
{
    index_cost_t costs[10];
    int count = index_selector_evaluate_all(sel, 100, &cond, &stats, costs, 10);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(costs[0].index_id, 1);

    count = index_selector_evaluate_all(sel, 999, &cond, &stats, costs, 10);
    EXPECT_EQ(count, -1);
}

TEST_F(IndexSelectorTest, EvaluateAllNullParams)
{
    index_cost_t costs[10];
    EXPECT_EQ(index_selector_evaluate_all(nullptr, 100, &cond, &stats, costs, 10), -1);
    EXPECT_EQ(index_selector_evaluate_all(sel, 100, nullptr, &stats, costs, 10), -1);
    EXPECT_EQ(index_selector_evaluate_all(sel, 100, &cond, nullptr, costs, 10), -1);
    EXPECT_EQ(index_selector_evaluate_all(sel, 100, &cond, &stats, nullptr, 10), -1);
    EXPECT_EQ(index_selector_evaluate_all(sel, 100, &cond, &stats, costs, 0), -1);
}

// ============================================================
// Main function
// ============================================================

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
