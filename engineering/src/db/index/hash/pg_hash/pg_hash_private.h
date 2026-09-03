/*
 * pg_hash_private.h
 *
 * Internal data structures and helper declarations for the PG-style
 * linear hash index.  NOT part of the public API.
 *
 * Terminology mirrors PG's hash access method:
 *
 *   tuple      – one (hashval, key, value) record
 *   page       – a fixed-capacity array of tuple pointers;
 *                primary pages are the "bucket head",
 *                overflow pages are chained via next_overflow
 *   bucket     – the chain: primary_page → overflow_page → ...
 *   high_mask  – bitmask used for bucket lookup (= 2^k − 1)
 *   low_mask   – bitmask for the "previous level" (= 2^(k−1) − 1)
 *   max_bucket – index of the highest allocated bucket (0-based)
 *
 * Bucket lookup (linear hashing, Litwin 1980):
 *   b = hashval & high_mask;
 *   if (b > max_bucket)  b &= low_mask;
 *
 * Split trigger (after every insert):
 *   if n_total > (fill_factor / 100.0) * PAGE_MAX_ITEMS * (max_bucket + 1)
 *       split
 */

#ifndef PG_HASH_PRIVATE_H
#define PG_HASH_PRIVATE_H

#include <db/index/hash/pg_hash.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Tuple
 * ----------------------------------------------------------------------- */

/*
 * pg_hash_tuple_t – one stored record.
 *
 * The hash value is pre-computed and stored so that bucket re-assignment
 * during a split does not require re-hashing the key.
 */
typedef struct pg_hash_tuple {
    uint32_t  t_hashval;   /* FNV-1a hash of the key                 */
    void     *t_key;       /* heap-allocated copy of the key data    */
    uint32_t  t_keylen;    /* byte length of key                     */
    void     *t_value;     /* heap-allocated copy of value data      */
    uint32_t  t_valuelen;  /* byte length of value (0 → NULL value)  */
} pg_hash_tuple_t;

/* -----------------------------------------------------------------------
 * Page
 * ----------------------------------------------------------------------- */

/*
 * pg_hash_page_t – one "disk page" equivalent (held in memory).
 *
 * Each bucket starts with exactly one primary page.  When that page is
 * full, overflow pages are appended via next_overflow.
 */
typedef struct pg_hash_page {
    uint32_t            ntuples;                            /* # live tuples on this page     */
    pg_hash_tuple_t    *tuples[PG_HASH_PAGE_MAX_ITEMS];     /* pointers to tuples             */
    struct pg_hash_page *next_overflow;                     /* next overflow page, or NULL    */
    bool                is_overflow;                        /* true → overflow page           */
} pg_hash_page_t;

/* -----------------------------------------------------------------------
 * Hash index control structure  (struct pg_hash)
 * ----------------------------------------------------------------------- */

struct pg_hash {
    uint32_t         n_total;        /* total number of live tuples                */
    uint32_t         max_bucket;     /* index of highest allocated bucket (0-based)*/
    uint32_t         high_mask;      /* = 2^k − 1  (current level mask)           */
    uint32_t         low_mask;       /* = 2^(k−1) − 1  (previous level mask)      */
    uint32_t         fill_factor;    /* fill-factor percentage (10‥100)            */
    uint32_t         bucket_alloc;   /* allocated length of buckets[]              */
    pg_hash_page_t **buckets;        /* bucket array, indexed [0..max_bucket]      */
};

/* -----------------------------------------------------------------------
 * Internal function declarations
 * ----------------------------------------------------------------------- */

/* --- Hash function (pg_hash_core.c) --- */

/* FNV-1a 32-bit hash, similar to PG's hash_any() */
uint32_t _pg_hash_func(const void *key, uint32_t keylen);

/* Linear hashing bucket lookup */
uint32_t _pg_hash_get_bucket_no(const pg_hash_t *idx, uint32_t hashval);

/* --- Page / tuple helpers (pg_hash_core.c) --- */

pg_hash_page_t  *_pg_hash_page_create(bool is_overflow);

/*
 * Free an entire chain starting at *page* (page + all overflow pages).
 * Also frees every tuple referenced by those pages.
 */
void             _pg_hash_page_drop(pg_hash_page_t *page);

pg_hash_tuple_t *_pg_hash_tuple_create(uint32_t    hashval,
                                       const void *key,   uint32_t keylen,
                                       const void *value, uint32_t valuelen);

void             _pg_hash_tuple_drop(pg_hash_tuple_t *tup);

/* --- Table expansion (pg_hash_core.c) --- */

/*
 * Grow buckets[] array to hold at least *new_size* slots.
 * New slots are zero-initialised.  Returns 0 on success, -1 on OOM.
 */
int _pg_hash_expand_table(pg_hash_t *idx, uint32_t new_size);

/* --- Splitting (pg_hash_split.c) --- */

/*
 * Split the bucket that is the "oldest unsplit" bucket, creating
 * bucket (max_bucket + 1) and redistributing its tuples.
 * Returns 0 on success, -1 on OOM.
 */
int _pg_hash_split_bucket(pg_hash_t *idx);

/*
 * Called after every insert; triggers _pg_hash_split_bucket() as many
 * times as necessary to restore the load invariant.
 */
void _pg_hash_maybe_split(pg_hash_t *idx);

#endif /* PG_HASH_PRIVATE_H */
