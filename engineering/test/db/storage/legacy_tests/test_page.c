#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "db/page.h"

class PageTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PageTest, ChecksumConsistency) {
    page_t page;
    memset(&page, 0, sizeof(page));
    page.header.magic = PAGE_MAGIC;
    uint32_t cs1 = page_compute_checksum(&page);
    uint32_t cs2 = page_compute_checksum(&page);
    EXPECT_EQ(cs1, cs2);
}

TEST_F(PageTest, ChecksumDifferent) {
    page_t p1, p2;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.header.magic = PAGE_MAGIC;
    p2.header.magic = PAGE_MAGIC;
    p1.data[0] = 0x42;
    p2.data[0] = 0x43;
    EXPECT_NE(page_compute_checksum(&p1), page_compute_checksum(&p2));
}

TEST_F(PageTest, VerifyValid) {
    page_t page;
    memset(&page, 0, sizeof(page));
    page.header.magic = PAGE_MAGIC;
    page.header.checksum = page_compute_checksum(&page);
    EXPECT_EQ(page_verify(&page), 0);
}

TEST_F(PageTest, VerifyBadMagic) {
    page_t page;
    memset(&page, 0, sizeof(page));
    page.header.magic = 0xDEADBEEF;
    EXPECT_NE(page_verify(&page), 0);
}

TEST_F(PageTest, VerifyBadChecksum) {
    page_t page;
    memset(&page, 0, sizeof(page));
    page.header.magic = PAGE_MAGIC;
    page.header.checksum = 0;
    EXPECT_NE(page_verify(&page), 0);
}

TEST_F(PageTest, NullSafe) {
    EXPECT_NE(page_compute_checksum(NULL), 0u);
    EXPECT_NE(page_verify(NULL), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
