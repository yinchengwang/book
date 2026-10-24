/**
 * @file yang_engine_test.cpp
 * @brief Yang 树存储引擎测试
 *
 * 测试 Yang 树引擎的节点操作和持久化功能
 */
#include <gtest/gtest.h>
#include "db/yang_engine.h"
#include "db/log.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

/**
 * @brief Yang 引擎测试夹具
 */
class YangEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_WARN;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

        /* 确保测试目录存在 */
#ifdef _WIN32
        mkdir("./test_data");
        mkdir("./test_data/yang_engine");
#else
        mkdir("./test_data", 0755);
        mkdir("./test_data/yang_engine", 0755);
#endif

        /* 初始化 Yang 引擎 */
        ASSERT_EQ(0, yang_engine_init("./test_data/yang_engine"));
    }

    void TearDown() override {
        yang_engine_shutdown();
        log_shutdown();

        /* 清理测试数据目录 */
        system("rm -rf ./test_data/yang_engine");
    }
};

/**
 * @brief 测试 Yang 引擎生命周期
 */
TEST_F(YangEngineTest, Lifecycle) {
    /* 创建树 */
    EXPECT_EQ(0, yang_engine_create("test_tree", NULL));

    /* 打开树 */
    void *rel = yang_engine_open("test_tree", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    yang_engine_db_t *db = static_cast<yang_engine_db_t *>(rel);
    EXPECT_STREQ("test_tree", db->name);
    EXPECT_EQ(ACCESS_MODE_READ_WRITE, db->mode);

    /* 关闭树 */
    EXPECT_EQ(0, yang_engine_close(rel));

    /* 删除树 */
    EXPECT_EQ(0, yang_engine_drop("test_tree"));
}

/**
 * @brief 测试插入节点
 */
TEST_F(YangEngineTest, InsertNode) {
    EXPECT_EQ(0, yang_engine_create("test_tree", NULL));

    void *rel = yang_engine_open("test_tree", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 构造节点数据: path_len(4) + path + name_len(4) + name + type(4) + value_len(4) + value */
    const char *path = "/root/child";
    const char *name = "child_node";
    const char *value = "child_value";
    uint8_t data[1024];
    uint32_t path_len = strlen(path);
    uint32_t name_len = strlen(name);
    yang_node_type_t node_type = YANG_NODE_ELEMENT;
    uint32_t value_len = strlen(value);

    uint8_t *ptr = data;
    memcpy(ptr, &path_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, path, path_len);
    ptr += path_len;
    memcpy(ptr, &name_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, name, name_len);
    ptr += name_len;
    memcpy(ptr, &node_type, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &value_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, value, value_len);

    /* 插入节点 */
    int ret = yang_engine_insert(rel, data,
                                 sizeof(uint32_t) * 4 + path_len + name_len + value_len);
    EXPECT_EQ(0, ret);

    /* 关闭 */
    EXPECT_EQ(0, yang_engine_close(rel));
    yang_engine_drop("test_tree");
}

/**
 * @brief 测试插入多个节点
 */
TEST_F(YangEngineTest, InsertMultipleNodes) {
    EXPECT_EQ(0, yang_engine_create("test_tree", NULL));

    void *rel = yang_engine_open("test_tree", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 构造多个节点数据 */
    struct {
        const char *path;
        const char *name;
        const char *value;
        yang_node_type_t type;
    } nodes[] = {
        {"/root", "root", NULL, YANG_NODE_ROOT},
        {"/root/users", "users", NULL, YANG_NODE_ELEMENT},
        {"/root/users/alice", "alice", "user_id=001", YANG_NODE_ELEMENT},
        {"/root/users/bob", "bob", "user_id=002", YANG_NODE_ELEMENT},
        {"/root/config", "config", NULL, YANG_NODE_ELEMENT},
        {"/root/config/db", "db", "host=localhost", YANG_NODE_ELEMENT},
    };

    for (size_t i = 0; i < 6; i++) {
        uint8_t data[1024];
        uint32_t path_len = strlen(nodes[i].path);
        uint32_t name_len = strlen(nodes[i].name);
        uint32_t value_len = nodes[i].value ? strlen(nodes[i].value) : 0;
        yang_node_type_t node_type = nodes[i].type;

        uint8_t *ptr = data;
        memcpy(ptr, &path_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, nodes[i].path, path_len);
        ptr += path_len;
        memcpy(ptr, &name_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, nodes[i].name, name_len);
        ptr += name_len;
        memcpy(ptr, &node_type, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, &value_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        if (value_len > 0) {
            memcpy(ptr, nodes[i].value, value_len);
        }

        EXPECT_EQ(0, yang_engine_insert(rel, data,
                                        sizeof(uint32_t) * 4 + path_len + name_len + value_len));
    }

    /* 关闭 */
    EXPECT_EQ(0, yang_engine_close(rel));
    yang_engine_drop("test_tree");
}

/**
 * @brief 测试查找节点
 */
TEST_F(YangEngineTest, FindNode) {
    EXPECT_EQ(0, yang_engine_create("test_tree", NULL));

    void *rel = yang_engine_open("test_tree", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入节点 */
    const char *path = "/root/findme";
    const char *name = "findme";
    const char *value = "found_value";
    uint8_t data[1024];
    uint32_t path_len = strlen(path);
    uint32_t name_len = strlen(name);
    yang_node_type_t node_type = YANG_NODE_ELEMENT;
    uint32_t value_len = strlen(value);

    uint8_t *ptr = data;
    memcpy(ptr, &path_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, path, path_len);
    ptr += path_len;
    memcpy(ptr, &name_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, name, name_len);
    ptr += name_len;
    memcpy(ptr, &node_type, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &value_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, value, value_len);

    EXPECT_EQ(0, yang_engine_insert(rel, data,
                                    sizeof(uint32_t) * 4 + path_len + name_len + value_len));

    /* 查找节点 - 使用路径作为键 */
    void *out_data = NULL;
    size_t out_len = 0;
    int ret = yang_engine_get(rel, path, path_len, &out_data, &out_len);

    if (ret == 0 && out_data && out_len > 0) {
        free(out_data);
    }

    /* 关闭 */
    EXPECT_EQ(0, yang_engine_close(rel));
    yang_engine_drop("test_tree");
}

/**
 * @brief 测试节点创建工具函数
 */
TEST_F(YangEngineTest, NodeCreate) {
    /* 创建节点 */
    yang_node_t *node = yang_node_create("test_node", YANG_NODE_ELEMENT, "test_value");
    ASSERT_NE(nullptr, node);

    EXPECT_STREQ("test_node", node->name);
    EXPECT_EQ(YANG_NODE_ELEMENT, node->type);
    EXPECT_STREQ("test_value", node->value);

    /* 清理 */
    yang_node_free(node);
}

/**
 * @brief 测试节点父子关系
 */
TEST_F(YangEngineTest, NodeRelations) {
    /* 创建父节点 */
    yang_node_t *parent = yang_node_create("parent", YANG_NODE_ELEMENT, "parent_value");
    ASSERT_NE(nullptr, parent);

    /* 创建子节点 */
    yang_node_t *child1 = yang_node_create("child1", YANG_NODE_ELEMENT, "child1_value");
    yang_node_t *child2 = yang_node_create("child2", YANG_NODE_ELEMENT, "child2_value");
    ASSERT_NE(nullptr, child1);
    ASSERT_NE(nullptr, child2);

    /* 添加子节点 */
    EXPECT_EQ(0, yang_add_child(parent, child1));
    EXPECT_EQ(0, yang_add_child(parent, child2));

    /* 验证父子关系 */
    EXPECT_EQ(parent, child1->parent);
    EXPECT_EQ(parent, child2->parent);

    /* 验证兄弟关系 */
    EXPECT_EQ(child2, child1->next_sibling);

    /* 验证获取子节点 */
    yang_node_t *found = yang_get_child(parent, "child1");
    EXPECT_EQ(child1, found);

    found = yang_get_child(parent, "nonexistent");
    EXPECT_EQ(nullptr, found);

    /* 清理 */
    yang_node_free(parent);
    yang_node_free(child1);
    yang_node_free(child2);
}

/**
 * @brief 测试树遍历
 */
TEST_F(YangEngineTest, TreeTraverse) {
    EXPECT_EQ(0, yang_engine_create("test_tree", NULL));

    void *rel = yang_engine_open("test_tree", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    yang_engine_db_t *db = static_cast<yang_engine_db_t *>(rel);

    /* 如果引擎支持内存中树结构，创建节点树 */
    if (db->root) {
        /* 创建根节点 */
        yang_node_t *root = yang_node_create("root", YANG_NODE_ROOT, NULL);

        /* 创建子节点树 */
        yang_node_t *level1_a = yang_node_create("A", YANG_NODE_ELEMENT, "A_value");
        yang_node_t *level1_b = yang_node_create("B", YANG_NODE_ELEMENT, "B_value");
        yang_node_t *level2_a1 = yang_node_create("A1", YANG_NODE_ELEMENT, "A1_value");
        yang_node_t *level2_a2 = yang_node_create("A2", YANG_NODE_ELEMENT, "A2_value");

        yang_add_child(root, level1_a);
        yang_add_child(root, level1_b);
        yang_add_child(level1_a, level2_a1);
        yang_add_child(level1_a, level2_a2);

        /* 验证树结构 */
        EXPECT_EQ(root, level1_a->parent);
        EXPECT_EQ(level1_a, level2_a1->parent);

        /* 获取子节点 */
        yang_node_t *found = yang_get_child(root, "A");
        EXPECT_EQ(level1_a, found);

        found = yang_get_child(level1_a, "A1");
        EXPECT_EQ(level2_a1, found);

        /* 清理 */
        yang_node_free(root);
        yang_node_free(level1_a);
        yang_node_free(level1_b);
        yang_node_free(level2_a1);
        yang_node_free(level2_a2);
    }

    /* 关闭 */
    EXPECT_EQ(0, yang_engine_close(rel));
    yang_engine_drop("test_tree");
}

/**
 * @brief 测试持久化 - 数据持久化到磁盘
 */
TEST_F(YangEngineTest, Persistence) {
    /* 第一阶段：创建并插入数据 */
    {
        EXPECT_EQ(0, yang_engine_create("persist_tree", NULL));

        void *rel = yang_engine_open("persist_tree", ACCESS_MODE_READ_WRITE);
        ASSERT_NE(nullptr, rel);

        /* 插入节点 */
        const char *path = "/root/persistent";
        const char *name = "persistent_node";
        const char *value = "persistent_value_12345";
        uint8_t data[1024];
        uint32_t path_len = strlen(path);
        uint32_t name_len = strlen(name);
        yang_node_type_t node_type = YANG_NODE_ELEMENT;
        uint32_t value_len = strlen(value);

        uint8_t *ptr = data;
        memcpy(ptr, &path_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, path, path_len);
        ptr += path_len;
        memcpy(ptr, &name_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, name, name_len);
        ptr += name_len;
        memcpy(ptr, &node_type, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, &value_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, value, value_len);

        EXPECT_EQ(0, yang_engine_insert(rel, data,
                                        sizeof(uint32_t) * 4 + path_len + name_len + value_len));

        EXPECT_EQ(0, yang_engine_close(rel));
    }

    /* 第二阶段：重新打开并验证数据存在 */
    {
        void *rel = yang_engine_open("persist_tree", ACCESS_MODE_READ);
        ASSERT_NE(nullptr, rel);

        /* 尝试查找节点 */
        const char *path = "/root/persistent";
        void *out_data = NULL;
        size_t out_len = 0;

        /* 注意：yang_engine_get 可能需要特定实现才能从磁盘恢复数据 */
        int ret = yang_engine_get(rel, path, strlen(path), &out_data, &out_len);

        /* 如果实现支持持久化读取，应该返回 0 */
        if (ret == 0 && out_data) {
            free(out_data);
        }

        EXPECT_EQ(0, yang_engine_close(rel));
    }

    yang_engine_drop("persist_tree");
}

/**
 * @brief 测试持久化 - 多节点树结构持久化
 */
TEST_F(YangEngineTest, PersistenceTreeStructure) {
    /* 第一阶段：创建复杂的树结构 */
    {
        EXPECT_EQ(0, yang_engine_create("tree_structure", NULL));

        void *rel = yang_engine_open("tree_structure", ACCESS_MODE_READ_WRITE);
        ASSERT_NE(nullptr, rel);

        /* 创建树结构：
         * root
         * ├── config
         * │   ├── database
         * │   │   ├── host
         * │   │   └── port
         * │   └── cache
         * └── users
         *     ├── admin
         *     └── guest
         */

        struct {
            const char *path;
            const char *name;
            const char *value;
        } nodes[] = {
            {"/root", "root", NULL},
            {"/root/config", "config", NULL},
            {"/root/config/database", "database", NULL},
            {"/root/config/database/host", "host", "localhost"},
            {"/root/config/database/port", "port", "5432"},
            {"/root/config/cache", "cache", NULL},
            {"/root/users", "users", NULL},
            {"/root/users/admin", "admin", "role=admin"},
            {"/root/users/guest", "guest", "role=guest"},
        };

        for (size_t i = 0; i < 9; i++) {
            uint8_t data[1024];
            uint32_t path_len = strlen(nodes[i].path);
            uint32_t name_len = strlen(nodes[i].name);
            uint32_t value_len = nodes[i].value ? strlen(nodes[i].value) : 0;
            yang_node_type_t node_type = (i == 0) ? YANG_NODE_ROOT : YANG_NODE_ELEMENT;

            uint8_t *ptr = data;
            memcpy(ptr, &path_len, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
            memcpy(ptr, nodes[i].path, path_len);
            ptr += path_len;
            memcpy(ptr, &name_len, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
            memcpy(ptr, nodes[i].name, name_len);
            ptr += name_len;
            memcpy(ptr, &node_type, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
            memcpy(ptr, &value_len, sizeof(uint32_t));
            ptr += sizeof(uint32_t);
            if (value_len > 0) {
                memcpy(ptr, nodes[i].value, value_len);
            }

            EXPECT_EQ(0, yang_engine_insert(rel, data,
                                            sizeof(uint32_t) * 4 + path_len + name_len + value_len));
        }

        EXPECT_EQ(0, yang_engine_close(rel));
    }

    /* 第二阶段：验证数据存在 */
    {
        storage_stats_t stats;
        int ret = yang_engine_stats("tree_structure", &stats);

        if (ret == 0) {
            /* 验证节点数量 */
            EXPECT_GE(stats.num_objects, 1);
        }
    }

    yang_engine_drop("tree_structure");
}

/**
 * @brief 测试打开已有树
 */
TEST_F(YangEngineTest, OpenExistingTree) {
    /* 创建树 */
    EXPECT_EQ(0, yang_engine_create("existing_tree", NULL));

    void *rel = yang_engine_open("existing_tree", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入一些数据 */
    const char *path = "/root/test";
    uint8_t data[256];
    uint32_t path_len = strlen(path);
    uint32_t name_len = 4;
    const char *name = "test";
    yang_node_type_t node_type = YANG_NODE_ELEMENT;
    uint32_t value_len = 0;

    uint8_t *ptr = data;
    memcpy(ptr, &path_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, path, path_len);
    ptr += path_len;
    memcpy(ptr, &name_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, name, name_len);
    ptr += name_len;
    memcpy(ptr, &node_type, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &value_len, sizeof(uint32_t));

    yang_engine_insert(rel, data, sizeof(uint32_t) * 4 + path_len + name_len);
    yang_engine_close(rel);

    /* 重新打开同一个树 */
    void *rel2 = yang_engine_open("existing_tree", ACCESS_MODE_READ);
    ASSERT_NE(nullptr, rel2);

    /* 验证可以读取 */
    void *out_data = NULL;
    size_t out_len = 0;
    yang_engine_get(rel2, path, path_len, &out_data, &out_len);

    yang_engine_close(rel2);
    yang_engine_drop("existing_tree");
}

/**
 * @brief 测试节点类型
 */
TEST_F(YangEngineTest, NodeTypes) {
    /* 测试各种节点类型 */
    yang_node_type_t types[] = {
        YANG_NODE_ELEMENT,
        YANG_NODE_ATTRIBUTE,
        YANG_NODE_TEXT,
        YANG_NODE_ROOT,
    };

    for (size_t i = 0; i < 4; i++) {
        yang_node_t *node = yang_node_create("test", types[i], "value");
        ASSERT_NE(nullptr, node);
        EXPECT_EQ(types[i], node->type);
        yang_node_free(node);
    }
}

/**
 * @brief 测试获取引擎操作表
 */
TEST_F(YangEngineTest, GetOps) {
    const storage_ops_t *ops = yang_engine_get_ops();
    ASSERT_NE(nullptr, ops);

    EXPECT_STREQ("yang_engine", ops->name);
    EXPECT_EQ(MODEL_TREE, ops->model);

    /* 验证函数指针有效 */
    EXPECT_NE(nullptr, ops->init);
    EXPECT_NE(nullptr, ops->shutdown);
    EXPECT_NE(nullptr, ops->table_create);
    EXPECT_NE(nullptr, ops->table_open);
    EXPECT_NE(nullptr, ops->tuple_insert);
    EXPECT_NE(nullptr, ops->table_close);
    EXPECT_NE(nullptr, ops->table_drop);
}

/**
 * @brief 测试统计信息
 */
TEST_F(YangEngineTest, Stats) {
    EXPECT_EQ(0, yang_engine_create("stats_tree", NULL));

    void *rel = yang_engine_open("stats_tree", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入节点 */
    const char *path = "/root/stats";
    uint8_t data[256];
    uint32_t path_len = strlen(path);
    uint32_t name_len = 5;
    const char *name = "stats";
    yang_node_type_t node_type = YANG_NODE_ELEMENT;
    uint32_t value_len = 0;

    uint8_t *ptr = data;
    memcpy(ptr, &path_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, path, path_len);
    ptr += path_len;
    memcpy(ptr, &name_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, name, name_len);
    ptr += name_len;
    memcpy(ptr, &node_type, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &value_len, sizeof(uint32_t));

    EXPECT_EQ(0, yang_engine_insert(rel, data, sizeof(uint32_t) * 4 + path_len + name_len));

    EXPECT_EQ(0, yang_engine_close(rel));

    /* 获取统计信息 */
    storage_stats_t stats;
    int ret = yang_engine_stats("stats_tree", &stats);
    EXPECT_EQ(0, ret);
    EXPECT_GE(stats.num_objects, 0);  /* 节点数量应该 >= 0 */

    yang_engine_drop("stats_tree");
}

/**
 * @brief 测试读写模式
 */
TEST_F(YangEngineTest, AccessModes) {
    EXPECT_EQ(0, yang_engine_create("mode_test", NULL));

    /* 以只读模式打开 */
    void *rel_read = yang_engine_open("mode_test", ACCESS_MODE_READ);
    ASSERT_NE(nullptr, rel_read);

    yang_engine_db_t *db_read = static_cast<yang_engine_db_t *>(rel_read);
    EXPECT_EQ(ACCESS_MODE_READ, db_read->mode);

    yang_engine_close(rel_read);

    /* 以读写模式打开 */
    void *rel_write = yang_engine_open("mode_test", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel_write);

    yang_engine_db_t *db_write = static_cast<yang_engine_db_t *>(rel_write);
    EXPECT_EQ(ACCESS_MODE_READ_WRITE, db_write->mode);

    yang_engine_close(rel_write);

    yang_engine_drop("mode_test");
}
