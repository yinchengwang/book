#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include <algo-prod/map/map.h>
}

namespace {

struct IntKey {
    int value;
};

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

uint64_t int_hash(const void *key) {
    const IntKey *int_key = static_cast<const IntKey *>(key);
    return static_cast<uint64_t>(static_cast<uint32_t>(int_key->value * 2654435761u));
}

bool int_equals(const void *lhs, const void *rhs) {
    const IntKey *left = static_cast<const IntKey *>(lhs);
    const IntKey *right = static_cast<const IntKey *>(rhs);
    return left->value == right->value;
}

void free_owned_string(void *value) {
    OwnedString *owned = static_cast<OwnedString *>(value);
    std::free(owned->text);
    owned->text = nullptr;
}

}  // namespace

TEST(MapTest, PutGetContainsAndRemoveWork) {
    map_t *map = map_create(sizeof(IntKey), sizeof(int), int_hash, int_equals);
    IntKey key1{1};
    IntKey key2{2};
    int value1 = 11;
    int value2 = 22;
    int out = 0;

    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map_put(map, &key1, &value1), 0);
    EXPECT_EQ(map_put(map, &key2, &value2), 0);
    EXPECT_TRUE(map_contains(map, &key1));
    EXPECT_EQ(map_size(map), 2u);

    EXPECT_EQ(map_get(map, &key2, &out), 0);
    EXPECT_EQ(out, 22);
    ASSERT_NE(map_get_ptr(map, &key1), nullptr);
    EXPECT_EQ(*static_cast<const int *>(map_get_ptr(map, &key1)), 11);

    EXPECT_EQ(map_remove(map, &key1, &out), 0);
    EXPECT_EQ(out, 11);
    EXPECT_FALSE(map_contains(map, &key1));
    EXPECT_EQ(map_size(map), 1u);

    map_destroy(map);
}

TEST(MapTest, PutReplacesExistingValue) {
    map_t *map = map_create(sizeof(IntKey), sizeof(int), int_hash, int_equals);
    IntKey key{7};
    int value1 = 70;
    int value2 = 700;
    int out = 0;

    ASSERT_NE(map, nullptr);
    ASSERT_EQ(map_put(map, &key, &value1), 0);
    ASSERT_EQ(map_put(map, &key, &value2), 0);
    EXPECT_EQ(map_size(map), 1u);
    EXPECT_EQ(map_get(map, &key, &out), 0);
    EXPECT_EQ(out, 700);

    map_destroy(map);
}

TEST(MapTest, IterationAndRehashPreserveEntries) {
    map_t *map = map_create_ex(sizeof(IntKey), sizeof(int), 2u, int_hash, int_equals, nullptr, nullptr);
    std::vector<int> seen_keys;

    ASSERT_NE(map, nullptr);
    for (int i = 0; i < 32; ++i) {
        IntKey key{i};
        int value = i * 10;
        ASSERT_EQ(map_put(map, &key, &value), 0);
    }

    for (const map_entry_t *entry = map_first(map); entry; entry = map_entry_next(entry)) {
        const IntKey *key = static_cast<const IntKey *>(map_entry_key(entry));
        const int *value = static_cast<const int *>(map_entry_value(entry));

        ASSERT_NE(key, nullptr);
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, key->value * 10);
        seen_keys.push_back(key->value);
    }

    EXPECT_EQ(seen_keys.size(), 32u);
    map_destroy(map);
}

TEST(MapTest, ClearAndDestroyUseFreeValueCallback) {
    map_t *map = map_create_ex(sizeof(IntKey),
                               sizeof(OwnedString),
                               0u,
                               int_hash,
                               int_equals,
                               nullptr,
                               free_owned_string);
    IntKey key1{1};
    IntKey key2{2};
    OwnedString value1{duplicate_c_string("one")};
    OwnedString value2{duplicate_c_string("two")};

    ASSERT_NE(map, nullptr);
    ASSERT_NE(value1.text, nullptr);
    ASSERT_NE(value2.text, nullptr);
    ASSERT_EQ(map_put(map, &key1, &value1), 0);
    ASSERT_EQ(map_put(map, &key2, &value2), 0);

    map_clear(map);
    EXPECT_TRUE(map_empty(map));
    map_destroy(map);
}

TEST(MapTest, RejectsInvalidOperations) {
    IntKey key{1};
    int value = 10;

    EXPECT_EQ(map_create(0u, sizeof(int), int_hash, int_equals), nullptr);
    EXPECT_EQ(map_create(sizeof(IntKey), 0u, int_hash, int_equals), nullptr);
    EXPECT_EQ(map_create(sizeof(IntKey), sizeof(int), nullptr, int_equals), nullptr);
    EXPECT_EQ(map_put(nullptr, &key, &value), -1);
    EXPECT_EQ(map_get(nullptr, &key, &value), -1);
    EXPECT_EQ(map_remove(nullptr, &key, &value), -1);
}