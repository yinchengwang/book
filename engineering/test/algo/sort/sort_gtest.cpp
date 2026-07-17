#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

extern "C" {
#include <algo-prod/sort/sort.h>
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

using GenericSortFn = int (*)(void *, size_t, size_t, sort_compare_fn);
using IntSortFn = int (*)(int32_t *, size_t);

void expect_generic_sort_works(GenericSortFn fn) {
    std::vector<int> values = {7, -3, 5, 5, 0, 9, -10, 4};
    const std::vector<int> expected = {-10, -3, 0, 4, 5, 5, 7, 9};

    ASSERT_EQ(fn(values.data(), values.size(), sizeof(int), int_compare), 0);
    EXPECT_EQ(values, expected);
}

void expect_int_sort_works(IntSortFn fn) {
    std::vector<int32_t> values = {7, -3, 5, 5, 0, 9, -10, 4};
    const std::vector<int32_t> expected = {-10, -3, 0, 4, 5, 5, 7, 9};

    ASSERT_EQ(fn(values.data(), values.size()), 0);
    EXPECT_EQ(values, expected);
}

}  // namespace

TEST(SortTest, ComparisonBasedSortsOrderIntegersAscending) {
    const std::array<GenericSortFn, 7> algorithms = {
        sort_bubble,
        sort_selection,
        sort_insertion,
        sort_shell,
        sort_merge,
        sort_quick,
        sort_heap,
    };

    for (GenericSortFn fn : algorithms) {
        expect_generic_sort_works(fn);
    }
}

TEST(SortTest, NonComparisonSortsOrderInt32Ascending) {
    const std::array<IntSortFn, 3> algorithms = {
        sort_counting_int32,
        sort_bucket_int32,
        sort_radix_int32,
    };

    for (IntSortFn fn : algorithms) {
        expect_int_sort_works(fn);
    }
}

TEST(SortTest, ComparisonBasedSortsHandleAlreadySortedAndSingleElementInputs) {
    const std::array<GenericSortFn, 7> algorithms = {
        sort_bubble,
        sort_selection,
        sort_insertion,
        sort_shell,
        sort_merge,
        sort_quick,
        sort_heap,
    };
    std::vector<int> sorted = {1, 2, 3, 4, 5};
    std::vector<int> single = {42};

    for (GenericSortFn fn : algorithms) {
        std::vector<int> values = sorted;
        std::vector<int> one = single;

        ASSERT_EQ(fn(values.data(), values.size(), sizeof(int), int_compare), 0);
        EXPECT_EQ(values, sorted);
        ASSERT_EQ(fn(one.data(), one.size(), sizeof(int), int_compare), 0);
        EXPECT_EQ(one, single);
    }
}

TEST(SortTest, NonComparisonSortsHandleDuplicatesAndNegativeValues) {
    const std::array<IntSortFn, 3> algorithms = {
        sort_counting_int32,
        sort_bucket_int32,
        sort_radix_int32,
    };
    const std::vector<int32_t> expected = {-100, -100, -1, 0, 0, 3, 50, 50};

    for (IntSortFn fn : algorithms) {
        std::vector<int32_t> values = {0, -1, 50, -100, 50, 3, 0, -100};

        ASSERT_EQ(fn(values.data(), values.size()), 0);
        EXPECT_EQ(values, expected);
    }
}

TEST(SortTest, DispatchRoutesToAllAlgorithms) {
    const std::array<sort_algorithm_t, 7> comparison_algorithms = {
        SORT_ALGORITHM_BUBBLE,
        SORT_ALGORITHM_SELECTION,
        SORT_ALGORITHM_INSERTION,
        SORT_ALGORITHM_SHELL,
        SORT_ALGORITHM_MERGE,
        SORT_ALGORITHM_QUICK,
        SORT_ALGORITHM_HEAP,
    };
    const std::array<sort_algorithm_t, 3> int_algorithms = {
        SORT_ALGORITHM_COUNTING_INT32,
        SORT_ALGORITHM_BUCKET_INT32,
        SORT_ALGORITHM_RADIX_INT32,
    };

    for (sort_algorithm_t algorithm : comparison_algorithms) {
        std::vector<int> values = {9, 1, -7, 3, 3, 2};
        const std::vector<int> expected = {-7, 1, 2, 3, 3, 9};

        ASSERT_EQ(sort_dispatch(algorithm, values.data(), values.size(), sizeof(int), int_compare), 0);
        EXPECT_EQ(values, expected);
    }

    for (sort_algorithm_t algorithm : int_algorithms) {
        std::vector<int32_t> values = {9, 1, -7, 3, 3, 2};
        const std::vector<int32_t> expected = {-7, 1, 2, 3, 3, 9};

        ASSERT_EQ(sort_dispatch(algorithm, values.data(), values.size(), sizeof(int32_t), nullptr), 0);
        EXPECT_EQ(values, expected);
    }
}

TEST(SortTest, RejectsInvalidInputs) {
    int values[] = {3, 1, 2};
    int32_t int32_values[] = {3, 1, 2};

    EXPECT_EQ(sort_bubble(nullptr, 3u, sizeof(int), int_compare), -1);
    EXPECT_EQ(sort_selection(values, 3u, 0u, int_compare), -1);
    EXPECT_EQ(sort_insertion(values, 3u, sizeof(int), nullptr), -1);
    EXPECT_EQ(sort_shell(nullptr, 0u, sizeof(int), int_compare), 0);
    EXPECT_EQ(sort_merge(values, 3u, sizeof(int), nullptr), -1);
    EXPECT_EQ(sort_quick(values, 3u, 0u, int_compare), -1);
    EXPECT_EQ(sort_heap(nullptr, 2u, sizeof(int), int_compare), -1);

    EXPECT_EQ(sort_counting_int32(nullptr, 3u), -1);
    EXPECT_EQ(sort_bucket_int32(nullptr, 1u), -1);
    EXPECT_EQ(sort_radix_int32(nullptr, 2u), -1);
    EXPECT_EQ(sort_counting_int32(int32_values, 0u), 0);

    EXPECT_EQ(sort_algorithm_is_valid(SORT_ALGORITHM_BUBBLE), 1);
    EXPECT_EQ(sort_algorithm_is_valid((sort_algorithm_t)-1), 0);
    EXPECT_EQ(sort_algorithm_is_valid((sort_algorithm_t)99), 0);
    EXPECT_EQ(sort_dispatch((sort_algorithm_t)99, values, 3u, sizeof(int), int_compare), -1);
    EXPECT_EQ(sort_dispatch(SORT_ALGORITHM_QUICK, values, 3u, sizeof(int), nullptr), -1);
    EXPECT_EQ(sort_dispatch(SORT_ALGORITHM_COUNTING_INT32, int32_values, 3u, sizeof(int16_t), nullptr), -1);
    EXPECT_EQ(sort_dispatch(SORT_ALGORITHM_COUNTING_INT32, int32_values, 3u, sizeof(int32_t), nullptr), 0);
}