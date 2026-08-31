#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "db/storage/catalog/catalog.h"

class CatalogTest : public ::testing::Test {
protected:
    void SetUp() override {
        catalog_init();
    }
    void TearDown() override {
        catalog_shutdown();
    }
};

TEST_F(CatalogTest, CreateDrop) {
    /* Create table returns Oid, not 0 */
    Oid oid = catalog_create_table("t1", NULL, 0);
    EXPECT_NE(oid, InvalidOid);

    /* Duplicate name should fail */
    EXPECT_EQ(catalog_create_table("t1", NULL, 0), InvalidOid);

    /* Drop should succeed */
    EXPECT_EQ(catalog_drop_table(oid), CATALOG_SUCCESS);

    /* Double drop should return NOT_FOUND */
    EXPECT_EQ(catalog_drop_table(oid), CATALOG_NOT_FOUND);
}

TEST_F(CatalogTest, Lookup) {
    Oid oid = catalog_create_table("t1", NULL, 0);
    table_info_t *t = catalog_get_table(oid);
    EXPECT_NE(t, nullptr);
    EXPECT_EQ(catalog_get_table(999), nullptr);
    EXPECT_EQ(catalog_drop_table(oid), CATALOG_SUCCESS);
}

TEST_F(CatalogTest, LookupByName) {
    Oid oid = catalog_create_table("mytable", NULL, 0);
    EXPECT_NE(oid, InvalidOid);

    Oid found = catalog_lookup_table("mytable");
    EXPECT_EQ(found, oid);

    EXPECT_EQ(catalog_lookup_table("nonexistent"), InvalidOid);

    catalog_drop_table(oid);
}

TEST_F(CatalogTest, NullSafe) {
    /* These should handle NULL gracefully */
    EXPECT_EQ(catalog_create_table(NULL, NULL, 0), InvalidOid);
    EXPECT_EQ(catalog_get_table(InvalidOid), nullptr);
    EXPECT_EQ(catalog_drop_table(InvalidOid), CATALOG_ERROR);
}

TEST_F(CatalogTest, PersistRecover) {
    /* Create some tables */
    Oid oid1 = catalog_create_table("t1", NULL, 0);
    Oid oid2 = catalog_create_table("t2", NULL, 0);
    EXPECT_NE(oid1, InvalidOid);
    EXPECT_NE(oid2, InvalidOid);

    /* Persist to file */
    EXPECT_EQ(catalog_persist("tests/catalog_test.dat"), 0);

    /* Clear catalog */
    catalog_invalidate_all();
    EXPECT_EQ(catalog_get_table(oid1), nullptr);
    EXPECT_EQ(catalog_get_table(oid2), nullptr);

    /* Recover */
    EXPECT_EQ(catalog_recover("tests/catalog_test.dat"), 0);

    /* Verify tables are back */
    EXPECT_NE(catalog_get_table(oid1), nullptr);
    EXPECT_NE(catalog_get_table(oid2), nullptr);
    EXPECT_EQ(catalog_lookup_table("t1"), oid1);
    EXPECT_EQ(catalog_lookup_table("t2"), oid2);

    /* Cleanup */
    catalog_drop_table(oid1);
    catalog_drop_table(oid2);
    remove("tests/catalog_test.dat");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}