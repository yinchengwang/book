/*
 * trie.h —— Trie 树（前缀树 / 字典树）
 *
 * ============================================================
 * 概述
 * ============================================================
 * Trie（发音 "try"）是一种用于高效存储和检索字符串集合的树形结构。
 * 每个节点代表一个字符，从根到某个节点的路径构成一个字符串前缀。
 *
 * Trie 的核心优势：查找、插入的时间复杂度为 O(L)，其中 L 是字符串长度，
 * 与集合中字符串的数量无关。这是哈希表做不到的（哈希表需要计算整个 key
 * 的哈希值，也是 O(L)，但哈希冲突会退化）。
 *
 * 本实现以"小写英文字母 a-z"为例（每个节点最多 26 个子节点）。
 *
 * Trie 结构示例（包含 "he", "hello", "hi", "her"）：
 *
 *                  root
 *                 /    \
 *              'h'     ...
 *              /
 *           'e' ──── 'i'
 *          /  \        \
 *      'r'(终) 'l'     (终)
 *               |
 *              'l'
 *               |
 *              'o'(终)
 *
 * 标记为(终)的节点表示一个完整单词在此结束。
 *
 * ============================================================
 * Trie 变种
 * ============================================================
 * - 压缩 Trie（Radix Tree）：合并只有一个子节点的链，减少空间
 * - AC 自动机：Trie + 失败指针（fail link），用于多模式串匹配
 * - 后缀树：存储所有后缀的压缩 Trie，用于子串查询
 * - 01 Trie：每个节点两个子节点（0/1），用于处理二进制数
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作          | 复杂度 | 说明                 |
 * |--------------|--------|---------------------|
 * | insert       | O(L)   | L = 字符串长度        |
 * | search       | O(L)   | 精确匹配              |
 * | starts_with  | O(L)   | 前缀匹配              |
 * | delete       | O(L)   |                       |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 搜索引擎的自动补全（Autocomplete）
 * - 拼写检查
 * - 字典的快速查找
 * - IP 路由表（最长前缀匹配）
 * - 字符串排序（Trie 的前序遍历天然有序）
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. Trie vs HashMap？→ Trie 支持前缀查找，适合自动补全；HashMap O(1) 但不支持前缀。
 * 2. Trie 的空间优化？→ 用动态数组/map 替代固定大小的子节点数组。
 * 3. 如何用 Trie 实现自动补全？→ 找到前缀节点后，DFS 收集所有带"终点"标记的后代。
 */

#ifndef DS_TRIE_H
#define DS_TRIE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS_TRIE_ALPHABET_SIZE 26u

/* Trie 节点 */
typedef struct ds_trie_node {
    struct ds_trie_node *children[DS_TRIE_ALPHABET_SIZE];
    bool                 is_end; /* 标记是否为一个完整单词的结尾 */
    int                  prefix_count; /* 经过此节点的单词数（用于统计前缀匹配数） */
} ds_trie_node_t;

/* Trie 树 */
typedef struct {
    ds_trie_node_t *root;
    size_t          word_count; /* 树中存储的单词总数 */
} ds_trie_t;

/* 创建/销毁 */
ds_trie_t *ds_trie_create(void);
void ds_trie_destroy(ds_trie_t *trie);

/* 插入单词（只支持小写字母 a-z） */
int ds_trie_insert(ds_trie_t *trie, const char *word);

/* 精确搜索：word 是否在 Trie 中 */
bool ds_trie_search(const ds_trie_t *trie, const char *word);

/* 前缀搜索：是否存在以 prefix 为前缀的单词 */
bool ds_trie_starts_with(const ds_trie_t *trie, const char *prefix);

/* 统计以 prefix 为前缀的单词数量 */
int ds_trie_count_prefix(const ds_trie_t *trie, const char *prefix);

/* 删除单词 */
int ds_trie_delete(ds_trie_t *trie, const char *word);

/* 演示函数 */
void ds_trie_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_TRIE_H */
