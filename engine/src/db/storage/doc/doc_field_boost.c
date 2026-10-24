#include "db/storage/doc/doc_engine.h"
#include "db/core/log.h"
#include <string.h>

/* C3-2 T1: 字段加权（占位）
 * schema 解析时记录 field_boost；query 时 field_boost × BM25 score
 */
typedef struct doc_field_boost_s {
    char field[64];
    float boost;  /* 默认 1.0 */
} doc_field_boost_t;

typedef struct doc_field_boost_set_s {
    doc_field_boost_t fields[16];
    size_t n;
} doc_field_boost_set_t;

doc_field_boost_set_t *doc_field_boost_set_create(void) {
    return calloc(1, sizeof(doc_field_boost_set_t));
}

void doc_field_boost_set_free(doc_field_boost_set_t *bs) { free(bs); }

int doc_field_boost_set_add(doc_field_boost_set_t *bs, const char *field, float boost) {
    if (!bs || !field || bs->n >= 16) return -1;
    strncpy(bs->fields[bs->n].field, field, 63);
    bs->fields[bs->n].boost = boost;
    bs->n++;
    return 0;
}

float doc_field_boost_get(const doc_field_boost_set_t *bs, const char *field) {
    if (!bs || !field) return 1.0f;
    for (size_t i = 0; i < bs->n; ++i) {
        if (strcmp(bs->fields[i].field, field) == 0) return bs->fields[i].boost;
    }
    return 1.0f;
}
