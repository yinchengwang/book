/**
 * @file index_integration_test.cpp
 * @brief Gap#7 Index Framework Integration Tests
 *
 * Tests the complete index workflow:
 * 1. IndexManager - index lifecycle management
 * 2. Index creation - Hash, BTree indexes
 * 3. IndexSelector - optimal index selection
 * 4. IndexScanExec - scan node creation
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

extern "C" {
#include "db/index/index_manager.h"
#include "db/index/index_selector.h"
#include "db/index/index_catalog.h"
#include "db/index/index_cost.h"
#include "db/index/index_config.h"
#include "db/executor/exec_index_scan.h"
}

/* Test fixture for Index Framework integration tests */
class IndexFrameworkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = index_manager_create();
        ASSERT_NE(mgr, nullptr);
    }

    void TearDown() override {
        if (mgr != nullptr) {
            index_manager_destroy(mgr);
            mgr = nullptr;
        }
    }

    index_manager_t *mgr = nullptr;
};

/**
 * Test: IndexManager Creation and Destruction
 *
 * Verify that IndexManager can be created and destroyed without memory errors.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexManagerCreateDestroy) {
    ASSERT_NE(mgr, nullptr);

    index_catalog_t *catalog = index_manager_get_catalog(mgr);
    ASSERT_NE(catalog, nullptr);

    /* IndexManager should start empty */
    EXPECT_EQ(index_catalog_get(catalog, 1), nullptr);
}

/**
 * Test: Create Hash Index
 *
 * Verify that a HASH index can be created through IndexManager.
 */
TEST_F(IndexFrameworkIntegrationTest, CreateHashIndex) {
    int columns[] = {1};

    index_config_t config = index_config_default();

    int result = index_manager_create_index(
        mgr,
        "test_hash_idx",
        INDEX_TYPE_HASH,
        1,                  /* table_id */
        columns,
        1,                  /* column_count */
        &config
    );

    EXPECT_EQ(result, 0);

    /* Verify index was created */
    const index_entry_t *entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->name, "test_hash_idx");
    EXPECT_EQ(entry->type, INDEX_TYPE_HASH);
    EXPECT_EQ(entry->state, INDEX_STATE_READY);
    EXPECT_EQ(entry->table_id, 1);
    EXPECT_EQ(entry->column_count, 1);
}

/**
 * Test: Create BTree Index
 *
 * Verify that a BTREE index can be created through IndexManager.
 */
TEST_F(IndexFrameworkIntegrationTest, CreateBTreeIndex) {
    int columns[] = {2, 3};

    index_config_t config = index_config_default();

    int result = index_manager_create_index(
        mgr,
        "test_btree_idx",
        INDEX_TYPE_BTREE,
        1,                  /* table_id */
        columns,
        2,                  /* column_count */
        &config
    );

    EXPECT_EQ(result, 0);

    /* Verify index was created */
    const index_entry_t *entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->name, "test_btree_idx");
    EXPECT_EQ(entry->type, INDEX_TYPE_BTREE);
    EXPECT_EQ(entry->state, INDEX_STATE_READY);
}

/**
 * Test: Create Multiple Indexes
 *
 * Verify that multiple indexes of different types can be created.
 */
TEST_F(IndexFrameworkIntegrationTest, CreateMultipleIndexes) {
    int col1[] = {1};
    int col2[] = {2};

    index_config_t config = index_config_default();

    /* Create Hash index */
    EXPECT_EQ(index_manager_create_index(mgr, "hash_idx", INDEX_TYPE_HASH,
                                          1, col1, 1, &config), 0);

    /* Create BTree index */
    EXPECT_EQ(index_manager_create_index(mgr, "btree_idx", INDEX_TYPE_BTREE,
                                          1, col2, 1, &config), 0);

    /* Verify both indexes exist */
    index_entry_t *results[10];
    int count = index_manager_get_table_indexes(mgr, 1, results, 10);
    EXPECT_EQ(count, 2);
}

/**
 * Test: Drop Index
 *
 * Verify that an index can be dropped through IndexManager.
 */
TEST_F(IndexFrameworkIntegrationTest, DropIndex) {
    int columns[] = {1};
    index_config_t config = index_config_default();

    /* Create index */
    EXPECT_EQ(index_manager_create_index(mgr, "to_drop_idx", INDEX_TYPE_HASH,
                                          1, columns, 1, &config), 0);

    /* Verify it exists */
    const index_entry_t *entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);

    /* Drop it */
    EXPECT_EQ(index_manager_drop_index(mgr, 1), 0);

    /* Verify it's gone */
    const index_entry_t *deleted = index_manager_get_index(mgr, 1);
    EXPECT_EQ(deleted, nullptr);
}

/**
 * Test: Index Selector Create and Destroy
 *
 * Verify that IndexSelector can be created with a valid IndexManager.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexSelectorCreateDestroy) {
    index_selector_t *sel = index_selector_create(mgr);
    ASSERT_NE(sel, nullptr);

    index_selector_destroy(sel);
}

/**
 * Test: Index Selector Rejects NULL Manager
 *
 * Verify that IndexSelector fails gracefully with NULL manager.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexSelectorRejectsNullManager) {
    index_selector_t *sel = index_selector_create(nullptr);
    EXPECT_EQ(sel, nullptr);
}

/**
 * Test: Index Selector Find Best - No Indexes
 *
 * Verify that IndexSelector returns error when no indexes exist.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexSelectorFindBestNoIndexes) {
    index_selector_t *sel = index_selector_create(mgr);
    ASSERT_NE(sel, nullptr);

    query_condition_t cond = {
        .type = COND_EQ,
        .column_id = 1,
        .value = nullptr,
        .value2 = nullptr
    };

    table_stats_t stats = {
        .row_count = 1000,
        .page_count = 10,
        .avg_row_width = 64,
        .distinct_values = 100
    };

    index_cost_t best_cost;
    int result = index_selector_find_best(sel, 1, &cond, &stats, &best_cost);

    EXPECT_EQ(result, -1);

    index_selector_destroy(sel);
}

/**
 * Test: Index Selector Find Best - With Indexes
 *
 * Verify that IndexSelector can find the best index among multiple candidates.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexSelectorFindBestWithIndexes) {
    int col1[] = {1};
    int col2[] = {2};

    index_config_t config = index_config_default();

    /* Create two indexes on different columns */
    EXPECT_EQ(index_manager_create_index(mgr, "hash_idx", INDEX_TYPE_HASH,
                                          1, col1, 1, &config), 0);
    EXPECT_EQ(index_manager_create_index(mgr, "btree_idx", INDEX_TYPE_BTREE,
                                          1, col2, 1, &config), 0);

    index_selector_t *sel = index_selector_create(mgr);
    ASSERT_NE(sel, nullptr);

    /* Query on column 1 (Hash index) */
    query_condition_t cond = {
        .type = COND_EQ,
        .column_id = 1,
        .value = nullptr,
        .value2 = nullptr
    };

    table_stats_t stats = {
        .row_count = 1000,
        .page_count = 10,
        .avg_row_width = 64,
        .distinct_values = 100
    };

    index_cost_t best_cost;
    int result = index_selector_find_best(sel, 1, &cond, &stats, &best_cost);

    EXPECT_EQ(result, 0);
    EXPECT_GE(best_cost.total_cost, 0);

    index_selector_destroy(sel);
}

/**
 * Test: Index Selector Evaluate All
 *
 * Verify that IndexSelector can evaluate all available indexes.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexSelectorEvaluateAll) {
    int col1[] = {1};

    index_config_t config = index_config_default();

    /* Create a Hash index */
    EXPECT_EQ(index_manager_create_index(mgr, "hash_idx", INDEX_TYPE_HASH,
                                          1, col1, 1, &config), 0);

    index_selector_t *sel = index_selector_create(mgr);
    ASSERT_NE(sel, nullptr);

    query_condition_t cond = {
        .type = COND_EQ,
        .column_id = 1,
        .value = nullptr,
        .value2 = nullptr
    };

    table_stats_t stats = {
        .row_count = 1000,
        .page_count = 10,
        .avg_row_width = 64,
        .distinct_values = 100
    };

    index_cost_t costs[10];
    int count = index_selector_evaluate_all(sel, 1, &cond, &stats, costs, 10);

    EXPECT_GE(count, 1);

    index_selector_destroy(sel);
}

/**
 * Test: Index Scan Exec Node Creation
 *
 * Verify that IndexScanExec node can be created with valid parameters.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexScanExecCreate) {
    /* First create an index */
    int columns[] = {1};
    index_config_t config = index_config_default();
    EXPECT_EQ(index_manager_create_index(mgr, "scan_test_idx", INDEX_TYPE_HASH,
                                          1, columns, 1, &config), 0);

    /* Create index scan node */
    ExecNode *node = exec_create_index_scan(mgr, 1, 1, nullptr);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->node_type, PLAN_SCAN_INDEX);
    EXPECT_NE(node->state, nullptr);

    /* Clean up */
    node->close(node);
    free(node);
}

/**
 * Test: Index Scan Exec Node Lifecycle
 *
 * Verify IndexScanExec node open/next/close lifecycle works.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexScanExecLifecycle) {
    /* First create an index */
    int columns[] = {1};
    index_config_t config = index_config_default();
    EXPECT_EQ(index_manager_create_index(mgr, "lifecycle_test_idx", INDEX_TYPE_BTREE,
                                          1, columns, 1, &config), 0);

    /* Create index scan node */
    ExecNode *node = exec_create_index_scan(mgr, 1, 1, nullptr);
    ASSERT_NE(node, nullptr);

    /* Open the node */
    EXPECT_EQ(node->open(node), 0);

    /* Call next - should return NULL (exhausted) since index is empty */
    VectorBlock *result = node->next(node);
    EXPECT_EQ(result, nullptr);

    /* Reset and close */
    node->reset(node);
    node->close(node);

    free(node);
}

/**
 * Test: Index Scan Exec Rejects NULL Manager
 *
 * Verify that IndexScanExec fails gracefully with NULL manager.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexScanExecRejectsNullManager) {
    ExecNode *node = exec_create_index_scan(nullptr, 1, 1, nullptr);
    EXPECT_EQ(node, nullptr);
}

/**
 * Test: Complete Workflow - Create Indexes and Query
 *
 * Integration test covering the complete workflow:
 * 1. Create IndexManager
 * 2. Create multiple indexes (Hash, BTree)
 * 3. Use IndexSelector to select optimal index
 * 4. Use IndexScanExec to create scan node
 */
TEST_F(IndexFrameworkIntegrationTest, CompleteWorkflow) {
    /* Step 1: Create indexes */
    int col_hash[] = {1};
    int col_btree[] = {2};

    index_config_t config = index_config_default();

    EXPECT_EQ(index_manager_create_index(mgr, "workflow_hash", INDEX_TYPE_HASH,
                                          1, col_hash, 1, &config), 0);
    EXPECT_EQ(index_manager_create_index(mgr, "workflow_btree", INDEX_TYPE_BTREE,
                                          1, col_btree, 1, &config), 0);

    /* Step 2: Create IndexSelector */
    index_selector_t *sel = index_selector_create(mgr);
    ASSERT_NE(sel, nullptr);

    /* Step 3: Query for best index */
    query_condition_t cond = {
        .type = COND_EQ,
        .column_id = 1,
        .value = nullptr,
        .value2 = nullptr
    };

    table_stats_t stats = {
        .row_count = 10000,
        .page_count = 100,
        .avg_row_width = 128,
        .distinct_values = 1000
    };

    index_cost_t best_cost;
    int result = index_selector_find_best(sel, 1, &cond, &stats, &best_cost);
    EXPECT_EQ(result, 0);

    /* Step 4: Create IndexScanExec with selected index */
    ExecNode *scan_node = exec_create_index_scan(mgr, best_cost.index_id, 1, nullptr);
    ASSERT_NE(scan_node, nullptr);

    /* Verify node is properly configured */
    EXPECT_EQ(scan_node->node_type, PLAN_SCAN_INDEX);

    /* Cleanup */
    index_selector_destroy(sel);
    scan_node->close(scan_node);
    free(scan_node);
}

/**
 * Test: Cost Estimation for Different Index Types
 *
 * Verify that cost estimation works correctly for different index types.
 */
TEST_F(IndexFrameworkIntegrationTest, CostEstimationDifferentTypes) {
    int col[] = {1};

    index_config_t config = index_config_default();

    /* Create Hash index */
    EXPECT_EQ(index_manager_create_index(mgr, "cost_hash", INDEX_TYPE_HASH,
                                          1, col, 1, &config), 0);

    /* Create BTree index */
    EXPECT_EQ(index_manager_create_index(mgr, "cost_btree", INDEX_TYPE_BTREE,
                                          1, col, 1, &config), 0);

    query_condition_t cond = {
        .type = COND_EQ,
        .column_id = 1,
        .value = nullptr,
        .value2 = nullptr
    };

    table_stats_t stats = {
        .row_count = 1000,
        .page_count = 10,
        .avg_row_width = 64,
        .distinct_values = 100
    };

    /* Evaluate all indexes */
    index_cost_t costs[10];
    int count = index_selector_evaluate_all(mgr, 1, &cond, &stats, costs, 10);
    EXPECT_EQ(count, 2);

    /* Both should have valid costs */
    EXPECT_GE(costs[0].total_cost, 0);
    EXPECT_GE(costs[1].total_cost, 0);
}

/**
 * Test: Index Type to String Conversion
 *
 * Verify index type string conversion functions.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexTypeStringConversion) {
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_BTREE), "BTREE");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_HASH), "HASH");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_HNSW), "HNSW");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_IVF), "IVF");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_FULLTEXT), "FULLTEXT");
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_GIN), "GIN");

    /* Invalid type */
    EXPECT_STREQ(index_type_to_string(INDEX_TYPE_COUNT), "UNKNOWN");
    EXPECT_STREQ(index_type_to_string((index_type_t)100), "UNKNOWN");
}

/**
 * Test: Index Config Default and Validation
 *
 * Verify index config default values and validation.
 */
TEST_F(IndexFrameworkIntegrationTest, IndexConfigDefaultAndValidation) {
    index_config_t config = index_config_default();

    /* Check default values */
    EXPECT_EQ(config.dims, 128);
    EXPECT_EQ(config.M, 16);
    EXPECT_EQ(config.ef_construction, 200);
    EXPECT_EQ(config.ef_search, 100);
    EXPECT_EQ(config.metric, DISTANCE_L2);
    EXPECT_EQ(config.quantization_type, QUANTIZATION_TYPE_NONE);
    EXPECT_FALSE(config.persist_enabled);

    /* Valid config should pass validation */
    EXPECT_EQ(index_config_validate(&config), 0);

    /* Invalid configs should fail */
    index_config_t invalid_config = config;
    invalid_config.dims = 0;
    EXPECT_NE(index_config_validate(&invalid_config), 0);

    invalid_config = config;
    invalid_config.M = 0;
    EXPECT_NE(index_config_validate(&invalid_config), 0);

    invalid_config = config;
    invalid_config.ef_construction = 0;
    EXPECT_NE(index_config_validate(&invalid_config), 0);

    invalid_config = config;
    invalid_config.ef_search = 0;
    EXPECT_NE(index_config_validate(&invalid_config), 0);

    /* NULL config should fail */
    EXPECT_NE(index_config_validate(nullptr), 0);
}

/**
 * Test: Condition Type String Conversion
 *
 * Verify condition type string conversion.
 */
TEST_F(IndexFrameworkIntegrationTest, ConditionTypeStringConversion) {
    EXPECT_STREQ(index_cost_condition_string(COND_EQ), "EQ");
    EXPECT_STREQ(index_cost_condition_string(COND_LT), "LT");
    EXPECT_STREQ(index_cost_condition_string(COND_LE), "LE");
    EXPECT_STREQ(index_cost_condition_string(COND_GT), "GT");
    EXPECT_STREQ(index_cost_condition_string(COND_GE), "GE");
    EXPECT_STREQ(index_cost_condition_string(COND_RANGE), "RANGE");
    EXPECT_STREQ(index_cost_condition_string(COND_TEXT), "TEXT");
}

/**
 * Test: Selectivity Estimation
 *
 * Verify selectivity estimation for different condition types.
 */
TEST_F(IndexFrameworkIntegrationTest, SelectivityEstimation) {
    int col[] = {1};
    index_config_t config = index_config_default();

    EXPECT_EQ(index_manager_create_index(mgr, "sel_test_idx", INDEX_TYPE_BTREE,
                                          1, col, 1, &config), 0);

    const index_entry_t *entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);

    table_stats_t stats = {
        .row_count = 1000,
        .page_count = 10,
        .avg_row_width = 64,
        .distinct_values = 100
    };

    /* Equality should have 1/NDV selectivity */
    query_condition_t eq_cond = {.type = COND_EQ, .column_id = 1};
    double sel = index_selectivity(entry, &eq_cond, &stats);
    EXPECT_DOUBLE_EQ(sel, 0.01);  /* 1/100 */

    /* Range should have ~10% selectivity */
    query_condition_t range_cond = {.type = COND_RANGE, .column_id = 1};
    sel = index_selectivity(entry, &range_cond, &stats);
    EXPECT_DOUBLE_EQ(sel, 0.10);
}

/**
 * Test: Rebuild Index
 *
 * Verify that an index can be rebuilt.
 */
TEST_F(IndexFrameworkIntegrationTest, RebuildIndex) {
    int columns[] = {1};
    index_config_t config = index_config_default();

    EXPECT_EQ(index_manager_create_index(mgr, "rebuild_idx", INDEX_TYPE_HASH,
                                          1, columns, 1, &config), 0);

    const index_entry_t *entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, INDEX_STATE_READY);

    /* Rebuild should succeed */
    EXPECT_EQ(index_manager_rebuild_index(mgr, 1), 0);

    /* State should still be READY after rebuild */
    entry = index_manager_get_index(mgr, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, INDEX_STATE_READY);
}

/**
 * Test: Get Table Indexes
 *
 * Verify that we can get all indexes for a specific table.
 */
TEST_F(IndexFrameworkIntegrationTest, GetTableIndexes) {
    index_config_t config = index_config_default();

    /* Create indexes for table 1 */
    int col1[] = {1};
    EXPECT_EQ(index_manager_create_index(mgr, "table1_idx1", INDEX_TYPE_HASH,
                                          1, col1, 1, &config), 0);

    int col2[] = {2};
    EXPECT_EQ(index_manager_create_index(mgr, "table1_idx2", INDEX_TYPE_BTREE,
                                          1, col2, 1, &config), 0);

    /* Create index for table 2 */
    EXPECT_EQ(index_manager_create_index(mgr, "table2_idx", INDEX_TYPE_HASH,
                                          2, col1, 1, &config), 0);

    /* Get indexes for table 1 */
    index_entry_t *results[10];
    int count = index_manager_get_table_indexes(mgr, 1, results, 10);
    EXPECT_EQ(count, 2);

    /* Get indexes for table 2 */
    count = index_manager_get_table_indexes(mgr, 2, results, 10);
    EXPECT_EQ(count, 1);

    /* Get indexes for non-existent table */
    count = index_manager_get_table_indexes(mgr, 999, results, 10);
    EXPECT_EQ(count, 0);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
