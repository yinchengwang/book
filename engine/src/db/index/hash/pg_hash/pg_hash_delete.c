/*
 * pg_hash_delete.c
 *
 * Tuple deletion for the PG-style linear hash index.
 *
 * Algorithm (mirrors PG's _hash_kill_items / _hash_vacuum_one_page):
 *
 *   1. Hash the key → hashval
 *   2. Determine bucket via _pg_hash_get_bucket_no()
 *   3. Scan the page chain for the matching tuple.
 *   4. On match:
 *       a. Free the tuple's memory.
 *       b. Compact the tuples[] array on that page (shift trailing entries
 *          left by one so there are no holes).
 *       c. Decrement page->ntuples and idx->n_total.
 *
 * Note: PG's hash index also recycles empty overflow pages back to a
 * free-list (see _hash_freeovflpage).  We simply leave empty overflow
 * pages in the chain; they will be reused on the next insert into the
 * same bucket, and are freed lazily when the bucket is split or the
 * index is destroyed.
 */

#include "pg_hash_private.h"

/*
 * pg_hash_delete
 *
 * Returns:
 *   0   key found and deleted
 *  -1   key not found, or bad arguments
 */
int pg_hash_delete(pg_hash_t  *idx,
                   const void *key, uint32_t keylen)
{
    uint32_t        hashval;
    uint32_t        bucket;
    pg_hash_page_t *page;
    uint32_t        i;
    uint32_t        j;

    if (!idx || !key || keylen == 0) {
        return -1;
    }

    hashval = _pg_hash_func(key, keylen);
    bucket  = _pg_hash_get_bucket_no(idx, hashval);
    page    = idx->buckets[bucket];

    while (page) {
        for (i = 0; i < page->ntuples; i++) {
            pg_hash_tuple_t *tup = page->tuples[i];

            if (tup                              &&
                tup->t_hashval == hashval        &&
                tup->t_keylen  == keylen         &&
                memcmp(tup->t_key, key, keylen) == 0) {

                /* Release the tuple */
                _pg_hash_tuple_drop(tup);

                /* Compact: shift remaining slots left */
                for (j = i + 1; j < page->ntuples; j++) {
                    page->tuples[j - 1] = page->tuples[j];
                }
                page->tuples[page->ntuples - 1] = NULL;
                page->ntuples--;
                idx->n_total--;

                return 0;
            }
        }
        page = page->next_overflow;
    }

    return -1;   /* not found */
}
