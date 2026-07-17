/**
 * @file main.c
 * @brief 字符串算法：KMP 模式匹配、Trie 树
 *
 * 演示内容：
 * 1. KMP 算法 - O(n+m) 线性匹配
 * 2. Trie 树 - O(m) 前缀查询
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * 数据结构定义
 * ══════════════════════════════════════════════════════════════════════ */

/** Trie 树节点 */
typedef struct TrieNode {
    int child[26];          /* 子节点索引（0 表示不存在） */
    bool is_end;            /* 是否为单词结尾 */
    int count;              /* 以此为结尾的单词计数 */
} TrieNode;

/** Trie 树结构 */
typedef struct {
    TrieNode *nodes;        /* 节点数组 */
    int node_count;         /* 已分配节点数 */
    int capacity;           /* 容量上限 */
} Trie;

/* ══════════════════════════════════════════════════════════════════════
 * KMP 算法
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 计算模式串的失败函数（next 数组）
 * @param pattern 模式串
 * @param m 模式串长度
 * @param next next 数组
 */
static void compute_next(const char *pattern, int m, int *next)
{
    next[0] = -1;
    int j = -1;

    for (int i = 1; i < m; i++) {
        while (j >= 0 && pattern[i] != pattern[j + 1]) {
            j = next[j];
        }
        if (pattern[i] == pattern[j + 1]) {
            j++;
        }
        next[i] = j;
    }
}

/**
 * KMP 模式匹配
 * @param text 主串
 * @param pattern 模式串
 * @return 首次匹配的位置，-1 表示未找到
 */
static int kmp_match(const char *text, const char *pattern)
{
    int n = (int)strlen(text);
    int m = (int)strlen(pattern);

    if (m == 0) return 0;
    if (m > n) return -1;

    int *next = (int *)malloc((size_t)m * sizeof(int));
    compute_next(pattern, m, next);

    int j = -1;
    for (int i = 0; i < n; i++) {
        while (j >= 0 && text[i] != pattern[j + 1]) {
            j = next[j];
        }
        if (text[i] == pattern[j + 1]) {
            j++;
        }
        if (j == m - 1) {
            free(next);
            return i - m + 1;  /* 找到匹配 */
        }
    }

    free(next);
    return -1;  /* 未找到 */
}

/* ══════════════════════════════════════════════════════════════════════
 * Trie 树
 * ══════════════════════════════════════════════════════════════════════ */

/** 创建 Trie 树 */
static Trie *trie_create(int capacity)
{
    Trie *t = (Trie *)malloc(sizeof(Trie));
    t->capacity = capacity;
    t->node_count = 1;  /* 根节点 */
    t->nodes = (TrieNode *)calloc((size_t)capacity, sizeof(TrieNode));
    memset(t->nodes[0].child, 0, sizeof(t->nodes[0].child));
    t->nodes[0].is_end = false;
    return t;
}

/** 释放 Trie 树 */
static void trie_free(Trie *t)
{
    free(t->nodes);
    free(t);
}

/**
 * 插入单词到 Trie
 * @param t Trie 树
 * @param word 单词
 */
static void trie_insert(Trie *t, const char *word)
{
    int node = 0;
    int len = (int)strlen(word);

    for (int i = 0; i < len; i++) {
        int idx = word[i] - 'a';
        if (idx < 0 || idx >= 26) continue;  /* 跳过非字母 */

        if (t->nodes[node].child[idx] == 0) {
            t->nodes[node].child[idx] = t->node_count++;
        }
        node = t->nodes[node].child[idx];
    }

    t->nodes[node].is_end = true;
    t->nodes[node].count++;
}

/**
 * 搜索 Trie 中的单词
 * @param t Trie 树
 * @param word 单词
 * @return 出现次数，0 表示不存在
 */
static int trie_search(Trie *t, const char *word)
{
    int node = 0;
    int len = (int)strlen(word);

    for (int i = 0; i < len; i++) {
        int idx = word[i] - 'a';
        if (idx < 0 || idx >= 26) return 0;

        if (t->nodes[node].child[idx] == 0) {
            return 0;
        }
        node = t->nodes[node].child[idx];
    }

    return t->nodes[node].is_end ? t->nodes[node].count : 0;
}

/**
 * 前缀匹配：查找以 prefix 为前缀的单词数
 * @param t Trie 树
 * @param prefix 前缀
 * @return 是否存在此前缀的单词
 */
static bool trie_has_prefix(Trie *t, const char *prefix)
{
    int node = 0;
    int len = (int)strlen(prefix);

    for (int i = 0; i < len; i++) {
        int idx = prefix[i] - 'a';
        if (idx < 0 || idx >= 26) return false;

        if (t->nodes[node].child[idx] == 0) {
            return false;
        }
        node = t->nodes[node].child[idx];
    }

    return true;
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示函数
 * ══════════════════════════════════════════════════════════════════════ */

/** 演示 KMP 算法 */
static void demo_kmp(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: KMP 模式匹配\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    const char *text = "ABABDABACDABABCABAB";
    const char *patterns[] = {"ABABCABAB", "ACD", "ABAB"};
    int pattern_count = sizeof(patterns) / sizeof(patterns[0]);

    printf("  主串: \"%s\"\n\n", text);

    for (int i = 0; i < pattern_count; i++) {
        int pos = kmp_match(text, patterns[i]);
        if (pos >= 0) {
            printf("  模式 \"%s\": 在位置 %d 处匹配\n", patterns[i], pos);
        } else {
            printf("  模式 \"%s\": 未找到匹配\n", patterns[i]);
        }
    }

    /* 性能对比：暴力匹配 vs KMP */
    printf("\n  next 数组示例 (模式 \"ABABCABAB\"):\n    ");
    int m = (int)strlen("ABABCABAB");
    int *next = (int *)malloc((size_t)m * sizeof(int));
    compute_next("ABABCABAB", m, next);
    printf("i:    ");
    for (int i = 0; i < m; i++) printf(" %d", i);
    printf("\n    pattern: ");
    for (int i = 0; i < m; i++) printf(" %c", "ABABCABAB"[i]);
    printf("\n    next:    ");
    for (int i = 0; i < m; i++) printf(" %d", next[i]);
    printf("\n");
    free(next);
}

/** 演示 Trie 树 */
static void demo_trie(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: Trie 树（前缀树）\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    const char *words[] = {"apple", "app", "application", "apply", "banana", "band", "bandana"};
    int word_count = sizeof(words) / sizeof(words[0]);

    Trie *t = trie_create(256);
    printf("  插入单词: ");
    for (int i = 0; i < word_count; i++) {
        printf("%s ", words[i]);
        trie_insert(t, words[i]);
    }
    printf("\n  节点数: %d\n", t->node_count);

    printf("\n  搜索测试:\n");
    const char *search_words[] = {"app", "apple", "apply", "ban", "bandana", "xyz"};
    for (int i = 0; i < 6; i++) {
        int count = trie_search(t, search_words[i]);
        printf("    \"%s\": %s\n", search_words[i], count > 0 ? "存在" : "不存在");
    }

    printf("\n  前缀匹配:\n");
    const char *prefixes[] = {"ap", "ban", "cat", "band"};
    for (int i = 0; i < 4; i++) {
        bool found = trie_has_prefix(t, prefixes[i]);
        printf("    \"%s\"* : %s\n", prefixes[i], found ? "有匹配" : "无匹配");
    }

    trie_free(t);
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          字符串算法: KMP 与 Trie 树                             ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_kmp();
    demo_trie();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  算法复杂度:\n");
    printf("    • KMP:   O(n+m) 时间，O(m) 空间\n");
    printf("    • Trie:  O(m) 插入/查询（m=单词长度）\n");
    printf("  空间复杂度:\n");
    printf("    • Trie:  O(字符集大小 × 总字符数)\n");
    printf("  BM 算法（简介）:\n");
    printf("    • 从模式串末尾向前匹配，利用坏字符/好后缀规则\n");
    printf("    • 最坏 O(nm)，实际通常比 KMP 快\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
