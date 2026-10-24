/*
 * pg_hash_insert.c
 *
 * Insert a key-value pair into the PG-style linear hash index.
 *
 * Algorithm (mirrors PG's _hash_doinsert):
 *
 *   1. Hash the key → hashval
 *   2. Determine target bucket via _pg_hash_get_bucket_no()
 *   3. Scan the bucket's page chain for a duplicate key; remember the
 *      first page that has a free slot (insert_page).
 *   4. If no free slot exists on any existing page, append an overflow page.
 *   5. Create a new tuple and write it to insert_page.
 *   6. Check the split trigger (_pg_hash_maybe_split).
 */

#include "pg_hash_private.h"

/*
 * pg_hash_insert
 *
 * Returns:
 *   0   inserted successfully
 *   1   key already exists (duplicate rejected)
 *  -1   bad arguments or allocation failure
 */
int pg_hash_insert(pg_hash_t   *idx,
                   const void  *key,      uint32_t keylen,
                   const void  *value,    uint32_t valuelen)
{
    uint32_t        hashval;
    uint32_t        bucket;
    pg_hash_page_t *page;
    pg_hash_page_t *last_page;        /* last page visited (tail of chain)  */
    pg_hash_page_t *insert_page;      /* first page with a free slot        */
    pg_hash_tuple_t *tup;
    uint32_t i;

    if (!idx || !key || keylen == 0) {
        return -1;
    }

    hashval = _pg_hash_func(key, keylen);
    bucket  = _pg_hash_get_bucket_no(idx, hashval);

    /*
     * Single-pass scan:
     *   - check for duplicates
     *   - record the first page with space (insert_page)
     *   - record the tail page (last_page) for overflow append
     */
    insert_page = NULL;
    last_page   = NULL;
    page        = idx->buckets[bucket];

    while (page) {
        /* Look for duplicate */
        for (i = 0; i < page->ntuples; i++) {
            const pg_hash_tuple_t *existing = page->tuples[i];

            if (existing                                  &&
                existing->t_hashval == hashval            &&
                existing->t_keylen  == keylen             &&
                memcmp(existing->t_key, key, keylen) == 0) {
                return 1;   /* duplicate */
            }
        }

        /* Remember first page that has room */
        if (!insert_page && page->ntuples < PG_HASH_PAGE_MAX_ITEMS) {
            insert_page = page;
        }

        last_page = page;
        page      = page->next_overflow;
    }

    /* No free slot on any existing page → allocate overflow page */
    if (!insert_page) {
        pg_hash_page_t *ovfl = _pg_hash_page_create(true);

        if (!ovfl) {
            return -1;
        }
        last_page->next_overflow = ovfl;
        insert_page = ovfl;
    }

    /* Create and store the new tuple */
    tup = _pg_hash_tuple_create(hashval, key, keylen, value, valuelen);
    if (!tup) {
        return -1;
    }

    insert_page->tuples[insert_page->ntuples++] = tup;
    idx->n_total++;

    /* Trigger a split if the load factor is exceeded */
    _pg_hash_maybe_split(idx);

    return 0;
}
