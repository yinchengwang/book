#include "db/doc_engine.h"
#include "db/core/log.h"
#include <string.h>

/* C3-2 T4: Unified Highlighter（骨架） */
char *doc_highlight_unified(const char *text, size_t text_len,
                            const char *const *terms, size_t n_terms,
                            size_t context_chars) {
    if (!text || !terms || n_terms == 0) return NULL;
    /* 占位：在 text 中找首个 term，返回 "...term..." 上下文窗口 */
    for (size_t i = 0; i < n_terms; ++i) {
        if (!terms[i] || terms[i][0] == '\0') continue;
        const char *p = strstr(text, terms[i]);
        if (p) {
            size_t idx = (size_t)(p - text);
            size_t start = idx > context_chars ? idx - context_chars : 0;
            size_t end = idx + strlen(terms[i]) + context_chars;
            if (end > text_len) end = text_len;
            size_t len = end - start;
            char *out = malloc(len + 3);
            if (!out) return NULL;
            memcpy(out, text + start, len);
            out[len] = '\0';
            return out;
        }
    }
    /* 没找到：返回前 context_chars 个字符 */
    size_t n = text_len < context_chars ? text_len : context_chars;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, text, n);
    out[n] = '\0';
    return out;
}
