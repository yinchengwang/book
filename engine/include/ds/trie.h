/**
 * @file trie.h
 * @brief Trie（前缀树/字典树）统一接口
 *
 * 提供统一的 C API 接口，封装 C++ 实现。
 * 支持插入、搜索、前缀匹配等操作。
 *
 * 时间复杂度：O(L)，其中 L 为字符串长度
 * 空间复杂度：O(ALPHABET_SIZE * L * N)，N 为单词数
 */
#ifndef DS_TRIE_H
#define DS_TRIE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 类型定义
// ============================================================

/** Trie 树 opaque 句柄 */
typedef struct trie_handle trie_t;

/** Trie 搜索结果 */
typedef struct {
    bool exists;       /**< 是否存在 */
    int  prefix_count; /**< 以此前缀开头的单词数 */
} trie_result_t;

// ============================================================
// 生命周期
// ============================================================

/**
 * @brief 创建 Trie 树
 * @return Trie 句柄，失败返回 NULL
 */
trie_t *trie_create(void);

/**
 * @brief 销毁 Trie 树
 * @param trie Trie 句柄
 */
void trie_destroy(trie_t *trie);

// ============================================================
// 操作 API（统一使用 snake_case 命名）
// ============================================================

/**
 * @brief 插入单词
 * @param trie Trie 句柄
 * @param word 要插入的单词
 * @return 成功返回 0，失败返回 -1
 */
int trie_insert(trie_t *trie, const char *word);

/**
 * @brief 精确搜索：word 是否完整存在于 Trie 中
 * @param trie Trie 句柄
 * @param word 要搜索的单词
 * @return 存在返回 true，否则返回 false
 */
bool trie_search(const trie_t *trie, const char *word);

/**
 * @brief 前缀搜索：是否存在以 prefix 为前缀的单词
 * @param trie Trie 句柄
 * @param prefix 要搜索的前缀
 * @return 存在返回 true，否则返回 false
 */
bool trie_starts_with(const trie_t *trie, const char *prefix);

/**
 * @brief 统计以 prefix 为前缀的单词数量
 * @param trie Trie 句柄
 * @param prefix 要统计的前缀
 * @return 单词数量
 */
int trie_count_prefix(const trie_t *trie, const char *prefix);

/**
 * @brief 删除单词
 * @param trie Trie 句柄
 * @param word 要删除的单词
 * @return 成功返回 0，单词不存在返回 -1
 */
int trie_delete(trie_t *trie, const char *word);

/**
 * @brief 获取树中的单词总数
 * @param trie Trie 句柄
 * @return 单词数量
 */
size_t trie_word_count(const trie_t *trie);

/**
 * @brief Trie 演示函数
 */
void trie_demo(void);

// ============================================================
// C++ 封装兼容接口（已废弃，请使用 snake_case 版本）
// ============================================================

/** @deprecated 请使用 trie_starts_with() */
static inline bool trie_startsWith(const trie_t *trie, const char *prefix) {
    return trie_starts_with(trie, prefix);
}

#ifdef __cplusplus
}
#endif

#endif /* DS_TRIE_H */
