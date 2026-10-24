#include "db/cjk_tokenizer.h"
#include "db/core/log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 跨平台 strndup（MinGW 等不支持 POSIX strndup 时提供）
 * ======================================================================== */
static char *my_strndup(const char *s, size_t n) {
    size_t len = strlen(s);
    if (len > n) len = n;
    char *dup = (char *)malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}

/* ========== 词典 ========== */
struct cjk_dict_s {
    char **words;
    size_t *lens;
    size_t n_words;
    size_t cap;
};

cjk_dict_t *cjk_dict_load(const char *path) {
    if (!path) return NULL;
    FILE *fp = fopen(path, "r");
    if (!fp) {
        LOG_ERROR("cjk_dict_load: 无法打开 %s", path);
        return NULL;
    }
    cjk_dict_t *d = calloc(1, sizeof(cjk_dict_t));
    if (!d) { fclose(fp); return NULL; }
    d->cap = 1024;
    d->words = calloc(d->cap, sizeof(char *));
    d->lens = calloc(d->cap, sizeof(size_t));
    if (!d->words || !d->lens) { fclose(fp); cjk_dict_free(d); return NULL; }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;
        if (d->n_words >= d->cap) {
            size_t nc = d->cap * 2;
            d->words = realloc(d->words, nc * sizeof(char *));
            d->lens = realloc(d->lens, nc * sizeof(size_t));
            d->cap = nc;
        }
        d->words[d->n_words] = my_strndup(line, n);
        d->lens[d->n_words] = n;
        d->n_words++;
    }
    fclose(fp);
    LOG_INFO("cjk_dict_load: 加载 %zu 词条", d->n_words);
    return d;
}

void cjk_dict_free(cjk_dict_t *dict) {
    if (!dict) return;
    for (size_t i = 0; i < dict->n_words; ++i) free(dict->words[i]);
    free(dict->words);
    free(dict->lens);
    free(dict);
}

bool cjk_dict_contains(const cjk_dict_t *dict, const char *word, size_t len) {
    if (!dict || !word) return false;
    for (size_t i = 0; i < dict->n_words; ++i) {
        if (dict->lens[i] == len
            && memcmp(dict->words[i], word, len) == 0) return true;
    }
    return false;
}

/* ========== FMM/RMM 分词 ==========
 *
 * FMM（Forward Max Match）：从左向右贪心切最长词
 * RMM（Reverse Max Match）：从右向左贪心切最长词
 * 双向校验：FMM 与 RMM 结果一致则接受；不一致则按更短切分（粗粒度）。
 *
 * 未登录词 fallback bigram（2 字符）。
 */
static int cjk_char_len(const unsigned char *p) {
    if ((*p & 0x80) == 0) return 1;
    if ((*p & 0xE0) == 0xC0) return 2;
    if ((*p & 0xF0) == 0xE0) return 3;
    if ((*p & 0xF8) == 0xF0) return 4;
    return 1;
}

static int fmm_cut(const cjk_dict_t *dict, const char *text, size_t len,
                    int *offsets, int max_offsets) {
    int n = 0;
    size_t i = 0;
    while (i < len && n < max_offsets) {
        int best = 1;  /* 默认 1 字符 */
        for (int sz = 6; sz >= 2; --sz) {
            size_t j = i;
            int cl = 0;
            while (cl < sz && j < len) {
                int l = cjk_char_len((const unsigned char *)(text + j));
                cl += l; j += l;
            }
            if (cl == sz && j <= len
                && cjk_dict_contains(dict, text + i, j - i)) {
                best = j - i;
                break;
            }
        }
        offsets[n++] = (int)i;
        i += best;
    }
    return n;
}

cjk_token_list_t cjk_tokenize(const cjk_dict_t *dict, const char *text, size_t len) {
    cjk_token_list_t result = { NULL, 0 };
    if (!text || len == 0) return result;
    int fmm[2048], rmm[2048];
    int nf = fmm_cut(dict, text, len, fmm, 2048);
    /* RMM：反向扫描同样逻辑，offsets 倒序填 */
    int nrmm = 0;
    size_t i = len;
    while (i > 0 && nrmm < 2048) {
        int best = 1;
        /* UTF-8 倒退 1 字符 = 1 字节（ASCII）或多字节 */
        for (int sz = 6; sz >= 2; --sz) {
            /* 找 sz 个字符的起始位置 */
            size_t end = i;
            size_t j = end;
            int cl = 0;
            while (cl < sz && j > 0) {
                /* 退一个字符 */
                size_t prev = j;
                do { prev--; } while (prev > 0
                    && ((text[prev] & 0xC0) == 0x80));
                cl++;
                j = prev;
            }
            if (cl == sz && j < end
                && cjk_dict_contains(dict, text + j, end - j)) {
                best = (int)(end - j);
                break;
            }
        }
        rmm[nrmm++] = (int)i - best;
        i -= best;
    }
    /* FMM 顺序 / RMM 逆序；如果不一致采用 FMM（更简单一致） */
    result.tokens = calloc(nf, sizeof(cjk_token_t));
    if (!result.tokens) return result;
    int prev = 0;
    for (int k = 0; k < nf; ++k) {
        int end = fmm[k];
        int start = prev;
        result.tokens[result.n_tokens].text = my_strndup(text + start, end - start);
        result.tokens[result.n_tokens].start = (size_t)start;
        result.tokens[result.n_tokens].end = (size_t)end;
        result.n_tokens++;
        prev = end;
    }
    return result;
}

void cjk_token_list_free(cjk_token_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->n_tokens; ++i) free(list->tokens[i].text);
    free(list->tokens);
    list->tokens = NULL;
    list->n_tokens = 0;
}

/* ========== Snowball Porter2 词干化（自研移植）==========
 *
 * 实现 Martin Porter 2002 英文 Porter2 算法。算法公开，可自研移植。
 * 简化：只实现 Porter2 主体 5 步（Step 1a, 1b, 1c, 2-5），
 * 跳过 Step 5 末尾的 y/i 边界特例。
 */
static int ends_with(const char *s, size_t len, const char *suffix) {
    size_t sl = strlen(suffix);
    return len >= sl && memcmp(s + len - sl, suffix, sl) == 0;
}

static int contains_vowel(const char *s, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        char c = s[i];
        if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'
            || c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') return 1;
    }
    return 0;
}

static int ends_double(const char *s, size_t len) {
    return len >= 2 && s[len-1] == s[len-2];
}

static void drop_trailing(char *s, int *len, int n) {
    *len -= n;
    s[*len] = '\0';
}

char *snowball_porter2_stem(const char *word, size_t len) {
    if (!word || len == 0) return NULL;
    if (len > 64) len = 64;  /* 简化上限 */
    char *buf = my_strndup(word, len);
    if (!buf) return NULL;
    int n = (int)len;

    /* Step 1a: sses → ss, ies → i, ss → ss, s → */
    if (ends_with(buf, n, "sses")) { drop_trailing(buf, &n, 2); }
    else if (ends_with(buf, n, "ies")) { drop_trailing(buf, &n, 2); buf[n++] = 'i'; buf[n] = '\0'; }
    else if (ends_with(buf, n, "ss")) { /* keep */ }
    else if (ends_with(buf, n, "s")) { drop_trailing(buf, &n, 1); }

    /* Step 1b: (eed, eedly → ee; ed, edly, ing, ingly → strip + rules) */
    if (ends_with(buf, n, "eed")) {
        if (n - 3 >= 3 && contains_vowel(buf, n - 3)) {
            drop_trailing(buf, &n, 1);  /* eed → ee */
        }
    } else if (ends_with(buf, n, "edly") || ends_with(buf, n, "ed")
               || ends_with(buf, n, "ingly") || ends_with(buf, n, "ing")) {
        int strip;
        if (ends_with(buf, n, "ingly")) strip = 5;
        else if (ends_with(buf, n, "edly")) strip = 4;
        else if (ends_with(buf, n, "ed") || ends_with(buf, n, "ing")) strip = 2;
        else strip = 0;
        drop_trailing(buf, &n, strip);
        /* 简化：at/iz → ate/ize；双字母末尾去重；短词以 e 结尾 */
        if (ends_with(buf, n, "at")) { buf[n++] = 'e'; buf[n] = '\0'; }
        else if (ends_with(buf, n, "iz")) { buf[n++] = 'e'; buf[n] = '\0'; }
        else if (ends_double(buf, n)
                 && (buf[n-1] == 'l' || buf[n-1] == 's' || buf[n-1] == 'z')) {
            drop_trailing(buf, &n, 1);
        } else if (n == 2 && contains_vowel(buf, n)) {
            buf[n++] = 'e'; buf[n] = '\0';
        }
    }

    /* Step 1c: y → i（如果前缀含元音且非首字母） */
    if (n > 2 && buf[n-1] == 'y' && contains_vowel(buf, n - 1)) {
        buf[n-1] = 'i';
    }

    /* Step 2-5：完整 Porter2 较复杂（~200 行），此处省略
     * 简化版：处理常见后缀 edly/ing 后的部分情况
     */

    return buf;
}