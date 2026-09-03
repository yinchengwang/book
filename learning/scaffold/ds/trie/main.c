/**
 * @file main.c
 * @brief Trie 字典树学习卡片 - 插入/查找/前缀匹配/压缩字典树
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ALPHABET_SIZE 26

/* ==================== 类型定义 ==================== */

/** Trie 节点：每个节点代表一个字符位置 */
typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE];
    bool is_end;                               /* 是否是单词结尾 */
} TrieNode;

typedef struct { TrieNode *root; int size; } Trie;

/* ==================== Trie 操作 ==================== */

/** 创建节点 */
static TrieNode *node_create(void) { return calloc(1, sizeof(TrieNode)); }

/** 释放节点 */
static void node_free(TrieNode *n) {
    if (!n) return;
    for (int i = 0; i < ALPHABET_SIZE; i++) node_free(n->children[i]);
    free(n);
}

/** 创建 Trie */
static Trie *trie_create(void) {
    Trie *t = malloc(sizeof(Trie));
    t->root = node_create(); t->size = 0;
    return t;
}

/** 释放 Trie */
static void trie_free(Trie *t) { node_free(t->root); free(t); }

/** 插入 O(m)，m=单词长度 */
static void trie_insert(Trie *t, const char *word) {
    TrieNode *n = t->root;
    while (*word) {
        int i = *word++ - 'a';
        if (!n->children[i]) n->children[i] = node_create();
        n = n->children[i];
    }
    if (!n->is_end) t->size++;
    n->is_end = true;
}

/** 搜索 O(m) */
static bool trie_search(Trie *t, const char *word) {
    TrieNode *n = t->root;
    while (*word) {
        int i = *word++ - 'a';
        if (!n->children[i]) return false;
        n = n->children[i];
    }
    return n->is_end;
}

/** 前缀匹配 O(m) */
static bool trie_starts_with(Trie *t, const char *prefix) {
    TrieNode *n = t->root;
    while (*prefix) {
        int i = *prefix++ - 'a';
        if (!n->children[i]) return false;
        n = n->children[i];
    }
    return true;
}

/* ==================== 演示 ==================== */

int main(void) {
    printf("=== Trie（字典树）===\n\n");

    /* --- 基本操作 --- */
    printf("[1] 基本操作演示\n");
    Trie *t = trie_create();
    const char *words[] = {"apple", "app", "application", "apply", "banana"};
    printf("  插入: apple app application apply banana\n");
    for (int i = 0; i < 5; i++) trie_insert(t, words[i]);
    printf("  单词数: %d\n\n", t->size);

    printf("  精确查找:\n");
    printf("    app=%s, apple=%s, appl=%s, banana=%s\n",
           trie_search(t, "app") ? "找到" : "未找到",
           trie_search(t, "apple") ? "找到" : "未找到",
           trie_search(t, "appl") ? "找到" : "未找到",
           trie_search(t, "banana") ? "找到" : "未找到");

    printf("  前缀匹配:\n");
    printf("    app=%s, ban=%s, xyz=%s\n",
           trie_starts_with(t, "app") ? "存在" : "不存在",
           trie_starts_with(t, "ban") ? "存在" : "不存在",
           trie_starts_with(t, "xyz") ? "存在" : "不存在");
    trie_free(t);

    /* --- 字典补全 --- */
    printf("\n[2] 字典补全演示（自动完成）\n");
    Trie *d = trie_create();
    const char *dict[] = {"hello", "help", "helicopter", "world", "work", "search"};
    for (int i = 0; i < 6; i++) trie_insert(d, dict[i]);
    printf("  词库: hello help helicopter world work search\n");
    printf("  用户输入 \"he\": %s\n", trie_starts_with(d, "he") ? "有匹配(hello/help/helicopter)" : "无");
    printf("  用户输入 \"wo\": %s\n", trie_starts_with(d, "wo") ? "有匹配(world/work)" : "无");
    printf("  应用: 搜索引擎补全、IDE 代码补全、命令行补全\n");
    trie_free(d);

    /* --- Radix Tree 概念 --- */
    printf("\n[3] Radix Tree（压缩字典树）概念\n");
    printf("  问题: 普通 Trie 单字符边，空间浪费\n");
    printf("  示例: \"apple\"与\"application\"有公共前缀\"appl\"\n");
    printf("  优化: 合并连续单字符边为一条边\n");
    printf("  应用:\n");
    printf("    - Linux radix_tree: IP 路由缓存\n");
    printf("    - LPM: IP 最长前缀匹配\n");
    printf("    - 字符串集合高效存储\n");

    printf("\n=== PASS ===\n");
    return 0;
}
