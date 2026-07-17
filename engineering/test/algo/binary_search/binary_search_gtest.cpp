#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

extern "C" {
#include <algo-prod/binary_search/binary_search.h>
}

namespace {

int int_compare(const void *lhs, const void *rhs) {
    const int left = *static_cast<const int *>(lhs);
    const int right = *static_cast<const int *>(rhs);

    if (left < right) {
        return -1;
    }
    if (left > right) {
        return 1;
    }
    return 0;
}

struct Record {
    int key;
    int value;
};

int record_compare_by_key(const void *lhs, const void *rhs) {
    const Record *left = static_cast<const Record *>(lhs);
    const Record *right = static_cast<const Record *>(rhs);

    if (left->key < right->key) {
        return -1;
    }
    if (left->key > right->key) {
        return 1;
    }
    return 0;
}

}  // namespace

TEST(BinarySearchTest, FindsExistingIntegerElement) {
    const int values[] = {1, 3, 5, 7, 9, 11};
    const int key = 7;
    size_t index = 0u;

    EXPECT_TRUE(binary_search(values, std::size(values), sizeof(values[0]), &key, int_compare, &index));
    EXPECT_EQ(index, 3u);
}

TEST(BinarySearchTest, ReturnsFalseAndInsertionPointForMissingElement) {
    const int values[] = {2, 4, 6, 8, 10};
    const int key = 5;
    size_t index = 99u;

    EXPECT_FALSE(binary_search(values, std::size(values), sizeof(values[0]), &key, int_compare, &index));
    EXPECT_EQ(binary_search_lower_bound(values, std::size(values), sizeof(values[0]), &key, int_compare), 2u);
    EXPECT_EQ(binary_search_upper_bound(values, std::size(values), sizeof(values[0]), &key, int_compare), 2u);
    EXPECT_EQ(index, 99u);
}

TEST(BinarySearchTest, HandlesDuplicateRangesWithBounds) {
    const int values[] = {1, 2, 2, 2, 4, 4, 6};
    const int key = 2;
    size_t index = 0u;

    EXPECT_TRUE(binary_search(values, std::size(values), sizeof(values[0]), &key, int_compare, &index));
    EXPECT_EQ(index, 1u);
    EXPECT_EQ(binary_search_lower_bound(values, std::size(values), sizeof(values[0]), &key, int_compare), 1u);
    EXPECT_EQ(binary_search_upper_bound(values, std::size(values), sizeof(values[0]), &key, int_compare), 4u);
}

TEST(BinarySearchTest, WorksWithStructArraysUsingCustomComparator) {
    const std::vector<Record> records = {
        {1, 100},
        {3, 300},
        {5, 500},
        {8, 800}
    };
    const Record key{5, 0};
    size_t index = 0u;

    ASSERT_TRUE(binary_search(records.data(), records.size(), sizeof(Record), &key, record_compare_by_key, &index));
    EXPECT_EQ(index, 2u);
    EXPECT_EQ(records[index].value, 500);
}

TEST(BinarySearchTest, RejectsInvalidInputs) {
    const int values[] = {1, 2, 3};
    const int key = 2;

    EXPECT_FALSE(binary_search(nullptr, std::size(values), sizeof(values[0]), &key, int_compare, nullptr));
    EXPECT_FALSE(binary_search(values, std::size(values), 0u, &key, int_compare, nullptr));
    EXPECT_FALSE(binary_search(values, std::size(values), sizeof(values[0]), nullptr, int_compare, nullptr));
    EXPECT_FALSE(binary_search(values, std::size(values), sizeof(values[0]), &key, nullptr, nullptr));
    EXPECT_EQ(binary_search_lower_bound(nullptr, std::size(values), sizeof(values[0]), &key, int_compare), std::size(values));
    EXPECT_EQ(binary_search_upper_bound(values, std::size(values), sizeof(values[0]), nullptr, int_compare), std::size(values));
}