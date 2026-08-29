#include "db/ltree.h"
#include "db/core/log.h"

#include <stdlib.h>
#include <string.h>

/* Windows 不提供 strndup，提供一个本地实现 */
static char *strndup_win(const char *s, size_t n) {
    size_t len = strlen(s);
    if (len < n) len = n;
    char *dup = (char *)malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}
#ifndef strndup
#define strndup strndup_win
#endif

#define LABEL_MAX 64
#define PATH_MAX_LEVEL 32

struct ltree_s {
    char **labels;
    int n_labels;
};

ltree_t *ltree_parse(const char *label_path) {
    if (!label_path) return NULL;
    ltree_t *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->labels = calloc(PATH_MAX_LEVEL, sizeof(char *));
    if (!t->labels) { free(t); return NULL; }
    const char *p = label_path;
    while (*p && t->n_labels < PATH_MAX_LEVEL) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        if (len == 0 || len >= LABEL_MAX) break;
        t->labels[t->n_labels] = strndup(p, len);
        t->n_labels++;
        if (!dot) break;
        p = dot + 1;
    }
    return t;
}

void ltree_free(ltree_t *t) {
    if (!t) return;
    for (int i = 0; i < t->n_labels; ++i) free(t->labels[i]);
    free(t->labels);
    free(t);
}

bool ltree_contains(const ltree_t *outer, const ltree_t *inner) {
    if (!outer || !inner) return false;
    if (inner->n_labels > outer->n_labels) return false;
    for (int i = 0; i < inner->n_labels; ++i) {
        if (strcmp(outer->labels[i], inner->labels[i]) != 0) return false;
    }
    return true;
}

bool ltree_contained_by(const ltree_t *inner, const ltree_t *outer) {
    return ltree_contains(outer, inner);
}

bool ltree_matches(const ltree_t *t, const char *pattern) {
    /* 占位：pattern 形式如 'a.*.c' 骨架 */
    if (!t || !pattern) return false;
    (void)t; (void)pattern;
    return false;
}
