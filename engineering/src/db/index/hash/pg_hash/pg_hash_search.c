/*
 * pg_hash_search.c
 *
 * Key lookup for the PG-style linear hash index.
 *
 * Algorithm (mirrors PG's _hash_search):
 *
 *   1. Hash the key → hashval
 *   2. Determine bucket via _pg_hash_get_bucket_no()
 *   3. Scan the page chain (primary + overflow pages) comparing:
 *        a. hashval  (fast pre-filter, avoids memcmp on mismatch)
 *        b. keylen
 *        c. key bytes (memcmp)
 *   4. On match, expose *t_value and t_valuelen via out-parameters.
 */

#include "pg_hash_private.h"

/*
 * pg_hash_lookup
 *
 * Returns:
 *   0   key found; *value_out and *valuelen_out set (may both be NULL
 *       if the caller passes NULL for those pointers).
 *  -1   key not found, or bad arguments.
 *
 * NOTE: *value_out points directly into the index's internal storage.
 *       The caller must NOT free it, and should not hold the pointer
 *       across any subsequent mutation (insert / delete / split).
 */
int pg_hash_lookup(const pg_hash_t *idx,
                   const void      *key,         uint32_t  keylen,
                   void           **value_out,   uint32_t *valuelen_out)
{
    uint32_t              hashval;
    uint32_t              bucket;
    const pg_hash_page_t *page;
    uint32_t              i;

    if (!idx || !key || keylen == 0) {
        return -1;
    }

    hashval = _pg_hash_func(key, keylen);
    bucket  = _pg_hash_get_bucket_no(idx, hashval);
    page    = idx->buckets[bucket];

    while (page) {
        for (i = 0; i < page->ntuples; i++) {
            const pg_hash_tuple_t *tup = page->tuples[i];

            /* hashval is a fast pre-filter before the expensive memcmp */
            if (tup                              &&
                tup->t_hashval == hashval        &&
                tup->t_keylen  == keylen         &&
                memcmp(tup->t_key, key, keylen) == 0) {

                if (value_out) {
                    *value_out    = tup->t_value;
                }
                if (valuelen_out) {
                    *valuelen_out = tup->t_valuelen;
                }
                return 0;
            }
        }
        page = page->next_overflow;
    }

    return -1;   /* not found */
}
