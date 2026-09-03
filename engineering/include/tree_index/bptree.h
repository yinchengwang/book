/*
 * bptree.h
 *
 * Public API for an in-memory B+Tree index inspired by database
 * secondary-index layouts.
 *
 * Characteristics:
 *   - internal nodes store only separator keys
 *   - payload lives in leaf pages
 *   - leaf pages are linked for efficient range scans
 */

#ifndef BPTREE_INDEX_H
#define BPTREE_INDEX_H

#include <tree_index/tree_page.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BPTREE_DEFAULT_ORDER 8u
#define BPTREE_MIN_ORDER 3u

typedef int (*bptree_compare_fn)(const void *lhs, uint32_t lhs_len,
                                 const void *rhs, uint32_t rhs_len,
                                 void *ctx);

typedef int (*bptree_scan_visit_fn)(const void *key, uint32_t keylen,
                                    const void *value, uint32_t valuelen,
                                    void *ctx);

typedef struct bptree_index bptree_index_t;

bptree_index_t *bptree_index_create(uint32_t order,
                                    bptree_compare_fn compare,
                                    void *compare_ctx);

void bptree_index_drop(bptree_index_t *index);

int bptree_index_insert(bptree_index_t *index,
                        const void *key, uint32_t keylen,
                        const void *value, uint32_t valuelen);

int bptree_index_upsert(bptree_index_t *index,
                        const void *key, uint32_t keylen,
                        const void *value, uint32_t valuelen);

int bptree_index_delete(bptree_index_t *index,
                        const void *key, uint32_t keylen);

int bptree_index_lookup(const bptree_index_t *index,
                        const void *key, uint32_t keylen,
                        void **value_out, uint32_t *valuelen_out);

int bptree_index_range_scan(const bptree_index_t *index,
                            const void *lower_key, uint32_t lower_keylen,
                            bool lower_inclusive,
                            const void *upper_key, uint32_t upper_keylen,
                            bool upper_inclusive,
                            bptree_scan_visit_fn visitor,
                            void *visitor_ctx);

            int bptree_index_save(const bptree_index_t *index,
                            const char *path,
                            const tree_index_persist_options_t *options);

            bptree_index_t *bptree_index_load(const char *path,
                                    bptree_compare_fn compare,
                                    void *compare_ctx);

uint32_t bptree_index_size(const bptree_index_t *index);
uint32_t bptree_index_height(const bptree_index_t *index);
uint32_t bptree_index_leaf_count(const bptree_index_t *index);

#ifdef __cplusplus
}
#endif

#endif /* BPTREE_INDEX_H */