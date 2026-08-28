/**
 * @file xml_parser_test.cpp
 * @brief C2-5 T2 标准 XML 用例测试
 */
#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/xml_parser.h"
}

TEST(XmlParser, ElementWithAttributes) {
    /* 标准 XML 含属性、命名空间前缀、嵌套 */
    const char *xml =
        "<root xmlns:ns=\"http://example.com/ns\">"
        "<ns:item attr=\"value\">text</ns:item>"
        "</root>";
    xml_node_t *root = xml_parse(xml, strlen(xml));
    ASSERT_NE(root, nullptr);
    EXPECT_STREQ(root->name, "root");
    EXPECT_EQ(root->n_children, 1u);
    if (root->n_children >= 1) {
        xml_node_t *item = root->children[0];
        EXPECT_STREQ(item->name, "item");
        EXPECT_STREQ(item->ns_prefix, "ns");
        EXPECT_GE(item->n_attrs, 1u);
        if (item->n_attrs >= 1) {
            EXPECT_STREQ(item->attrs[0].name, "attr");
            EXPECT_STREQ(item->attrs[0].value, "value");
        }
    }
    xml_tree_free(root);
}

TEST(XmlParser, SelfClosingTag) {
    const char *xml = "<root><br/><hr/></root>";
    xml_node_t *root = xml_parse(xml, strlen(xml));
    ASSERT_NE(root, nullptr);
    EXPECT_STREQ(root->name, "root");
    EXPECT_EQ(root->n_children, 2u);
    if (root->n_children == 2) {
        EXPECT_STREQ(root->children[0]->name, "br");
        EXPECT_STREQ(root->children[1]->name, "hr");
    }
    xml_tree_free(root);
}

TEST(XmlParser, NestedElements) {
    const char *xml = "<a><b><c>deep</c></b></a>";
    xml_node_t *root = xml_parse(xml, strlen(xml));
    ASSERT_NE(root, nullptr);
    EXPECT_STREQ(root->name, "a");
    ASSERT_GE(root->n_children, 1u);
    if (root->n_children >= 1) {
        xml_node_t *b = root->children[0];
        EXPECT_STREQ(b->name, "b");
        ASSERT_GE(b->n_children, 1u);
        if (b->n_children >= 1) {
            xml_node_t *c = b->children[0];
            EXPECT_STREQ(c->name, "c");
            ASSERT_EQ(c->n_children, 1u);
            if (c->n_children == 1) {
                EXPECT_STREQ(c->children[0]->text, "deep");
            }
        }
    }
    xml_tree_free(root);
}

TEST(XmlXPath, AbsolutePath) {
    const char *xml = "<root><a><b>x</b></a><a><b>y</b></a></root>";
    xml_node_t *root = xml_parse(xml, strlen(xml));
    ASSERT_NE(root, nullptr);
    xml_xpath_result_t r = xml_xpath_eval(root, "/root/a/b");
    EXPECT_EQ(r.n_nodes, 2u);
    xml_xpath_result_free(&r);
    xml_tree_free(root);
}