/**
 * @file main.c
 * @brief 树结构应用示例：表达式树、Huffman树、树状数组
 *
 * 演示三种经典树结构的应用场景：
 * 1. 表达式树 - 用于解析和计算数学表达式
 * 2. Huffman树 - 用于数据压缩和最优编码
 * 3. 树状数组(Fenwick Tree) - 用于高效前缀和查询与点更新
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================
 * 第一部分：表达式树 (Expression Tree)
 * ============================================================ */

/** 表达式树节点 */
typedef struct ExprNode {
    char data;                    // 操作数或操作符
    struct ExprNode *left;        // 左子树
    struct ExprNode *right;       // 右子树
} ExprNode;

/**
 * 根据中缀表达式构建表达式树
 * 输入格式：每个操作数和操作符之间用空格分隔
 * 例如：a + b * c 对应节点结构：
 *        +
 *       / \
 *      a   *
 *         / \
 *        b   c
 */
ExprNode* build_expr_tree(const char **expr) {
    // 跳过空格
    while (**expr == ' ') (*expr)++;

    if (**expr == '\0') return NULL;

    char token = **expr;
    (*expr)++;

    // 创建节点
    ExprNode *node = (ExprNode*)malloc(sizeof(ExprNode));
    node->data = token;
    node->left = NULL;
    node->right = NULL;

    // 如果是操作符，递归构建左右子树
    if (token == '+' || token == '-' || token == '*' || token == '/') {
        node->left = build_expr_tree(expr);
        node->right = build_expr_tree(expr);
    }

    return node;
}

/** 计算表达式树（变量值通过数组传入） */
double eval_expr_tree(ExprNode *node, double *vars) {
    if (!node) return 0;

    // 叶子节点：操作数
    if (node->left == NULL && node->right == NULL) {
        // 假设变量名为小写字母 a-z
        return vars[node->data - 'a'];
    }

    // 内部节点：操作符
    double left_val = eval_expr_tree(node->left, vars);
    double right_val = eval_expr_tree(node->right, vars);

    switch (node->data) {
        case '+': return left_val + right_val;
        case '-': return left_val - right_val;
        case '*': return left_val * right_val;
        case '/': return (right_val != 0) ? left_val / right_val : 0;
        default:  return 0;
    }
}

/** 释放表达式树内存 */
void free_expr_tree(ExprNode *node) {
    if (!node) return;
    free_expr_tree(node->left);
    free_expr_tree(node->right);
    free(node);
}

/** 先序遍历表达式树（前缀表达式） */
void prefix_traversal(ExprNode *node) {
    if (!node) return;
    printf("%c ", node->data);
    prefix_traversal(node->left);
    prefix_traversal(node->right);
}

/* ============================================================
 * 第二部分：Huffman树 (Huffman Tree)
 * ============================================================ */

/** Huffman树节点 */
typedef struct HuffmanNode {
    char ch;                      // 字符（叶子节点使用）
    int freq;                     // 频率/权重
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

/** 优先队列节点（用于构建Huffman树） */
typedef struct PQNode {
    HuffmanNode *tree;
    struct PQNode *next;
} PQNode;

/** 优先队列操作 */
void pq_push(PQNode **head, HuffmanNode *tree) {
    PQNode *node = (PQNode*)malloc(sizeof(PQNode));
    node->tree = tree;
    node->next = NULL;

    // 按频率升序排列
    if (*head == NULL || (*head)->tree->freq > tree->freq) {
        node->next = *head;
        *head = node;
    } else {
        PQNode *cur = *head;
        while (cur->next && cur->next->tree->freq <= tree->freq) {
            cur = cur->next;
        }
        node->next = cur->next;
        cur->next = node;
    }
}

/** 弹出最小频率节点 */
HuffmanNode* pq_pop(PQNode **head) {
    if (*head == NULL) return NULL;
    PQNode *node = *head;
    HuffmanNode *tree = node->tree;
    *head = node->next;
    free(node);
    return tree;
}

/**
 * 根据字符频率构建Huffman树
 * @param freqs 字符频率数组（索引为字符ASCII码）
 * @param size 频率数组大小
 */
HuffmanNode* build_huffman_tree(int *freqs, int size) {
    PQNode *head = NULL;

    // 将所有非零频率的字符加入优先队列
    for (int i = 0; i < size; i++) {
        if (freqs[i] > 0) {
            HuffmanNode *node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
            node->ch = (char)i;
            node->freq = freqs[i];
            node->left = NULL;
            node->right = NULL;
            pq_push(&head, node);
        }
    }

    // 构建Huffman树
    while (head && head->next) {
        HuffmanNode *left = pq_pop(&head);
        HuffmanNode *right = pq_pop(&head);

        HuffmanNode *parent = (HuffmanNode*)malloc(sizeof(HuffmanNode));
        parent->ch = '\0';  // 内部节点无字符
        parent->freq = left->freq + right->freq;
        parent->left = left;
        parent->right = right;

        pq_push(&head, parent);
    }

    return head ? head->tree : NULL;
}

/** 生成Huffman编码 */
void generate_huffman_codes(HuffmanNode *root, char *code, int depth, char codes[128][256]) {
    if (!root) return;

    if (root->left == NULL && root->right == NULL) {
        code[depth] = '\0';
        strcpy(codes[(unsigned char)root->ch], code);
        return;
    }

    code[depth] = '0';
    generate_huffman_codes(root->left, code, depth + 1, codes);

    code[depth] = '1';
    generate_huffman_codes(root->right, code, depth + 1, codes);
}

/** 释放Huffman树内存 */
void free_huffman_tree(HuffmanNode *node) {
    if (!node) return;
    free_huffman_tree(node->left);
    free_huffman_tree(node->right);
    free(node);
}

/* ============================================================
 * 第三部分：树状数组 (Fenwick Tree / Binary Indexed Tree)
 * ============================================================ */

/**
 * 树状数组结构
 * 支持 O(log n) 的单点更新和前缀和查询
 * 核心思想：用树形结构维护部分和
 */
typedef struct FenwickTree {
    int n;        // 数组大小
    int *tree;    // 树状数组（1-based indexing）
} FenwickTree;

/** 初始化树状数组 */
FenwickTree* fenwick_create(int n) {
    FenwickTree *ft = (FenwickTree*)malloc(sizeof(FenwickTree));
    ft->n = n;
    ft->tree = (int*)calloc(n + 1, sizeof(int));  // 1-based
    return ft;
}

/**
 * 在指定位置添加值
 * @param idx 位置（0-based，需转换为1-based）
 * @param delta 要添加的值
 */
void fenwick_update(FenwickTree *ft, int idx, int delta) {
    // 转换为1-based索引
    for (idx++; idx <= ft->n; idx += idx & (-idx)) {
        ft->tree[idx] += delta;
    }
}

/**
 * 查询前缀和 [0, idx]
 * @param idx 位置（0-based）
 */
int fenwick_query(FenwickTree *ft, int idx) {
    int sum = 0;
    // 转换为1-based索引
    for (idx++; idx > 0; idx -= idx & (-idx)) {
        sum += ft->tree[idx];
    }
    return sum;
}

/** 查询区间和 [l, r] */
int fenwick_range_query(FenwickTree *ft, int l, int r) {
    return fenwick_query(ft, r) - fenwick_query(ft, l - 1);
}

/** 释放树状数组内存 */
void fenwick_free(FenwickTree *ft) {
    free(ft->tree);
    free(ft);
}

/* ============================================================
 * 主函数：演示三种树结构
 * ============================================================ */

int main(void) {
    printf("============================================\n");
    printf("     树结构应用示例：表达式树、Huffman、树状数组\n");
    printf("============================================\n\n");

    /* ----- 1. 表达式树演示 ----- */
    printf("【1】表达式树\n");
    printf("-------------------------------------------\n");
    const char *expr1 = "+ a * b c";  // a + b * c
    const char *expr2 = "* + a b c";  // (a + b) * c

    // 示例：计算 a=2, b=3, c=4 时的值
    double vars[26] = {0};
    vars['a' - 'a'] = 2;
    vars['b' - 'a'] = 3;
    vars['c' - 'a'] = 4;

    const char *p1 = expr1;
    ExprNode *root1 = build_expr_tree(&p1);
    printf("表达式 'a + b * c' 前缀表示: ");
    prefix_traversal(root1);
    printf("\n计算结果 (a=2, b=3, c=4): %.1f\n", eval_expr_tree(root1, vars));

    const char *p2 = expr2;
    ExprNode *root2 = build_expr_tree(&p2);
    printf("表达式 '(a + b) * c' 前缀表示: ");
    prefix_traversal(root2);
    printf("\n计算结果 (a=2, b=3, c=4): %.1f\n", eval_expr_tree(root2, vars));

    free_expr_tree(root1);
    free_expr_tree(root2);
    printf("\n");

    /* ----- 2. Huffman树演示 ----- */
    printf("【2】Huffman树\n");
    printf("-------------------------------------------\n");

    // 统计字符串中字符频率
    const char *text = "aabbbcccc";
    int freqs[128] = {0};
    for (int i = 0; text[i]; i++) {
        freqs[(unsigned char)text[i]]++;
    }

    printf("输入文本: \"%s\"\n", text);
    printf("字符频率统计:\n");
    for (int i = 0; i < 128; i++) {
        if (freqs[i] > 0) {
            printf("  '%c': %d\n", (char)i, freqs[i]);
        }
    }

    // 构建Huffman树
    HuffmanNode *huffman = build_huffman_tree(freqs, 128);

    // 生成编码
    char codes[128][256] = {0};
    char code[256];
    generate_huffman_codes(huffman, code, 0, codes);

    printf("\nHuffman编码:\n");
    for (int i = 0; i < 128; i++) {
        if (codes[i][0] != '\0') {
            printf("  '%c': %s\n", (char)i, codes[i]);
        }
    }

    // 计算压缩后的总位数
    int total_bits = 0;
    for (int i = 0; text[i]; i++) {
        total_bits += strlen(codes[(unsigned char)text[i]]);
    }
    printf("\n原始大小: %zu bits (每个字符8位)\n", strlen(text) * 8);
    printf("压缩后:   %d bits\n", total_bits);
    printf("压缩率:   %.1f%%\n", 100.0 * total_bits / (strlen(text) * 8));

    free_huffman_tree(huffman);
    printf("\n");

    /* ----- 3. 树状数组演示 ----- */
    printf("【3】树状数组 (Fenwick Tree)\n");
    printf("-------------------------------------------\n");

    int data[] = {1, 3, 5, 7, 9, 11};
    int n = sizeof(data) / sizeof(data[0]);

    FenwickTree *ft = fenwick_create(n);

    // 构建树状数组（逐个添加元素）
    printf("原始数组: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", data[i]);
        fenwick_update(ft, i, data[i]);
    }
    printf("\n\n");

    // 查询演示
    printf("前缀和查询:\n");
    for (int i = 0; i < n; i++) {
        printf("  sum[0..%d] = %d\n", i, fenwick_query(ft, i));
    }

    printf("\n区间和查询:\n");
    printf("  sum[1..4] = %d\n", fenwick_range_query(ft, 1, 4));
    printf("  sum[2..5] = %d\n", fenwick_range_query(ft, 2, 5));

    // 单点更新
    printf("\n单点更新: 将第3个元素(+5)增加到第4个元素\n");
    fenwick_update(ft, 2, 5);  // data[2] += 5
    printf("  sum[0..5] = %d (原为 %d)\n", fenwick_query(ft, 5), 1+3+5+7+9+11);

    fenwick_free(ft);
    printf("\n");

    printf("============================================\n");
    printf("                演示结束\n");
    printf("============================================\n");

    return 0;
}
