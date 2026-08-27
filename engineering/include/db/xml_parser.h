/**
 * @file xml_parser.h
 * @brief 自研 XML 解析器（递归下降，C2-5 T1）
 *
 * 支持元素、属性、命名空间前缀、自闭合标签、文本、注释。
 * 不依赖 libxml2/expat，零外部依赖。
 */
#ifndef DB_XML_PARSER_H
#define DB_XML_PARSER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AST 节点类型 */
typedef enum {
    XML_NODE_ELEMENT,    /**< 元素 */
    XML_NODE_TEXT,       /**< 文本 */
} xml_node_type_t;

typedef struct xml_attr_s {
    char *name;            /**< 属性名（可含 prefix:name） */
    char *value;           /**< 属性值 */
    char *ns_uri;          /**< 命名空间 URI（可 NULL） */
} xml_attr_t;

typedef struct xml_node_s xml_node_t;
typedef struct xml_attr_s xml_attr_t;

struct xml_node_s {
    xml_node_type_t type;
    char *name;            /**< 元素名（child::name 用） */
    char *ns_prefix;       /**< 命名空间前缀（NULL = 默认命名空间） */
    char *ns_uri;          /**< 命名空间 URI */
    char *text;            /**< TEXT 节点内容（NULL = 元素） */
    xml_attr_t *attrs;     /**< 属性数组（NULL 终止） */
    size_t n_attrs;
    xml_node_t **children; /**< 子节点（NULL 终止） */
    size_t n_children;
};

/* 解析 */
xml_node_t *xml_parse(const char *src, size_t len);
void xml_tree_free(xml_node_t *root);

/* XPath 子集（C2-5 T7） */
typedef struct xml_xpath_result_s {
    xml_node_t **nodes;
    size_t n_nodes;
} xml_xpath_result_t;

/**
 * @brief XPath 子集求值：仅支持 /a/b/c 绝对路径 + child::name + [attr='v'] 谓词
 */
xml_xpath_result_t xml_xpath_eval(const xml_node_t *root, const char *path);
void xml_xpath_result_free(xml_xpath_result_t *r);

#ifdef __cplusplus
}
#endif

#endif /* DB_XML_PARSER_H */