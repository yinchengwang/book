#ifndef DB_LTREE_H
#define DB_LTREE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* C7.6: PostgreSQL ltree 兼容层骨架
 * 路径列类型：'a.b.c.d' 形式，以 . 分隔
 * 操作符：<@ 子集，@> 超集，~> 后续
 */

typedef struct ltree_s ltree_t;

ltree_t *ltree_parse(const char *label_path);
void ltree_free(ltree_t *t);

/* 操作符 */
bool ltree_contains(const ltree_t *outer, const ltree_t *inner);  /* <@ */
bool ltree_contained_by(const ltree_t *inner, const ltree_t *outer);  /* @> */
bool ltree_matches(const ltree_t *t, const char *pattern);  /* ~ pattern */

#ifdef __cplusplus
}
#endif

#endif
