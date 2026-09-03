#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include <algo-prod/list/list.h>
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

TEST(ListTest, PushesAndPopsFromBothEnds) {
    list_t *list = list_create(sizeof(int));
    int one = 1;
    int two = 2;
    int three = 3;
    int out = 0;

    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list_push_back(list, &two), 0);
    EXPECT_EQ(list_push_front(list, &one), 0);
    EXPECT_EQ(list_push_back(list, &three), 0);
    EXPECT_EQ(list_size(list), 3u);

    ASSERT_NE(list_front_ptr(list), nullptr);
    ASSERT_NE(list_back_ptr(list), nullptr);
    EXPECT_EQ(*static_cast<const int *>(list_front_ptr(list)), 1);
    EXPECT_EQ(*static_cast<const int *>(list_back_ptr(list)), 3);

    EXPECT_EQ(list_pop_front(list, &out), 0);
    EXPECT_EQ(out, 1);
    EXPECT_EQ(list_pop_back(list, &out), 0);
    EXPECT_EQ(out, 3);
    EXPECT_EQ(list_pop_front(list, &out), 0);
    EXPECT_EQ(out, 2);
    EXPECT_TRUE(list_empty(list));

    list_destroy(list);
}

TEST(ListTest, SupportsNodeIteration) {
    list_t *list = list_create(sizeof(int));
    int values[] = {10, 20, 30};
    const list_node_t *node;
    int index = 0;

    ASSERT_NE(list, nullptr);
    for (const int value : values) {
        ASSERT_EQ(list_push_back(list, &value), 0);
    }

    node = list_front_node(list);
    while (node) {
        ASSERT_LT(index, 3);
        EXPECT_EQ(*static_cast<const int *>(list_node_value(node)), values[index]);
        node = list_node_next(node);
        ++index;
    }
    EXPECT_EQ(index, 3);

    list_destroy(list);
}

TEST(ListTest, ClearAndDestroyCallFreeElement) {
    list_t *list = list_create_ex(sizeof(OwnedString), free_owned_string);
    OwnedString first{duplicate_c_string("alpha")};
    OwnedString second{duplicate_c_string("beta")};

    ASSERT_NE(list, nullptr);
    ASSERT_NE(first.text, nullptr);
    ASSERT_NE(second.text, nullptr);
    ASSERT_EQ(list_push_back(list, &first), 0);
    ASSERT_EQ(list_push_back(list, &second), 0);

    list_clear(list);
    EXPECT_TRUE(list_empty(list));
    list_destroy(list);
}

TEST(ListTest, RejectsInvalidOperations) {
    int value = 7;

    EXPECT_EQ(list_push_back(nullptr, &value), -1);
    EXPECT_EQ(list_push_front(nullptr, &value), -1);
    EXPECT_EQ(list_pop_front(nullptr, &value), -1);
    EXPECT_EQ(list_pop_back(nullptr, &value), -1);
    EXPECT_EQ(list_create(0u), nullptr);
}