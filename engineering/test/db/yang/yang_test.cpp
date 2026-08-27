/**
 * @file yang_test.cpp
 * @brief YANG 模型与 NETCONF 协议单元测试
 *
 * 覆盖：
 *   - YANG 模型解析（module/container/leaf/list/typedef/grouping）
 *   - 数据节点创建/路径访问/值转换
 *   - Datastore 管理（running/startup/candidate）
 *   - NETCONF RPC（get/get-config/edit-config/copy-config/delete-config）
 */
#include <gtest/gtest.h>
#include "db/yang/yang_model.h"
#include "db/yang/yang_data.h"
#include "db/netconf/netconf_server.h"
#include <cstring>
#include <cstdio>

/* 辅助宏：填充 yang_data_node_t 字符串字段 */
#define YANG_SET_STR(dst, src) strncpy((dst), (src), sizeof(dst) - 1)

/* ============================================================
 * 辅助宏
 * ============================================================ */

#define YANG_TEST_MODEL_TEXT                                          \
    "module network {\n"                                              \
    "  container interfaces {\n"                                      \
    "    list interface {\n"                                          \
    "      key \"name\";\n"                                           \
    "      leaf name { type string; mandatory true; }\n"             \
    "      leaf mtu { type uint16; default \"1500\"; }\n"             \
    "      leaf enabled { type boolean; default \"true\"; }\n"        \
    "    }\n"                                                         \
    "  }\n"                                                           \
    "  container system {\n"                                          \
    "    leaf hostname { type string; mandatory true; }\n"            \
    "    leaf domain { type string; }\n"                              \
    "  }\n"                                                           \
    "}\n"

/* ============================================================
 * YANG 模型解析测试
 * ============================================================ */

TEST(YangModelTest, ParseBasicModule) {
    yang_model_t *m = yang_model_create("test");
    ASSERT_NE(m, nullptr);

    const char *src = "module example { container x { leaf y { type string; } } }";
    EXPECT_EQ(yang_model_parse(m, src, 0), 0);
    EXPECT_STREQ(m->module_name, "example");
    EXPECT_GT(m->node_count, 1u);

    yang_model_free(m);
}

TEST(YangModelTest, ParseFullText) {
    yang_model_t *m = yang_model_create("network");
    ASSERT_NE(m, nullptr);

    EXPECT_EQ(yang_model_parse(m, YANG_TEST_MODEL_TEXT, 0), 0);

    /* 查找 interfaces 容器 */
    yang_schema_node_t *interfaces = yang_schema_find(m->root, "/network/interfaces");
    ASSERT_NE(interfaces, nullptr);
    EXPECT_EQ(interfaces->kind, YANG_SCHEMA_CONTAINER);

    /* 查找 interface 列表 */
    yang_schema_node_t *iface = yang_schema_find(m->root, "/network/interfaces/interface");
    ASSERT_NE(iface, nullptr);
    EXPECT_EQ(iface->kind, YANG_SCHEMA_LIST);
    EXPECT_STREQ(iface->keys[0], "name");

    /* 查找 mtu leaf */
    yang_schema_node_t *mtu = yang_schema_find(m->root,
        "/network/interfaces/interface/mtu");
    ASSERT_NE(mtu, nullptr);
    EXPECT_EQ(mtu->kind, YANG_SCHEMA_LEAF);
    EXPECT_EQ(mtu->value_type, YANG_TYPE_UINT16);
    EXPECT_STREQ(mtu->default_value, "1500");

    yang_model_free(m);
}

TEST(YangModelTest, ParseLeafTypes) {
    yang_model_t *m = yang_model_create("types");
    ASSERT_NE(m, nullptr);

    const char *src =
        "module types {\n"
        "  leaf a { type int8; }\n"
        "  leaf b { type int32; }\n"
        "  leaf c { type uint64; }\n"
        "  leaf d { type boolean; }\n"
        "  leaf e { type string; }\n"
        "}";
    EXPECT_EQ(yang_model_parse(m, src, 0), 0);

    EXPECT_EQ(yang_schema_find(m->root, "/types/a")->value_type, YANG_TYPE_INT8);
    EXPECT_EQ(yang_schema_find(m->root, "/types/b")->value_type, YANG_TYPE_INT32);
    EXPECT_EQ(yang_schema_find(m->root, "/types/c")->value_type, YANG_TYPE_UINT64);
    EXPECT_EQ(yang_schema_find(m->root, "/types/d")->value_type, YANG_TYPE_BOOLEAN);
    EXPECT_EQ(yang_schema_find(m->root, "/types/e")->value_type, YANG_TYPE_STRING);

    yang_model_free(m);
}

/* ============================================================
 * YANG 数据节点测试
 * ============================================================ */

TEST(YangDataTest, CreateAndFindNode) {
    yang_data_node_t *root = yang_data_node_create("network",
                                                   YANG_KIND_CONTAINER,
                                                   YANG_TYPE_EMPTY);
    ASSERT_NE(root, nullptr);

    /* 添加 interfaces 容器 */
    yang_data_node_t *ifs = yang_data_node_create("interfaces",
                                                  YANG_KIND_CONTAINER,
                                                  YANG_TYPE_EMPTY);
    EXPECT_EQ(yang_data_add_child(root, ifs), 0);

    /* 添加 list 项 */
    yang_data_node_t *iface = yang_data_node_create("interface",
                                                    YANG_KIND_LIST,
                                                    YANG_TYPE_EMPTY);
    YANG_SET_STR(iface->keys[0], "name");
    EXPECT_EQ(yang_data_add_child(ifs, iface), 0);

    /* 添加 leaf 子节点 */
    yang_data_node_t *name_leaf = yang_data_node_create("name",
                                                        YANG_KIND_LEAF,
                                                        YANG_TYPE_STRING);
    YANG_SET_STR(name_leaf->value, "eth0");
    EXPECT_EQ(yang_data_add_child(iface, name_leaf), 0);

    /* 查找 */
    EXPECT_EQ(yang_data_find_child(root, "interfaces"), ifs);
    EXPECT_EQ(yang_data_find_child(ifs, "interface"), iface);

    yang_data_node_free(root);
}

TEST(YangDataTest, PathGetAndCreate) {
    yang_data_node_t *root = yang_data_node_create("root",
                                                   YANG_KIND_CONTAINER,
                                                   YANG_TYPE_EMPTY);
    ASSERT_NE(root, nullptr);

    yang_data_node_t *node = yang_data_create_node(root,
                                                   "/a/b/c",
                                                   YANG_KIND_LEAF);
    ASSERT_NE(node, nullptr);
    EXPECT_STREQ(node->name, "c");

    /* 再次获取：返回已存在节点 */
    yang_data_node_t *again = yang_data_create_node(root, "/a/b/c",
                                                    YANG_KIND_LEAF);
    EXPECT_EQ(again, node);

    yang_data_node_free(root);
}

TEST(YangDataTest, ListKeyPredicate) {
    yang_data_node_t *root = yang_data_node_create("network",
                                                   YANG_KIND_CONTAINER,
                                                   YANG_TYPE_EMPTY);
    yang_data_node_t *ifs = yang_data_node_create("interfaces",
                                                  YANG_KIND_CONTAINER,
                                                  YANG_TYPE_EMPTY);
    yang_data_add_child(root, ifs);

    /* 创建 list 项 iface[name=eth0] */
    yang_data_node_t *iface = yang_data_create_node(root,
        "/interfaces/interface[name='eth0']", YANG_KIND_LIST);
    ASSERT_NE(iface, nullptr);

    /* 创建同名的另一项 */
    yang_data_node_t *iface2 = yang_data_create_node(root,
        "/interfaces/interface[name='eth1']", YANG_KIND_LIST);
    ASSERT_NE(iface2, nullptr);

    /* 通过谓词访问 */
    yang_data_node_t *got = yang_data_get_node(root, "/interfaces/interface[name='eth0']");
    EXPECT_EQ(got, iface);

    yang_data_node_free(root);
}

TEST(YangDataTest, ValueTypeConversion) {
    yang_data_node_t *leaf = yang_data_node_create("x",
                                                   YANG_KIND_LEAF,
                                                   YANG_TYPE_INT32);
    EXPECT_EQ(yang_data_set_value_from_string(leaf, "12345"), 0);
    EXPECT_STREQ(leaf->value, "12345");

    /* 越界 */
    EXPECT_NE(yang_data_set_value_from_string(leaf, "999999999999"), 0);

    /* boolean */
    yang_data_node_t *bf = yang_data_node_create("flag",
                                                 YANG_KIND_LEAF,
                                                 YANG_TYPE_BOOLEAN);
    EXPECT_EQ(yang_data_set_value_from_string(bf, "true"), 0);
    EXPECT_STREQ(bf->value, "true");
    EXPECT_EQ(yang_data_set_value_from_string(bf, "yes"), -1);

    yang_data_node_free(leaf);
    yang_data_node_free(bf);
}

TEST(YangDataTest, CloneSubtree) {
    yang_data_node_t *root = yang_data_node_create("a",
                                                   YANG_KIND_CONTAINER,
                                                   YANG_TYPE_EMPTY);
    yang_data_node_t *leaf = yang_data_node_create("b",
                                                   YANG_KIND_LEAF,
                                                   YANG_TYPE_STRING);
    YANG_SET_STR(leaf->value, "hello");
    yang_data_add_child(root, leaf);

    yang_data_node_t *copy = yang_data_clone(root);
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy, root);
    EXPECT_STREQ(copy->first_child->name, "b");
    EXPECT_STREQ(copy->first_child->value, "hello");

    yang_data_node_free(root);
    yang_data_node_free(copy);
}

/* ============================================================
 * Datastore 测试
 * ============================================================ */

TEST(YangDatastoreTest, CreateAndClear) {
    yang_datastore_t *ds = yang_datastore_create("running");
    ASSERT_NE(ds, nullptr);
    EXPECT_NE(yang_datastore_root(ds), nullptr);

    EXPECT_EQ(yang_datastore_clear(ds), 0);
    EXPECT_NE(yang_datastore_root(ds), nullptr);

    yang_datastore_free(ds);
}

/* ============================================================
 * NETCONF 会话与 RPC 测试
 * ============================================================ */

TEST(NetconfTest, SessionCreate) {
    netconf_session_t *s = netconf_session_create("sess-001");
    ASSERT_NE(s, nullptr);
    EXPECT_STREQ(s->session_id, "sess-001");

    EXPECT_NE(netconf_session_get_datastore(s, "running"), nullptr);
    EXPECT_NE(netconf_session_get_datastore(s, "startup"), nullptr);
    EXPECT_NE(netconf_session_get_datastore(s, "candidate"), nullptr);
    EXPECT_EQ(netconf_session_get_datastore(s, "invalid"), nullptr);

    netconf_session_free(s);
}

TEST(NetconfTest, XmlSerializeBasic) {
    yang_datastore_t *ds = yang_datastore_create("test");
    yang_data_node_t *root = yang_datastore_root(ds);

    yang_data_node_t *ifs = yang_data_node_create("interfaces",
                                                  YANG_KIND_CONTAINER,
                                                  YANG_TYPE_EMPTY);
    yang_data_add_child(root, ifs);

    yang_data_node_t *name_leaf = yang_data_node_create("hostname",
                                                        YANG_KIND_LEAF,
                                                        YANG_TYPE_STRING);
    YANG_SET_STR(name_leaf->value, "router1");
    yang_data_add_child(ifs, name_leaf);

    char buf[4096];
    int n = netconf_xml_serialize(root, buf, sizeof(buf));
    EXPECT_GT(n, 0);
    EXPECT_NE(strstr(buf, "<interfaces>"), nullptr);
    EXPECT_NE(strstr(buf, "<hostname>router1</hostname>"), nullptr);

    yang_datastore_free(ds);
}

TEST(NetconfTest, EditConfigMergeOp) {
    netconf_session_t *s = netconf_session_create("sess-edit");
    ASSERT_NE(s, nullptr);

    /* 构造 edit-config RPC */
    const char *rpc =
        "<rpc message-id=\"1\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<edit-config>"
        "<target><running/></target>"
        "<config>"
        "<hostname>router-A</hostname>"
        "<domain>example.com</domain>"
        "</config>"
        "</edit-config>"
        "</rpc>";
    char reply[4096];
    EXPECT_EQ(netconf_handle_rpc(s, rpc, 0, reply, sizeof(reply)), NETCONF_OK);
    EXPECT_NE(strstr(reply, "<ok/>"), nullptr);

    /* 验证 running datastore 已更新 */
    yang_data_node_t *r = s->running->root;
    yang_data_node_t *host = yang_data_find_child(r, "hostname");
    ASSERT_NE(host, nullptr);
    EXPECT_STREQ(host->value, "router-A");

    netconf_session_free(s);
}

TEST(NetconfTest, GetConfigOperation) {
    netconf_session_t *s = netconf_session_create("sess-get");
    ASSERT_NE(s, nullptr);

    /* 先写入配置 */
    const char *edit_rpc =
        "<rpc message-id=\"1\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<edit-config>"
        "<target><candidate/></target>"
        "<config><leaf1>v1</leaf1><leaf2>v2</leaf2></config>"
        "</edit-config>"
        "</rpc>";
    char reply[4096];
    EXPECT_EQ(netconf_handle_rpc(s, edit_rpc, 0, reply, sizeof(reply)), NETCONF_OK);

    /* get-config 读取 */
    const char *get_rpc =
        "<rpc message-id=\"2\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<get-config><source><candidate/></source></get-config>"
        "</rpc>";
    memset(reply, 0, sizeof(reply));
    EXPECT_EQ(netconf_handle_rpc(s, get_rpc, 0, reply, sizeof(reply)), NETCONF_OK);
    EXPECT_NE(strstr(reply, "<data>"), nullptr);
    EXPECT_NE(strstr(reply, "leaf1"), nullptr);
    EXPECT_NE(strstr(reply, "v1"), nullptr);

    netconf_session_free(s);
}

TEST(NetconfTest, GetOperation) {
    netconf_session_t *s = netconf_session_create("sess-getall");
    ASSERT_NE(s, nullptr);

    /* 编辑 running */
    const char *edit_rpc =
        "<rpc message-id=\"1\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<edit-config>"
        "<target><running/></target>"
        "<config><status>up</status></config>"
        "</edit-config>"
        "</rpc>";
    char reply[4096];
    EXPECT_EQ(netconf_handle_rpc(s, edit_rpc, 0, reply, sizeof(reply)), NETCONF_OK);

    /* get 操作 */
    const char *get_rpc =
        "<rpc message-id=\"2\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<get><filter/></get>"
        "</rpc>";
    memset(reply, 0, sizeof(reply));
    EXPECT_EQ(netconf_handle_rpc(s, get_rpc, 0, reply, sizeof(reply)), NETCONF_OK);
    EXPECT_NE(strstr(reply, "<status>up</status>"), nullptr);

    netconf_session_free(s);
}

TEST(NetconfTest, CopyConfigOperation) {
    netconf_session_t *s = netconf_session_create("sess-copy");
    ASSERT_NE(s, nullptr);

    /* 在 candidate 写入 */
    const char *edit =
        "<rpc message-id=\"1\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<edit-config>"
        "<target><candidate/></target>"
        "<config><keyA>valA</keyA></config>"
        "</edit-config>"
        "</rpc>";
    char reply[4096];
    EXPECT_EQ(netconf_handle_rpc(s, edit, 0, reply, sizeof(reply)), NETCONF_OK);

    /* copy-config: candidate -> running */
    const char *copy =
        "<rpc message-id=\"2\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<copy-config>"
        "<source><candidate/></source>"
        "<target><running/></target>"
        "</copy-config>"
        "</rpc>";
    EXPECT_EQ(netconf_handle_rpc(s, copy, 0, reply, sizeof(reply)), NETCONF_OK);

    /* 验证 running 有 keyA */
    EXPECT_NE(yang_data_find_child(s->running->root, "keyA"), nullptr);

    netconf_session_free(s);
}

TEST(NetconfTest, DeleteConfigOperation) {
    netconf_session_t *s = netconf_session_create("sess-del");
    ASSERT_NE(s, nullptr);

    /* 在 candidate 写入 */
    const char *edit =
        "<rpc message-id=\"1\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<edit-config>"
        "<target><candidate/></target>"
        "<config><x>1</x></config>"
        "</edit-config>"
        "</rpc>";
    char reply[4096];
    EXPECT_EQ(netconf_handle_rpc(s, edit, 0, reply, sizeof(reply)), NETCONF_OK);

    /* delete-config: candidate */
    const char *del =
        "<rpc message-id=\"2\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<delete-config><target><candidate/></target></delete-config>"
        "</rpc>";
    EXPECT_EQ(netconf_handle_rpc(s, del, 0, reply, sizeof(reply)), NETCONF_OK);

    /* 验证 candidate 已清空 */
    EXPECT_EQ(yang_data_find_child(s->candidate->root, "x"), nullptr);

    /* 不能删除 running */
    const char *del_running =
        "<rpc message-id=\"3\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<delete-config><target><running/></target></delete-config>"
        "</rpc>";
    EXPECT_NE(netconf_handle_rpc(s, del_running, 0, reply, sizeof(reply)), NETCONF_OK);

    netconf_session_free(s);
}

TEST(NetconfTest, EditConfigReplaceOp) {
    netconf_session_t *s = netconf_session_create("sess-replace");
    ASSERT_NE(s, nullptr);

    /* 第一次 merge */
    const char *edit1 =
        "<rpc message-id=\"1\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<edit-config>"
        "<target><running/></target>"
        "<default-operation>merge</default-operation>"
        "<config><a>1</a><b>2</b></config>"
        "</edit-config>"
        "</rpc>";
    char reply[4096];
    EXPECT_EQ(netconf_handle_rpc(s, edit1, 0, reply, sizeof(reply)), NETCONF_OK);
    EXPECT_NE(yang_data_find_child(s->running->root, "a"), nullptr);
    EXPECT_NE(yang_data_find_child(s->running->root, "b"), nullptr);

    /* 第二次 replace：清空旧数据，只剩 c */
    const char *edit2 =
        "<rpc message-id=\"2\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<edit-config>"
        "<target><running/></target>"
        "<default-operation>replace</default-operation>"
        "<config><c>3</c></config>"
        "</edit-config>"
        "</rpc>";
    EXPECT_EQ(netconf_handle_rpc(s, edit2, 0, reply, sizeof(reply)), NETCONF_OK);

    /* a/b 应被清空 */
    EXPECT_EQ(yang_data_find_child(s->running->root, "a"), nullptr);
    EXPECT_EQ(yang_data_find_child(s->running->root, "b"), nullptr);
    EXPECT_NE(yang_data_find_child(s->running->root, "c"), nullptr);

    netconf_session_free(s);
}

TEST(NetconfTest, EditConfigDeleteOp) {
    netconf_session_t *s = netconf_session_create("sess-delcfg");
    ASSERT_NE(s, nullptr);

    /* 先写两个 key */
    const char *edit =
        "<rpc message-id=\"1\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<edit-config>"
        "<target><running/></target>"
        "<config><key1>v1</key1><key2>v2</key2></config>"
        "</edit-config>"
        "</rpc>";
    char reply[4096];
    EXPECT_EQ(netconf_handle_rpc(s, edit, 0, reply, sizeof(reply)), NETCONF_OK);

    /* 删除操作 */
    const char *del =
        "<rpc message-id=\"2\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<edit-config>"
        "<target><running/></target>"
        "<config><key1/></config>"
        "</edit-config>"
        "</rpc>";
    EXPECT_EQ(netconf_handle_rpc(s, del, 0, reply, sizeof(reply)), NETCONF_OK);

    EXPECT_EQ(yang_data_find_child(s->running->root, "key1"), nullptr);
    EXPECT_NE(yang_data_find_child(s->running->root, "key2"), nullptr);

    netconf_session_free(s);
}

TEST(YangDataTest, RemoveChild) {
    yang_data_node_t *root = yang_data_node_create("root",
                                                   YANG_KIND_CONTAINER,
                                                   YANG_TYPE_EMPTY);
    yang_data_node_t *a = yang_data_node_create("a",
                                                 YANG_KIND_LEAF,
                                                 YANG_TYPE_STRING);
    yang_data_node_t *b = yang_data_node_create("b",
                                                 YANG_KIND_LEAF,
                                                 YANG_TYPE_STRING);
    yang_data_add_child(root, a);
    yang_data_add_child(root, b);

    EXPECT_EQ(yang_data_remove_child(root, "a"), 0);
    EXPECT_EQ(yang_data_find_child(root, "a"), nullptr);
    EXPECT_NE(yang_data_find_child(root, "b"), nullptr);

    /* 删除不存在的名字 */
    EXPECT_NE(yang_data_remove_child(root, "zzz"), 0);

    yang_data_node_free(root);
}

TEST(YangDataTest, FormatValue) {
    yang_data_node_t *c = yang_data_node_create("c",
                                                 YANG_KIND_CONTAINER,
                                                 YANG_TYPE_EMPTY);
    char buf[64];
    EXPECT_EQ(yang_data_format_value(c, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "");

    yang_data_node_t *leaf = yang_data_node_create("v",
                                                   YANG_KIND_LEAF,
                                                   YANG_TYPE_INT32);
    YANG_SET_STR(leaf->value, "42");
    EXPECT_GT(yang_data_format_value(leaf, buf, sizeof(buf)), 0);
    EXPECT_STREQ(buf, "42");

    yang_data_node_free(c);
    yang_data_node_free(leaf);
}

TEST(NetconfTest, UnknownRpcOperation) {
    netconf_session_t *s = netconf_session_create("sess-unk");
    ASSERT_NE(s, nullptr);

    const char *rpc =
        "<rpc message-id=\"1\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
        "<unknown-op/>"
        "</rpc>";
    char reply[4096];
    EXPECT_NE(netconf_handle_rpc(s, rpc, 0, reply, sizeof(reply)), NETCONF_OK);

    netconf_session_free(s);
}

/* 辅助宏：填充 yang_data_node_t 字符串字段 */
#define SET_STR_FOR_TEST(dst, src) strncpy((dst), (src), sizeof(dst) - 1)