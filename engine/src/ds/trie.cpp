/**
 * @file trie.cpp
 * @brief Trie（前缀树）C++ 实现
 *
 * 提供统一的 trie 接口实现，封装 unordered_map 存储。
 */
#include "ds/trie.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <stack>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 内部类型
// ============================================================

struct TrieNode {
    std::unordered_map<char, TrieNode *> children;
    bool is_end{false};
    int prefix_count{0};

    ~TrieNode() {
        for (auto &p : children) {
            delete p.second;
        }
    }
};

struct trie_handle {
    TrieNode *root;
    size_t word_count;

    trie_handle() : root(new TrieNode()), word_count(0) {}
    ~trie_handle() { delete root; }
};

// ============================================================
// 生命周期实现
// ============================================================

trie_t *trie_create(void) {
    return new trie_handle();
}

void trie_destroy(trie_t *trie) {
    delete trie;
}

// ============================================================
// 辅助函数
// ============================================================

static TrieNode *find_node(const trie_t *trie, const char *prefix) {
    TrieNode *node = trie->root;
    for (const char *p = prefix; *p; ++p) {
        auto it = node->children.find(*p);
        if (it == node->children.end()) {
            return nullptr;
        }
        node = it->second;
    }
    return node;
}

// ============================================================
// 操作 API 实现
// ============================================================

int trie_insert(trie_t *trie, const char *word) {
    if (!trie || !word) return -1;

    TrieNode *node = trie->root;
    for (const char *p = word; *p; ++p) {
        if (node->children.find(*p) == node->children.end()) {
            node->children[*p] = new TrieNode();
        }
        node = node->children[*p];
        node->prefix_count++;
    }

    if (!node->is_end) {
        node->is_end = true;
        trie->word_count++;
    }
    return 0;
}

bool trie_search(const trie_t *trie, const char *word) {
    if (!trie || !word) return false;
    TrieNode *node = find_node(trie, word);
    return node && node->is_end;
}

bool trie_starts_with(const trie_t *trie, const char *prefix) {
    if (!trie || !prefix) return false;
    return find_node(trie, prefix) != nullptr;
}

int trie_count_prefix(const trie_t *trie, const char *prefix) {
    if (!trie || !prefix) return 0;
    TrieNode *node = find_node(trie, prefix);
    return node ? node->prefix_count : 0;
}

int trie_delete(trie_t *trie, const char *word) {
    if (!trie || !word) return -1;

    // 使用栈记录路径，标记要删除的节点
    std::stack<std::pair<TrieNode *, char>> path;
    TrieNode *node = trie->root;

    for (const char *p = word; *p; ++p) {
        auto it = node->children.find(*p);
        if (it == node->children.end()) {
            return -1;  // 单词不存在
        }
        path.push({node, *p});
        node = it->second;
        node->prefix_count--;
    }

    if (!node->is_end) {
        return -1;  // 单词不存在
    }

    node->is_end = false;
    trie->word_count--;

    // 清理无用的叶子节点（可选优化）
    // 这里简化处理，不做完整清理
    return 0;
}

size_t trie_word_count(const trie_t *trie) {
    return trie ? trie->word_count : 0;
}

// ============================================================
// 演示函数
// ============================================================

void trie_demo(void) {
    trie_t *trie = trie_create();

    // 插入测试
    trie_insert(trie, "hello");
    trie_insert(trie, "he");
    trie_insert(trie, "her");
    trie_insert(trie, "hi");
    trie_insert(trie, "how");
    trie_insert(trie, "world");

    printf("Trie 演示:\n");
    printf("  单词数: %zu\n", trie_word_count(trie));

    // 搜索测试
    printf("  search(\"hello\"): %s\n", trie_search(trie, "hello") ? "true" : "false");
    printf("  search(\"hell\"): %s\n", trie_search(trie, "hell") ? "true" : "false");

    // 前缀测试
    printf("  starts_with(\"he\"): %s\n", trie_starts_with(trie, "he") ? "true" : "false");
    printf("  starts_with(\"hr\"): %s\n", trie_starts_with(trie, "hr") ? "true" : "false");

    // 前缀计数
    printf("  count_prefix(\"he\"): %d\n", trie_count_prefix(trie, "he"));
    printf("  count_prefix(\"h\"): %d\n", trie_count_prefix(trie, "h"));

    trie_destroy(trie);
}

#ifdef __cplusplus
}
#endif
