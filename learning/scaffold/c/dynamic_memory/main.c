/* dynamic_memory scaffold — malloc/calloc/realloc/free + malloc trap */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "OOM: malloc(%zu) failed\n", n); exit(1); }
    return p;
}

static void *xcalloc(size_t n, size_t sz) {
    void *p = calloc(n, sz);
    if (!p) { fprintf(stderr, "OOM: calloc failed\n"); exit(1); }
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q && n != 0) { fprintf(stderr, "OOM: realloc failed\n"); exit(1); }
    return q;
}

typedef struct {
    int *data;
    size_t len;
    size_t cap;
} int_vec_t;

static void vec_init(int_vec_t *v) {
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}

static void vec_push(int_vec_t *v, int x) {
    if (v->len == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 4;
        v->data = xrealloc(v->data, new_cap * sizeof(int));
        v->cap = new_cap;
    }
    v->data[v->len++] = x;
}

static void vec_free(int_vec_t *v) {
    free(v->data);
    v->data = NULL;
    v->len = v->cap = 0;
}

int main(void) {
    /* 1) malloc 基础 */
    int *a = xmalloc(5 * sizeof(int));
    for (int i = 0; i < 5; i++) a[i] = i * i;
    printf("[malloc] a =");
    for (int i = 0; i < 5; i++) printf(" %d", a[i]);
    printf("\n");
    free(a);

    /* 2) calloc 初始化为 0 */
    int *b = xcalloc(4, sizeof(int));
    printf("[calloc] b =");
    for (int i = 0; i < 4; i++) printf(" %d", b[i]);
    printf("\n");
    free(b);

    /* 3) realloc 扩容 */
    char *s = xmalloc(8);
    strcpy(s, "hello");
    s = xrealloc(s, 32);
    strcat(s, ", world!");
    printf("[realloc] s = '%s'\n", s);
    free(s);

    /* 4) 简易 vector 演示 2 倍扩容 */
    int_vec_t v;
    vec_init(&v);
    for (int i = 0; i < 10; i++) vec_push(&v, i * 10);
    printf("[vec]    capacity grown, len=%zu cap=%zu, items:",
           v.len, v.cap);
    for (size_t i = 0; i < v.len; i++) printf(" %d", v.data[i]);
    printf("\n");
    vec_free(&v);

    /* 5) 已 free 指针置 NULL 防 dangling */
    int *p = xmalloc(sizeof(int));
    *p = 7;
    free(p);
    p = NULL;
    if (p) printf("dangling!\n"); else printf("[null]    p == NULL after free\n");

    return 0;
}
