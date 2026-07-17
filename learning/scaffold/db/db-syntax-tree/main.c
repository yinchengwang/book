/**
 * @file main.c
 * @brief 语法树 (AST) 演示：节点结构 + 遍历 + 打印
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * AST 节点类型
 * ======================================================================== */

typedef enum {
    NODE_SELECT,
    NODE_INSERT,
    NODE_UPDATE,
    NODE_DELETE,
    NODE_COLUMN_REF,
    NODE_TABLE_REF,
    NODE_CONST,
    NODE_BINOP,
    NODE_UNARYOP,
    NODE_FUNCALL,
    NODE_LIST
} NodeType;

typedef struct AstNode {
    NodeType type;
    char *value;                  // 节点值
    struct AstNode **children;    // 子节点
    int nchildren;
} AstNode;

/* ========================================================================
 * AST 节点操作
 * ======================================================================== */

AstNode *node_new(NodeType type, const char *value) {
    AstNode *n = calloc(1, sizeof(AstNode));
    n->type = type;
    n->value = value ? strdup(value) : NULL;
    return n;
}

void node_add_child(AstNode *parent, AstNode *child) {
    parent->nchildren++;
    parent->children = realloc(parent->children,
                              parent->nchildren * sizeof(AstNode *));
    parent->children[parent->nchildren - 1] = child;
}

const char *node_type_name(NodeType t) {
    switch (t) {
        case NODE_SELECT: return "SELECT";
        case NODE_INSERT: return "INSERT";
        case NODE_COLUMN_REF: return "COLUMN";
        case NODE_TABLE_REF: return "TABLE";
        case NODE_CONST: return "CONST";
        case NODE_BINOP: return "BINOP";
        default: return "NODE";
    }
}

void node_print(AstNode *n, int indent) {
    if (!n) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("[%s]", node_type_name(n->type));
    if (n->value) printf(" '%s'", n->value);
    printf("\n");
    for (int i = 0; i < n->nchildren; i++) {
        node_print(n->children[i], indent + 1);
    }
}

/* ========================================================================
 * 遍历器 (Visitor Pattern)
 * ======================================================================== */

typedef void (*VisitFunc)(AstNode *);

void visit_preorder(AstNode *n, VisitFunc visit) {
    if (!n) return;
    visit(n);
    for (int i = 0; i < n->nchildren; i++) {
        visit_preorder(n->children[i], visit);
    }
}

void visit_postorder(AstNode *n, VisitFunc visit) {
    if (!n) return;
    for (int i = 0; i < n->nchildren; i++) {
        visit_postorder(n->children[i], visit);
    }
    visit(n);
}

void visit_inorder(AstNode *n, VisitFunc visit) {
    if (!n) return;
    if (n->nchildren > 0) visit_inorder(n->children[0], visit);
    visit(n);
    for (int i = 1; i < n->nchildren; i++) {
        visit_inorder(n->children[i], visit);
    }
}

/* ========================================================================
 * 访问函数示例
 * ======================================================================== */

int count_nodes = 0;

void count_visit(AstNode *n) {
    count_nodes++;
    printf("  访问: %s\n", node_type_name(n->type));
}

void collect_columns_visit(AstNode *n) {
    if (n->type == NODE_COLUMN_REF) {
        printf("  发现列: %s\n", n->value);
    }
}

/* ========================================================================
 * AST 转换
 * ======================================================================== */

void transform_add_alias(AstNode *select) {
    printf("\n[AST 转换] 为所有列添加别名\n");
    // 遍历所有 COLUMN_REF 节点，添加 alias
}

void transform_inline_view(AstNode *view) {
    printf("\n[AST 转换] 内联视图定义\n");
    // 展开视图子查询
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    printf("=== 语法树 (AST) 演示 ===\n\n");

    /* 构建 AST */
    printf("--- 1. 构建 AST ---\n");
    printf("SQL: SELECT id, name FROM users WHERE age > 18\n\n");

    AstNode *select = node_new(NODE_SELECT, NULL);

    // SELECT 列
    AstNode *columns = node_new(NODE_LIST, "columns");
    node_add_child(columns, node_new(NODE_COLUMN_REF, "id"));
    node_add_child(columns, node_new(NODE_COLUMN_REF, "name"));
    node_add_child(select, columns);

    // FROM 表
    AstNode *table = node_new(NODE_TABLE_REF, "users");
    node_add_child(select, table);

    // WHERE 条件
    AstNode *where = node_new(NODE_BINOP, ">");
    AstNode *left = node_new(NODE_COLUMN_REF, "age");
    AstNode *right = node_new(NODE_CONST, "18");
    node_add_child(where, left);
    node_add_child(where, right);
    node_add_child(select, where);

    printf("AST 结构:\n");
    node_print(select, 0);

    /* 遍历演示 */
    printf("\n--- 2. 遍历 AST (Pre-order) ---\n");
    count_nodes = 0;
    visit_preorder(select, count_visit);
    printf("总节点数: %d\n", count_nodes);

    printf("\n--- 3. 查找所有列引用 ---\n");
    visit_preorder(select, collect_columns_visit);

    printf("\n--- 4. AST 转换 ---\n");
    transform_add_alias(select);
    printf("  (实际应用中会修改 AST 结构)\n");

    printf("\n=== 演示完成 ===\n");
    return 0;
}