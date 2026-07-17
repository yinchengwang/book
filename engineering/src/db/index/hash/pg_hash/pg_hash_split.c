/*
 * pg_hash_split.c
 *
 * Bucket splitting for the PG-style linear hash index.
 *
 * --------------------------------------------------------------------------
 * Linear hashing split mechanics (Litwin 1980, as in PG's _hash_splitbucket)
 * --------------------------------------------------------------------------
 *
 * State before split:
 *   max_bucket = B,  high_mask = H = 2^k − 1,  low_mask = L = 2^(k−1) − 1
 *
 * Step 1 – advance max_bucket:
 *   new_bucket = B + 1
 *   old_bucket = new_bucket & L    ← the bucket being split
 *
 * Step 2 – allocate primary page for new_bucket.
 *
 * Step 3 – update max_bucket and possibly the masks:
 *   max_bucket = new_bucket
 *   if (new_bucket == H) {          ← end of current "round"
 *       L = H;
 *       H = (H << 1) | 1;           ← = 2^(k+1) − 1
 *   }
 *
 * Step 4 – redistribute tuples from old_bucket:
 *   For each tuple t in old_bucket:
 *     if _pg_hash_get_bucket_no(t->hashval) == new_bucket  → move to new
 *     else                                                  → keep in old
 *
 *   The lookup function already uses the updated masks, so it correctly
 *   partitions items between old_bucket and new_bucket.
 *
 * This redistribution creates a fresh page chain for old_bucket and fills
 * new_bucket's chain from scratch, then discards the original page chain
 * of old_bucket (page structs only—tuples are moved, not freed).
 * --------------------------------------------------------------------------
 */

#include "pg_hash_private.h"

/*
 * _pg_hash_split_bucket
 *
 * Creates bucket (max_bucket + 1) and redistributes old_bucket's tuples.
 * Returns 0 on success, -1 on OOM.
 */
int _pg_hash_split_bucket(pg_hash_t *idx)
{
    uint32_t        new_bucket;
    uint32_t        old_bucket;
    pg_hash_page_t *new_primary;
    pg_hash_page_t *old_new_primary;   /* rebuilt head for old_bucket */
    pg_hash_page_t *old_write;         /* current write page for old_bucket */
    pg_hash_page_t *new_write;         /* current write page for new_bucket */
    pg_hash_page_t *scan;              /* scan pointer over old_bucket chain */

    new_bucket = idx->max_bucket + 1;
    old_bucket = new_bucket & idx->low_mask;

    /* ------------------------------------------------------------------ */
    /* Step 1 – grow buckets[] array if needed                            */
    /* ------------------------------------------------------------------ */
    if (new_bucket >= idx->bucket_alloc) {
        uint32_t new_alloc = idx->bucket_alloc * 2;

        if (new_alloc <= new_bucket) {
            new_alloc = new_bucket + 1;
        }
        if (_pg_hash_expand_table(idx, new_alloc) != 0) {
            return -1;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Step 2 – allocate primary page for new_bucket                      */
    /* ------------------------------------------------------------------ */
    new_primary = _pg_hash_page_create(false);
    if (!new_primary) {
        return -1;
    }

    /* ------------------------------------------------------------------ */
    /* Step 3 – commit state update BEFORE redistribution                 */
    /*                                                                    */
    /* _pg_hash_get_bucket_no() must use the final masks when deciding    */
    /* where each tuple lands, so we update everything now.               */
    /* ------------------------------------------------------------------ */
    idx->buckets[new_bucket] = new_primary;
    idx->max_bucket = new_bucket;

    if (new_bucket == idx->high_mask) {
        /* End of this doubling round: advance both masks */
        idx->low_mask  = idx->high_mask;
        idx->high_mask = (idx->high_mask << 1) | 1u;
    }

    /* ------------------------------------------------------------------ */
    /* Step 4 – redistribute tuples from old_bucket                       */
    /* ------------------------------------------------------------------ */

    /* Allocate a fresh primary page to rebuild old_bucket's chain */
    old_new_primary = _pg_hash_page_create(false);
    if (!old_new_primary) {
        return -1;
    }

    old_write = old_new_primary;
    new_write = new_primary;

    /*
     * Walk every page in old_bucket's existing chain.
     * For each tuple, call _pg_hash_get_bucket_no() to determine its
     * correct bucket under the new masks, then append it to the
     * appropriate write-page chain.
     *
     * We free the original page structs as we go.  Tuples are not freed—
     * ownership is transferred to the new chains.
     */
    scan = idx->buckets[old_bucket];
    while (scan) {
        pg_hash_page_t *next_page = scan->next_overflow;
        uint32_t i;

        for (i = 0; i < scan->ntuples; i++) {
            pg_hash_tuple_t *tup = scan->tuples[i];

            if (!tup) {
                continue;
            }

            if (_pg_hash_get_bucket_no(idx, tup->t_hashval) == new_bucket) {
                /* Tuple belongs in the new bucket */
                if (new_write->ntuples >= PG_HASH_PAGE_MAX_ITEMS) {
                    pg_hash_page_t *ovfl = _pg_hash_page_create(true);

                    if (!ovfl) {
                        return -1;
                    }
                    new_write->next_overflow = ovfl;
                    new_write = ovfl;
                }
                new_write->tuples[new_write->ntuples++] = tup;
            } else {
                /* Tuple stays in old bucket */
                if (old_write->ntuples >= PG_HASH_PAGE_MAX_ITEMS) {
                    pg_hash_page_t *ovfl = _pg_hash_page_create(true);

                    if (!ovfl) {
                        return -1;
                    }
                    old_write->next_overflow = ovfl;
                    old_write = ovfl;
                }
                old_write->tuples[old_write->ntuples++] = tup;
            }

            /* Null out the slot so the page-struct free below is safe */
            scan->tuples[i] = NULL;
        }

        /* Free the old page struct (tuples already moved) */
        free(scan);
        scan = next_page;
    }

    /* Install the rebuilt chain as old_bucket's new head */
    idx->buckets[old_bucket] = old_new_primary;

    return 0;
}

/* -----------------------------------------------------------------------
 * _pg_hash_maybe_split
 *
 * Split trigger, called after every insert.
 *
 * Invariant to maintain:
 *   n_total ≤ (fill_factor / 100.0) * PAGE_MAX_ITEMS * (max_bucket + 1)
 *
 * Equivalently (integer arithmetic, no float):
 *   n_total * 100 ≤ fill_factor * PAGE_MAX_ITEMS * (max_bucket + 1)
 *
 * We loop because a single insert can push the load above threshold by
 * more than one split-worth when fill_factor is very low.
 * ----------------------------------------------------------------------- */
void _pg_hash_maybe_split(pg_hash_t *idx)
{
    if (!idx) {
        return;
    }

    while ((uint64_t)idx->n_total * 100 >
           (uint64_t)idx->fill_factor * PG_HASH_PAGE_MAX_ITEMS * (idx->max_bucket + 1)) {
        if (_pg_hash_split_bucket(idx) != 0) {
            break;   /* OOM – stop splitting, accept degraded performance */
        }
    }
}
