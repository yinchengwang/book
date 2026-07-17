/*
 * dict_hmm_model.c
 *
 * 嵌入式 HMM 模型数据，参数取自 jieba 分词器的 prob_start.py / prob_trans.py。
 * 发射概率使用字符类型启发式规则，避免嵌入超大码点概率表。
 */

#include "dict_private.h"

#include <math.h>

/* jieba HMM 初始概率 (log 空间) */
static const float k_hmm_start_prob[4] = {
    -0.26268660809250016f,   /* B */
    -3.14e+10f,              /* E (不可达) */
    -3.14e+10f,              /* M (不可达) */
    -1.4652633398537678f,    /* S */
};

/* jieba HMM 转移概率矩阵 (log 空间)，trans_prob[from][to] */
static const float k_hmm_trans_prob[4][4] = {
    /* from B */ { -3.14e+10f, -0.510825623765990f, -0.916290731874155f,  -3.14e+10f },
    /* from E */ { -0.5897149736854513f, -3.14e+10f, -3.14e+10f, -0.8085250474669937f },
    /* from M */ { -3.14e+10f, -0.33344856811948514f, -1.2603623820268226f, -3.14e+10f },
    /* from S */ { -0.7211965654669841f, -3.14e+10f, -3.14e+10f, -0.6658631448798212f },
};

void dict_hmm_model_init_default(dict_hmm_model_t *model)
{
    int i, j;

    if (!model) return;
    for (i = 0; i < 4; ++i) {
        model->start_prob[i] = k_hmm_start_prob[i];
        for (j = 0; j < 4; ++j) {
            model->trans_prob[i][j] = k_hmm_trans_prob[i][j];
        }
    }
}

/*
 * 发射概率启发式：
 * CJK 汉字在 B/M/E 状态有较高概率（倾向多字成词），S 状态概率较低。
 * 非 CJK 字符倾向 S 状态（单字成词）。
 * 概率值基于 jieba HMM 模型中对 CJK 字符的平均行为估算。
 */
float dict_hmm_emit_prob(uint32_t codepoint, dict_hmm_state_t state)
{
    bool is_cjk_char;

    /* 忽略大小写的发射概率因子 */
    (void)codepoint;
    is_cjk_char = dict_is_cjk(codepoint);

    switch (state) {
    case DICT_HMM_STATE_B:
        return is_cjk_char ? -0.5f : -5.0f;
    case DICT_HMM_STATE_E:
        return is_cjk_char ? -0.5f : -5.0f;
    case DICT_HMM_STATE_M:
        return is_cjk_char ? -0.3f : -10.0f;
    case DICT_HMM_STATE_S:
        return is_cjk_char ? -1.8f : -0.2f;
    default:
        return -10.0f;
    }
}
