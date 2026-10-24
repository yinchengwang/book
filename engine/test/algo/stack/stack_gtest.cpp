#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>

extern "C" {
#include <algo-prod/stack/stack.h>
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

TEST(StackTest, PushPopAndTopFollowLifo) {
    stack_t *stack = stack_create(sizeof(int));
    int values[] = {1, 2, 3};
    int out = 0;

    ASSERT_NE(stack, nullptr);
    for (const int value : values) {
        ASSERT_EQ(stack_push(stack, &value), 0);
    }
    EXPECT_EQ(stack_size(stack), 3u);
    ASSERT_NE(stack_top_ptr(stack), nullptr);
    EXPECT_EQ(*static_cast<const int *>(stack_top_ptr(stack)), 3);
    EXPECT_EQ(stack_top(stack, &out), 0);
    EXPECT_EQ(out, 3);

    EXPECT_EQ(stack_pop(stack, &out), 0);
    EXPECT_EQ(out, 3);
    EXPECT_EQ(stack_pop(stack, &out), 0);
    EXPECT_EQ(out, 2);
    EXPECT_EQ(stack_pop(stack, &out), 0);
    EXPECT_EQ(out, 1);
    EXPECT_TRUE(stack_empty(stack));

    stack_destroy(stack);
}

TEST(StackTest, ClearAndDestroyUseFreeElementCallback) {
    stack_t *stack = stack_create_ex(sizeof(OwnedString), 0u, free_owned_string);
    OwnedString value1{duplicate_c_string("alpha")};
    OwnedString value2{duplicate_c_string("beta")};

    ASSERT_NE(stack, nullptr);
    ASSERT_NE(value1.text, nullptr);
    ASSERT_NE(value2.text, nullptr);
    ASSERT_EQ(stack_push(stack, &value1), 0);
    ASSERT_EQ(stack_push(stack, &value2), 0);

    stack_clear(stack);
    EXPECT_TRUE(stack_empty(stack));
    stack_destroy(stack);
}

TEST(StackTest, ReserveAndShrinkToFitPreserveElements) {
    stack_t *stack = stack_create_ex(sizeof(int), 1u, nullptr);
    int values[] = {10, 20, 30, 40, 50};
    int out = 0;

    ASSERT_NE(stack, nullptr);
    EXPECT_EQ(stack_reserve(stack, 32u), 0);
    for (const int value : values) {
        ASSERT_EQ(stack_push(stack, &value), 0);
    }

    EXPECT_EQ(stack_shrink_to_fit(stack), 0);
    EXPECT_EQ(stack_top(stack, &out), 0);
    EXPECT_EQ(out, 50);

    for (int index = static_cast<int>(std::size(values)) - 1; index >= 0; --index) {
        ASSERT_EQ(stack_pop(stack, &out), 0);
        EXPECT_EQ(out, values[index]);
    }

    EXPECT_TRUE(stack_empty(stack));
    EXPECT_EQ(stack_shrink_to_fit(stack), 0);

    stack_destroy(stack);
}

TEST(StackTest, RejectsInvalidOperations) {
    int value = 7;

    EXPECT_EQ(stack_create(0u), nullptr);
    EXPECT_EQ(stack_push(nullptr, &value), -1);
    EXPECT_EQ(stack_pop(nullptr, &value), -1);
    EXPECT_EQ(stack_top(nullptr, &value), -1);
    EXPECT_EQ(stack_reserve(nullptr, 8u), -1);
    EXPECT_EQ(stack_shrink_to_fit(nullptr), -1);
}