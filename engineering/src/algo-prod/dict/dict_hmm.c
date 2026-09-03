/*
 * dict_hmm.c
 *
 * HMM Viterbi 解码器，用于未登录词（OOV）识别。
 * 对连续的在 Trie 中无匹配的 CJK 字符序列执行 4 状态 (B/M/E/S) 序列标注。
 */

#include "dict_private.h"

/* Viterbi 解码：对 rune 序列进行 B/M/E/S 状态标注 */
int dict_hmm_viterbi(const dict_hmm_model_t *model, const dict_rune_t *runes,
                     int32_t rune_count, int32_t *labels)
{
    float *dp;
    int32_t *backptr;
    int32_t t, s;
    int32_t best_last;

    if (!model || !runes || rune_count <= 0 || !labels) {
        return -1;
    }

    /* dp[t * 4 + s] = 到位置 t 的状�?s 的最优对数概率 */
    dp = (float *)malloc((size_t)(rune_count) * 4 * sizeof(float));
    backptr = (int32_t *)malloc((size_t)(rune_count) * 4 * sizeof(int32_t));
    if (!dp || !backptr) {
        free(dp);
        free(backptr);
        return -1;
    }

    /* 初始�? t=0 */
    for (s = 0; s < 4; ++s) {
        dp[s] = model->start_prob[s] + dict_hmm_emit_prob(runes[0].codepoint, (dict_hmm_state_t)s);
        backptr[s] = -1;
    }

    /* 递推 */
    for (t = 1; t < rune_count; ++t) {
        int32_t offset = t * 4;
        int32_t prev_offset = (t - 1) * 4;
        float emit[4];

        for (s = 0; s < 4; ++s) {
            emit[s] = dict_hmm_emit_prob(runes[t].codepoint, (dict_hmm_state_t)s);
        }

        for (s = 0; s < 4; ++s) {
            float best_score = -1e30f;
            int32_t best_from = 0;
            int32_t prev_s;

            for (prev_s = 0; prev_s < 4; ++prev_s) {
                float score = dp[prev_offset + prev_s] + model->trans_prob[prev_s][s] + emit[s];
                if (score > best_score) {
                    best_score = score;
                    best_from = prev_s;
                }
            }
            dp[offset + s] = best_score;
            backptr[offset + s] = best_from;
        }
    }

    /* 终止：找最后位置的最优状�?*/
    {
        int32_t last_offset = (rune_count - 1) * 4;
        best_last = 0;
        for (s = 1; s < 4; ++s) {
            if (dp[last_offset + s] > dp[last_offset + best_last]) {
                best_last = s;
            }
        }
    }

    /* 回溯 */
    {
        int32_t cur = best_last;
        for (t = rune_count - 1; t >= 0; --t) {
            labels[t] = cur;
            cur = backptr[t * 4 + cur];
        }
    }

    free(dp);
    free(backptr);
    return 0;
}

/*
 * 从 B/M/E/S 标签序列提取词边界，输出为 token 列表。
 * B 开始 → E 结束为一个多字词；S 独立成单字词。
 */
int dict_hmm_segment(const char *text, const dict_rune_t *runes,
                     const int32_t *labels, int32_t rune_count,
                     token_t **tokens, int32_t *token_count, int32_t *token_capacity)
{
    int32_t i = 0;

    if (!text || !runes || !labels || rune_count <= 0 || !tokens || !token_count || !token_capacity) {
        return -1;
    }

    while (i < rune_count) {
        if (labels[i] == DICT_HMM_STATE_S) {
            /* 单字成词 */
            if (dict_append_token(tokens, token_count, token_capacity,
                                  text, runes[i].byte_start, runes[i].byte_length, false) != 0) {
                return -1;
            }
            i += 1;
        } else if (labels[i] == DICT_HMM_STATE_B) {
            /* B 开始，找到下个 E 结�?*/
            int32_t start = i;
            i += 1;
            while (i < rune_count && labels[i] != DICT_HMM_STATE_E) {
                i += 1;
            }
            /* i 现在指向 E（或越界，此时回退到最后一�?*/
            if (i >= rune_count) {
                i = rune_count - 1;
            }
            {
                int32_t byte_start = runes[start].byte_start;
                int32_t byte_end = runes[i].byte_start + runes[i].byte_length;
                if (dict_append_token(tokens, token_count, token_capacity,
                                      text, byte_start, byte_end - byte_start, false) != 0) {
                    return -1;
                }
            }
            i += 1;
        } else {
            /* M 或 E 开头（异常），按单字处理 */
            if (dict_append_token(tokens, token_count, token_capacity,
                                  text, runes[i].byte_start, runes[i].byte_length, false) != 0) {
                return -1;
            }
            i += 1;
        }
    }

    return 0;
}
