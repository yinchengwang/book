#include "db/fsm.h"
#include "db/core/log.h"

#include <stdlib.h>
#include <string.h>

/* 简化实现：bitmap，1 bit per page */
struct fsm_s {
    uint8_t *bitmap;     /* n_pages/8 字节 */
    uint32_t n_pages;
    uint32_t cap;
};

static uint32_t byte_count(uint32_t n_pages) {
    return (n_pages + 7) / 8;
}

fsm_t *fsm_create(uint32_t n_pages) {
    fsm_t *fsm = calloc(1, sizeof(*fsm));
    if (!fsm) return NULL;
    fsm->n_pages = n_pages;
    fsm->cap = byte_count(n_pages);
    fsm->bitmap = calloc(fsm->cap, 1);
    if (!fsm->bitmap) { free(fsm); return NULL; }
    return fsm;
}

void fsm_destroy(fsm_t *fsm) { if (fsm) { free(fsm->bitmap); free(fsm); } }

int fsm_mark_free(fsm_t *fsm, uint32_t page_id, bool is_free) {
    if (!fsm || page_id >= fsm->n_pages) return -1;
    uint32_t byte = page_id / 8;
    uint32_t bit = page_id % 8;
    if (is_free) fsm->bitmap[byte] |= (uint8_t)(1u << bit);
    else fsm->bitmap[byte] &= (uint8_t)~(1u << bit);
    return 0;
}

int32_t fsm_find_free(fsm_t *fsm) {
    if (!fsm) return -1;
    for (uint32_t i = 0; i < fsm->cap; ++i) {
        if (fsm->bitmap[i] != 0) {
            for (uint32_t b = 0; b < 8; ++b) {
                if (fsm->bitmap[i] & (1u << b)) {
                    uint32_t p = i * 8 + b;
                    if (p < fsm->n_pages) return (int32_t)p;
                }
            }
        }
    }
    return -1;
}

uint32_t fsm_n_pages(const fsm_t *fsm) {
    return fsm ? fsm->n_pages : 0;
}
