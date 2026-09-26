/**
 * @file strndup_win32.c
 * @brief Windows fallback for POSIX strndup
 *
 * T12: MinGW 没有原生 strndup；bm25_index.c 使用它做 token 截断。
 * 提供 _strndup 截断版替代。
 */
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(strndup)
char *strndup(const char *s, size_t n) {
    if (s == NULL) return NULL;
    size_t len = strnlen(s, n);
    char *p = (char *)malloc(len + 1);
    if (p == NULL) return NULL;
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

size_t strnlen(const char *s, size_t n) {
    if (s == NULL) return 0;
    size_t i = 0;
    while (i < n && s[i] != '\0') i++;
    return i;
}
#endif