/*
 * consistent_hash_test.cpp — Consistent Hash 单元测试
 *
 * 测试内容：
 * 1. 基本创建和销毁
 * 2. 节点添加
 * 3. 节点移除
 * 4. 查找节点
 * 5. 负载均衡
 * 6. 节点增删后的查找
 */

#include <gtest/gtest.h>
#include <ds/distributed_index.h>

#include <cstring>
#include <string>
#include <map>
#include <cmath>

/* ============================================================
 * 测试 1: 基本创建和销毁
 * ============================================================ */

TEST(ConsistentHashTest, CreateAndDestroy)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    EXPECT_TRUE(ch_is_empty(ring));
    EXPECT_EQ(ch_get_node_count(ring), 0u);
    EXPECT_EQ(ch_get_vnode_count(ring), 0u);

    ch_destroy(ring);
}

TEST(ConsistentHashTest, CreateWithNodes)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    EXPECT_EQ(ch_add_node(ring, "node1", 0), 0);
    EXPECT_EQ(ch_add_node(ring, "node2", 0), 0);
    EXPECT_EQ(ch_add_node(ring, "node3", 0), 0);

    EXPECT_FALSE(ch_is_empty(ring));
    EXPECT_EQ(ch_get_node_count(ring), 3u);
    EXPECT_EQ(ch_get_vnode_count(ring), 3u * CH_DEFAULT_VNODE_COUNT);

    ch_destroy(ring);
}

/* ============================================================
 * 测试 2: 节点添加
 * ============================================================ */

TEST(ConsistentHashTest, AddNode)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    /* 添加第一个节点 */
    EXPECT_EQ(ch_add_node(ring, "192.168.1.1", 0), 0);
    EXPECT_EQ(ch_get_node_count(ring), 1u);

    /* 添加相同节点应该返回 1 */
    EXPECT_EQ(ch_add_node(ring, "192.168.1.1", 0), 1);

    /* 添加第二个节点 */
    EXPECT_EQ(ch_add_node(ring, "192.168.1.2", 0), 0);
    EXPECT_EQ(ch_get_node_count(ring), 2u);

    ch_destroy(ring);
}

TEST(ConsistentHashTest, AddNodeWithCustomVnodeCount)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    /* 添加节点，指定虚拟节点数量 */
    EXPECT_EQ(ch_add_node(ring, "node1", 10), 0);
    EXPECT_EQ(ch_get_vnode_count(ring), 10u);

    /* 添加节点，使用默认虚拟节点数量 */
    EXPECT_EQ(ch_add_node(ring, "node2", 0), 0);
    EXPECT_EQ(ch_get_vnode_count(ring), 10u + CH_DEFAULT_VNODE_COUNT);

    ch_destroy(ring);
}

TEST(ConsistentHashTest, HasNode)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    ch_add_node(ring, "node1", 0);

    EXPECT_TRUE(ch_has_node(ring, "node1"));
    EXPECT_FALSE(ch_has_node(ring, "node2"));

    ch_destroy(ring);
}

/* ============================================================
 * 测试 3: 节点移除
 * ============================================================ */

TEST(ConsistentHashTest, RemoveNode)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    ch_add_node(ring, "node1", 0);
    ch_add_node(ring, "node2", 0);

    EXPECT_EQ(ch_get_node_count(ring), 2u);

    /* 移除一个节点 */
    EXPECT_EQ(ch_remove_node(ring, "node1"), 0);
    EXPECT_EQ(ch_get_node_count(ring), 1u);
    EXPECT_FALSE(ch_has_node(ring, "node1"));
    EXPECT_TRUE(ch_has_node(ring, "node2"));

    /* 移除不存在的节点 */
    EXPECT_EQ(ch_remove_node(ring, "nonexistent"), 1);

    ch_destroy(ring);
}

TEST(ConsistentHashTest, RemoveAllNodes)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    ch_add_node(ring, "node1", 0);
    ch_add_node(ring, "node2", 0);
    ch_add_node(ring, "node3", 0);

    ch_remove_node(ring, "node1");
    ch_remove_node(ring, "node2");
    ch_remove_node(ring, "node3");

    EXPECT_TRUE(ch_is_empty(ring));
    EXPECT_EQ(ch_get_node_count(ring), 0u);
    EXPECT_EQ(ch_get_vnode_count(ring), 0u);

    ch_destroy(ring);
}

/* ============================================================
 * 测试 4: 查找节点
 * ============================================================ */

TEST(ConsistentHashTest, FindNode)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    ch_add_node(ring, "node1", 0);
    ch_add_node(ring, "node2", 0);
    ch_add_node(ring, "node3", 0);

    /* 查找多个 key 的归属节点 */
    std::map<std::string, int> node_counts;

    for (int i = 0; i < 1000; i++) {
        std::string key = "user_" + std::to_string(i);
        const char *node = ch_find_node(ring, key.c_str(), key.size());
        ASSERT_NE(node, nullptr);
        node_counts[node]++;
    }

    /* 每个节点应该分配到一些 key */
    EXPECT_EQ(node_counts.size(), 3u);
    for (const auto &p : node_counts) {
        EXPECT_GT(p.second, 0);
    }

    ch_destroy(ring);
}

TEST(ConsistentHashTest, FindNodeOnEmptyRing)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    const char *node = ch_find_node(ring, "key", 3);
    EXPECT_EQ(node, nullptr);

    ch_destroy(ring);
}

TEST(ConsistentHashTest, FindNodeDeterministic)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    ch_add_node(ring, "node1", 0);
    ch_add_node(ring, "node2", 0);

    /* 同一个 key 应该始终返回相同节点 */
    const char *key = "consistent_key";
    size_t keylen = strlen(key);

    for (int i = 0; i < 100; i++) {
        const char *node = ch_find_node(ring, key, keylen);
        EXPECT_STREQ(node, ch_find_node(ring, key, keylen));
    }

    ch_destroy(ring);
}

/* ============================================================
 * 测试 5: 负载均衡
 * ============================================================ */

TEST(ConsistentHashTest, LoadBalancing)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    /* 添加 10 个节点 */
    for (int i = 0; i < 10; i++) {
        std::string node = "node_" + std::to_string(i);
        ch_add_node(ring, node.c_str(), 0);
    }

    EXPECT_EQ(ch_get_node_count(ring), 10u);

    /* 分配 10000 个 key */
    std::map<std::string, int> node_counts;
    const size_t num_keys = 10000;

    for (size_t i = 0; i < num_keys; i++) {
        std::string key = "key_" + std::to_string(i);
        const char *node = ch_find_node(ring, key.c_str(), key.size());
        node_counts[node]++;
    }

    /* 验证所有 key 都分配了 */
    EXPECT_EQ(node_counts.size(), 10u);

    /* 计算每个节点的负载比例 */
    double expected = (double)num_keys / 10.0;
    double max_deviation = 0.0;

    for (const auto &p : node_counts) {
        double deviation = fabs((double)p.second - expected) / expected;
        max_deviation = std::max(max_deviation, deviation);
    }

    /* 负载偏差应该在 20% 以内 */
    EXPECT_LT(max_deviation, 0.2);

    ch_destroy(ring);
}

/* ============================================================
 * 测试 6: 节点增删后的查找
 * ============================================================ */

TEST(ConsistentHashTest, NodeAddition)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    ch_add_node(ring, "node1", 0);
    ch_add_node(ring, "node2", 0);

    /* 记录添加第三个节点前后的分布变化 */
    std::map<std::string, int> before, after;

    for (int i = 0; i < 1000; i++) {
        std::string key = "key_" + std::to_string(i);
        const char *node = ch_find_node(ring, key.c_str(), key.size());
        before[node]++;
    }

    ch_add_node(ring, "node3", 0);

    for (int i = 0; i < 1000; i++) {
        std::string key = "key_" + std::to_string(i);
        const char *node = ch_find_node(ring, key.c_str(), key.size());
        after[node]++;
    }

    /* 节点数量应该从 2 增加到 3 */
    EXPECT_EQ(before.size(), 2u);
    EXPECT_EQ(after.size(), 3u);

    /* node3 应该分配到一些 key */
    EXPECT_GT(after["node3"], 0);

    ch_destroy(ring);
}

TEST(ConsistentHashTest, NodeRemoval)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    ch_add_node(ring, "node1", 0);
    ch_add_node(ring, "node2", 0);
    ch_add_node(ring, "node3", 0);

    /* 移除 node2 前的分布 */
    std::map<std::string, int> before, after;

    for (int i = 0; i < 1000; i++) {
        std::string key = "key_" + std::to_string(i);
        const char *node = ch_find_node(ring, key.c_str(), key.size());
        before[node]++;
    }

    ch_remove_node(ring, "node2");

    for (int i = 0; i < 1000; i++) {
        std::string key = "key_" + std::to_string(i);
        const char *node = ch_find_node(ring, key.c_str(), key.size());
        after[node]++;
    }

    /* 节点数量应该从 3 减少到 2 */
    EXPECT_EQ(before.size(), 3u);
    EXPECT_EQ(after.size(), 2u);

    /* node2 不应该再出现 */
    EXPECT_EQ(after["node2"], 0);

    ch_destroy(ring);
}

/* ============================================================
 * 测试 7: 单节点场景
 * ============================================================ */

TEST(ConsistentHashTest, SingleNode)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    ch_add_node(ring, "only_node", 0);

    /* 所有 key 都应该分配给唯一节点 */
    for (int i = 0; i < 100; i++) {
        std::string key = "key_" + std::to_string(i);
        const char *node = ch_find_node(ring, key.c_str(), key.size());
        EXPECT_STREQ(node, "only_node");
    }

    ch_destroy(ring);
}

/* ============================================================
 * 测试 8: 二进制 key
 * ============================================================ */

TEST(ConsistentHashTest, BinaryKeys)
{
    consistent_hash_t *ring = ch_create();
    ASSERT_NE(ring, nullptr);

    ch_add_node(ring, "node1", 0);
    ch_add_node(ring, "node2", 0);

    /* 使用二进制数据作为 key */
    const uint8_t binary_key[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};

    const char *node = ch_find_node(ring, binary_key, sizeof(binary_key));
    EXPECT_NE(node, nullptr);

    ch_destroy(ring);
}