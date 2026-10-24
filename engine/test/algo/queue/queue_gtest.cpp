#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>

extern "C" {
#include <algo-prod/queue/queue.h>
}

namespace {

struct OwnedString {
    char *text;
};

char *duplicate_c_string(const char *text) {
    const size_t length = std::strlen(text);
    char *copy = static_cast<char *>(std::malloc(length + 1u));

    if (!copy) {
        return nullptr;
    }
    std::memcpy(copy, text, length + 1u);
    return copy;
}

void free_owned_string(void *element) {
    OwnedString *owned = static_cast<OwnedString *>(element);
    std::free(owned->text);
    owned->text = nullptr;
}

}  // namespace

TEST(QueueTest, PushPopFrontAndBackFollowFifo) {
    queue_t *queue = queue_create(sizeof(int));
    int values[] = {1, 2, 3};
    int out = 0;

    ASSERT_NE(queue, nullptr);
    for (const int value : values) {
        ASSERT_EQ(queue_push(queue, &value), 0);
    }
    EXPECT_EQ(queue_size(queue), 3u);
    ASSERT_NE(queue_front_ptr(queue), nullptr);
    ASSERT_NE(queue_back_ptr(queue), nullptr);
    EXPECT_EQ(*static_cast<const int *>(queue_front_ptr(queue)), 1);
    EXPECT_EQ(*static_cast<const int *>(queue_back_ptr(queue)), 3);
    EXPECT_EQ(queue_front(queue, &out), 0);
    EXPECT_EQ(out, 1);
    EXPECT_EQ(queue_back(queue, &out), 0);
    EXPECT_EQ(out, 3);

    EXPECT_EQ(queue_pop(queue, &out), 0);
    EXPECT_EQ(out, 1);
    EXPECT_EQ(queue_pop(queue, &out), 0);
    EXPECT_EQ(out, 2);
    EXPECT_EQ(queue_pop(queue, &out), 0);
    EXPECT_EQ(out, 3);
    EXPECT_TRUE(queue_empty(queue));

    queue_destroy(queue);
}

TEST(QueueTest, SupportsWrapAroundAndClear) {
    queue_t *queue = queue_create_ex(sizeof(int), 2u, nullptr);
    int value = 0;

    ASSERT_NE(queue, nullptr);
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(queue_push(queue, &i), 0);
        if (i % 2 == 1) {
            ASSERT_EQ(queue_pop(queue, &value), 0);
        }
    }

    queue_clear(queue);
    EXPECT_TRUE(queue_empty(queue));
    queue_destroy(queue);
}

TEST(QueueTest, ReportsCapacityAndSupportsBatchPush) {
    queue_t *queue = queue_create_ex(sizeof(int), 4u, nullptr);
    int initial_values[] = {1, 2, 3};
    int batch_values[] = {4, 5, 6, 7};
    int out = 0;

    ASSERT_NE(queue, nullptr);
    EXPECT_EQ(queue_capacity(queue), 4u);

    ASSERT_EQ(queue_push_batch(queue, initial_values, std::size(initial_values)), 0);
    EXPECT_EQ(queue_size(queue), std::size(initial_values));
    EXPECT_EQ(queue_capacity(queue), 4u);

    ASSERT_EQ(queue_pop(queue, &out), 0);
    EXPECT_EQ(out, 1);
    ASSERT_EQ(queue_pop(queue, &out), 0);
    EXPECT_EQ(out, 2);

    ASSERT_EQ(queue_push_batch(queue, batch_values, std::size(batch_values)), 0);
    EXPECT_GE(queue_capacity(queue), 5u);
    EXPECT_EQ(queue_size(queue), 5u);

    for (int expected : {3, 4, 5, 6, 7}) {
        ASSERT_EQ(queue_pop(queue, &out), 0);
        EXPECT_EQ(out, expected);
    }

    EXPECT_TRUE(queue_empty(queue));
    queue_destroy(queue);
}

TEST(QueueTest, ClearAndDestroyUseFreeElementCallback) {
    queue_t *queue = queue_create_ex(sizeof(OwnedString), 0u, free_owned_string);
    OwnedString value1{duplicate_c_string("alpha")};
    OwnedString value2{duplicate_c_string("beta")};

    ASSERT_NE(queue, nullptr);
    ASSERT_NE(value1.text, nullptr);
    ASSERT_NE(value2.text, nullptr);
    ASSERT_EQ(queue_push(queue, &value1), 0);
    ASSERT_EQ(queue_push(queue, &value2), 0);

    queue_clear(queue);
    EXPECT_TRUE(queue_empty(queue));
    queue_destroy(queue);
}

TEST(QueueTest, RejectsInvalidOperations) {
    int value = 7;
    int values[] = {1, 2};

    EXPECT_EQ(queue_create(0u), nullptr);
    EXPECT_EQ(queue_push(nullptr, &value), -1);
    EXPECT_EQ(queue_push_batch(nullptr, values, std::size(values)), -1);
    EXPECT_EQ(queue_push_batch(nullptr, nullptr, 0u), -1);
    EXPECT_EQ(queue_push_batch(queue_create(sizeof(int)), nullptr, 1u), -1);
    EXPECT_EQ(queue_pop(nullptr, &value), -1);
    EXPECT_EQ(queue_front(nullptr, &value), -1);
    EXPECT_EQ(queue_back(nullptr, &value), -1);
    EXPECT_EQ(queue_capacity(nullptr), 0u);
}