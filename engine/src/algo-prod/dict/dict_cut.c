#include "dict_private.h"

static int dict_collect_runes(const char *text,
                                   int32_t byte_start,
                                   int32_t byte_end,
                                   dict_rune_t **runes,
                                   int32_t *rune_count)
{
    dict_rune_t *items;
    int32_t count = 0;
    int32_t capacity = 0;
    int32_t cursor;

    if (!text || byte_start < 0 || byte_end < byte_start || !runes || !rune_count) {
        return -1;
    }

    items = NULL;
    /* 先把 UTF-8 字节流拆�?rune 视图，后续动态规划都基于 rune 下标而不是字节偏移�?*/
    for (cursor = byte_start; cursor < byte_end;) {
        dict_rune_t rune;
        int32_t byte_length;

        if (dict_utf8_decode_one(text + cursor, byte_end - cursor, &rune.codepoint, &byte_length) != 0 || byte_length <= 0) {
            free(items);
            return -1;
        }
        if (count >= capacity) {
            int32_t new_capacity = capacity > 0 ? capacity * 2 : 8;
            dict_rune_t *new_items = (dict_rune_t *)realloc(items, (size_t)new_capacity * sizeof(dict_rune_t));
            if (!new_items) {
                free(items);
                return -1;
            }
            items = new_items;
            capacity = new_capacity;
        }
        rune.byte_start = cursor;
        rune.byte_length = byte_length;
        items[count] = rune;
        count += 1;
        cursor += byte_length;
    }

    *runes = items;
    *rune_count = count;
    return 0;
}

static int dict_build_route(const dict_t *dict,
                                 const dict_rune_t *runes,
                                 int32_t rune_count,
                                 dict_route_t *route)
{
    int32_t i;
    double total_freq;

    if (!dict || !runes || rune_count < 0 || !route) {
        return -1;
    }

    total_freq = dict->total_freq > 0 ? (double)dict->total_freq : 1.0;
    route[rune_count].score = 0.0;
    route[rune_count].next_index = rune_count;

    /*
     * 从右往左做动态规划�?
     * route[i] 表示“从�?i �?rune 开始切，能够得到的最优后缀得分和下一跳位置”�?
     */
    for (i = rune_count - 1; i >= 0; --i) {
        const dict_node_t *node = dict->root;
        double best_score = -DBL_MAX;
        int32_t best_next = i + 1;
        int32_t j;
        bool matched = false;

        /* �?trie 向后尝试所有可能成词的前缀，并挑选总分最高的一条路径�?*/
        for (j = i; j < rune_count; ++j) {
            const dict_edge_t *edge = dict_find_edge(node, runes[j].codepoint);
            double score;

            if (!edge) {
                break;
            }
            node = edge->child;
            if (!node->is_word) {
                continue;
            }

            matched = true;
            score = log((double)node->freq) - log(total_freq) + route[j + 1].score;
            if (score > best_score) {
                best_score = score;
                best_next = j + 1;
            }
        }

        /* 没有词典命中时退化为单字切分�?*/
        if (!matched) {
            best_score = log(1.0 / total_freq) + route[i + 1].score;
        }

        route[i].score = best_score;
        route[i].next_index = best_next;
    }

    return 0;
}

static int dict_segment_cjk(const dict_t *dict,
                                 const char *text,
                                 int32_t byte_start,
                                 int32_t byte_end,
                                 token_t **tokens,
                                 int32_t *token_count,
                                 int32_t *token_capacity)
{
    dict_rune_t *runes;
    dict_route_t *route;
    int32_t rune_count;
    int32_t i;

    if (!dict || !text || byte_end <= byte_start || !tokens || !token_count || !token_capacity) {
        return -1;
    }

    runes = NULL;
    rune_count = 0;
    if (dict_collect_runes(text, byte_start, byte_end, &runes, &rune_count) != 0) {
        return -1;
    }

    route = (dict_route_t *)malloc((size_t)(rune_count + 1) * sizeof(dict_route_t));
    if (!route) {
        free(runes);
        return -1;
    }

    /* 第一�? 生成最优切分路径�?*/
    if (dict_build_route(dict, runes, rune_count, route) != 0) {
        free(route);
        free(runes);
        return -1;
    }

    i = 0;
    /* 第二�? �?route 还原 token；OOV 片段交给 HMM 处�?*/
    while (i < rune_count) {
        /* HMM 未登录词识别：连续 Trie 无法匹配的字符交给 Viterbi 解码 */
        if (dict->hmm_enabled && dict_find_edge(dict->root, runes[i].codepoint) == NULL) {
            int32_t oov_start = i;

            while (i < rune_count && dict_find_edge(dict->root, runes[i].codepoint) == NULL) {
                i += 1;
            }
            {
                int32_t oov_count = i - oov_start;
                int32_t *labels;

                labels = (int32_t *)malloc((size_t)oov_count * sizeof(int32_t));
                if (labels) {
                    if (dict_hmm_viterbi(&dict->hmm_model, &runes[oov_start], oov_count, labels) == 0) {
                        dict_hmm_segment(text, &runes[oov_start], labels, oov_count,
                                         tokens, token_count, token_capacity);
                    }
                    free(labels);
                }
            }
            continue;
        }

        {
            int32_t next_index = route[i].next_index;
            int32_t token_byte_start = runes[i].byte_start;
            int32_t token_byte_end;

            if (next_index <= i || next_index > rune_count) {
                next_index = i + 1;
            }
            token_byte_end = runes[next_index - 1].byte_start + runes[next_index - 1].byte_length;
            if (dict_append_normalized_token(dict,
                                                  tokens,
                                                  token_count,
                                                  token_capacity,
                                                  text,
                                                  token_byte_start,
                                                  token_byte_end - token_byte_start,
                                                  false) != 0) {
                free(route);
                free(runes);
                return -1;
            }
            i = next_index;
        }
    }

    free(route);
    free(runes);
    return 0;
}

/* 判断 (byte_start, byte_length) 是否已在 token 数组中 */
static bool dict_token_exists(const token_t *tokens, int32_t token_count,
                              int32_t byte_start, int32_t byte_length)
{
    int32_t i;

    for (i = 0; i < token_count; ++i) {
        if (tokens[i].byte_start == byte_start && tokens[i].byte_length == byte_length) {
            return true;
        }
    }
    return false;
}

int dict_expand_subwords(const dict_t *dict,
                         const char *text,
                         const dict_rune_t *runes,
                         int32_t rune_count,
                         token_t **tokens,
                         int32_t *token_count,
                         int32_t *token_capacity)
{
    int32_t i;

    if (!dict || !text || !runes || rune_count <= 0 || !tokens || !token_count || !token_capacity) {
        return -1;
    }

    /* 以每个 rune 位置为起点，往前走 Trie 枚举所有成词前缀 */
    for (i = 0; i < rune_count; ++i) {
        const dict_node_t *node = dict->root;
        int32_t j;

        for (j = i; j < rune_count; ++j) {
            const dict_edge_t *edge;

            edge = dict_find_edge(node, runes[j].codepoint);
            if (!edge) {
                break;
            }
            node = edge->child;
            if (!node->is_word) {
                continue;
            }

            /* 找到�?��词 → 去重后输出 */
            {
                int32_t byte_start = runes[i].byte_start;
                int32_t byte_end = runes[j].byte_start + runes[j].byte_length;
                int32_t byte_length = byte_end - byte_start;

                if (!dict_token_exists(*tokens, *token_count, byte_start, byte_length)) {
                    if (dict_append_normalized_token(dict,
                                                     tokens,
                                                     token_count,
                                                     token_capacity,
                                                     text,
                                                     byte_start,
                                                     byte_length,
                                                     false) != 0) {
                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}

int dict_segment_cjk_for_search(const dict_t *dict,
                                const char *text,
                                int32_t byte_start,
                                int32_t byte_end,
                                token_t **tokens,
                                int32_t *token_count,
                                int32_t *token_capacity)
{
    dict_rune_t *runes;
    int32_t rune_count;

    if (!dict || !text || byte_end <= byte_start || !tokens || !token_count || !token_capacity) {
        return -1;
    }

    runes = NULL;
    rune_count = 0;
    if (dict_collect_runes(text, byte_start, byte_end, &runes, &rune_count) != 0) {
        return -1;
    }

    if (dict_expand_subwords(dict, text, runes, rune_count, tokens, token_count, token_capacity) != 0) {
        free(runes);
        return -1;
    }

    /* HMM OOV 补充：对 Trie 无覆盖的字符序列做 Viterbi 解码 */
    if (dict->hmm_enabled) {
        int32_t i = 0;
        while (i < rune_count) {
            if (dict_find_edge(dict->root, runes[i].codepoint) == NULL) {
                int32_t oov_start = i;

                while (i < rune_count && dict_find_edge(dict->root, runes[i].codepoint) == NULL) {
                    i += 1;
                }
                {
                    int32_t oov_count = i - oov_start;
                    int32_t *labels;

                    labels = (int32_t *)malloc((size_t)oov_count * sizeof(int32_t));
                    if (labels) {
                        if (dict_hmm_viterbi(&dict->hmm_model, &runes[oov_start], oov_count, labels) == 0) {
                            dict_hmm_segment(text, &runes[oov_start], labels, oov_count,
                                             tokens, token_count, token_capacity);
                        }
                        free(labels);
                    }
                }
            } else {
                i += 1;
            }
        }
    }

    free(runes);
    return 0;
}

int32_t dict_cut_for_search(const dict_t *dict, const char *text, token_t **tokens, int32_t *token_count)
{
    token_t *items;
    int32_t count;
    int32_t capacity;
    int32_t length;
    int32_t cursor;

    if (!dict || !text || !tokens || !token_count) {
        return -1;
    }

    items = NULL;
    count = 0;
    capacity = 0;
    length = (int32_t)strlen(text);

    /*
     * 主循环与 dict_cut() 结构相同，区别在于 CJK 段使用 "全词枚举" 而非 "最优路径 DP"。
     * 1. 分隔�? 直接跳过
     * 2. ASCII 连续�? 当作英文/数字词处�?
     * 3. CJK 连续�? 所有 Trie 匹配子词全部输出
     */
    for (cursor = 0; cursor < length;) {
        uint32_t codepoint;
        int32_t byte_length;

        if (dict_utf8_decode_one(text + cursor, length - cursor, &codepoint, &byte_length) != 0 || byte_length <= 0) {
            dict_free_tokens(items, count);
            return -1;
        }

        if (dict_is_delimiter(codepoint)) {
            cursor += byte_length;
            continue;
        }

        if (dict_is_ascii_word_char(codepoint)) {
            int32_t begin = cursor;

            cursor += byte_length;
            while (cursor < length) {
                if (dict_utf8_decode_one(text + cursor, length - cursor, &codepoint, &byte_length) != 0 ||
                    byte_length <= 0 || !dict_is_ascii_word_char(codepoint)) {
                    break;
                }
                cursor += byte_length;
            }
            if (dict_append_normalized_token(dict, &items, &count, &capacity, text, begin, cursor - begin, true) != 0) {
                dict_free_tokens(items, count);
                return -1;
            }
            continue;
        }

        if (dict_is_cjk(codepoint)) {
            int32_t begin = cursor;

            cursor += byte_length;
            while (cursor < length) {
                if (dict_utf8_decode_one(text + cursor, length - cursor, &codepoint, &byte_length) != 0 ||
                    byte_length <= 0 || !dict_is_cjk(codepoint)) {
                    break;
                }
                cursor += byte_length;
            }
            /* 搜索模式：枚举所有词典子词而非常规最优路径 */
            if (dict_segment_cjk_for_search(dict, text, begin, cursor, &items, &count, &capacity) != 0) {
                dict_free_tokens(items, count);
                return -1;
            }
            continue;
        }

        if (dict_append_normalized_token(dict, &items, &count, &capacity, text, cursor, byte_length, false) != 0) {
            dict_free_tokens(items, count);
            return -1;
        }
        cursor += byte_length;
    }

    *tokens = items;
    *token_count = count;
    return 0;
}

int32_t dict_cut(const dict_t *dict, const char *text, token_t **tokens, int32_t *token_count)
{
    token_t *items;
    int32_t count;
    int32_t capacity;
    int32_t length;
    int32_t cursor;

    if (!dict || !text || !tokens || !token_count) {
        return -1;
    }

    items = NULL;
    count = 0;
    capacity = 0;
    length = (int32_t)strlen(text);

    /*
     * 主循环按字符类型把文本切成三类片�?
     * 1. 分隔�? 直接跳过
     * 2. ASCII 连续�? 当作英文/数字词处�?
     * 3. CJK 连续�? 交给词典 + 动态规划切�?
     */
    for (cursor = 0; cursor < length;) {
        uint32_t codepoint;
        int32_t byte_length;

        if (dict_utf8_decode_one(text + cursor, length - cursor, &codepoint, &byte_length) != 0 || byte_length <= 0) {
            dict_free_tokens(items, count);
            return -1;
        }

        /* 分隔符不产出 token，只负责切断相邻片段�?*/
        if (dict_is_delimiter(codepoint)) {
            cursor += byte_length;
            continue;
        }

        if (dict_is_ascii_word_char(codepoint)) {
            int32_t begin = cursor;

            /* 收集一整段 ASCII 单词，再统一做归一化�?*/
            cursor += byte_length;
            while (cursor < length) {
                if (dict_utf8_decode_one(text + cursor, length - cursor, &codepoint, &byte_length) != 0 ||
                    byte_length <= 0 || !dict_is_ascii_word_char(codepoint)) {
                    break;
                }
                cursor += byte_length;
            }
            if (dict_append_normalized_token(dict, &items, &count, &capacity, text, begin, cursor - begin, true) != 0) {
                dict_free_tokens(items, count);
                return -1;
            }
            continue;
        }

        if (dict_is_cjk(codepoint)) {
            int32_t begin = cursor;

            /* 连续的中日韩字符段交给词典分词�?*/
            cursor += byte_length;
            while (cursor < length) {
                if (dict_utf8_decode_one(text + cursor, length - cursor, &codepoint, &byte_length) != 0 ||
                    byte_length <= 0 || !dict_is_cjk(codepoint)) {
                    break;
                }
                cursor += byte_length;
            }
            if (dict_segment_cjk(dict, text, begin, cursor, &items, &count, &capacity) != 0) {
                dict_free_tokens(items, count);
                return -1;
            }
            continue;
        }

        /* 其他字符按单字符 token 处理�?*/
        if (dict_append_normalized_token(dict, &items, &count, &capacity, text, cursor, byte_length, false) != 0) {
            dict_free_tokens(items, count);
            return -1;
        }
        cursor += byte_length;
    }

    *tokens = items;
    *token_count = count;
    return 0;
}