/*
 * pg_hash_core.c
 *
 * Core routines for the PG-style linear hash index:
 *   - FNV-1a hash function  (_pg_hash_func)
 *   - Linear-hashing bucket lookup  (_pg_hash_get_bucket_no)
 *   - Page / tuple allocation and teardown
 *   - Bucket-array expansion
 *   - Index creation (pg_hash_create) and destruction (pg_hash_drop)
 *   - Stat accessors (pg_hash_size, pg_hash_bucket_count)
 */

#include "pg_hash_private.h"

/* Initial bucket count; must be a power of two */
#define PG_HASH_INITIAL_BUCKETS  1u

/* -----------------------------------------------------------------------
 * Hash function
 * ----------------------------------------------------------------------- */

/*
 * _pg_hash_func – FNV-1a 32-bit hash.
 *
 * Similar to PG's hash_any().  Fast, good distribution, no external deps.
 *
 *   offset_basis = 2166136261  (0x811c9dc5)
 *   prime        = 16777619    (0x01000193)
 *
 * The XOR-then-multiply variant (FNV-1a) has better avalanche properties
 * on short keys than the multiply-then-XOR (FNV-1) variant.
 */
uint32_t _pg_hash_func(const void *key, uint32_t keylen)
{
    const uint8_t *data = (const uint8_t *)key;
    uint32_t       hash = 2166136261u;
    uint32_t       i;

    for (i = 0; i < keylen; i++) {
        hash ^= (uint32_t)data[i];
        hash *= 16777619u;
    }
    return hash;
}

/* -----------------------------------------------------------------------
 * Bucket lookup  (linear hashing)
 * ----------------------------------------------------------------------- */

/*
 * _pg_hash_get_bucket_no – map a hash value to a bucket index.
 *
 * Mirrors PG's bucket-selection logic (hashpage.c):
 *
 *   b = hashval & high_mask;
 *   if (b > max_bucket)  b &= low_mask;
 *
 * Correctness argument:
 *   high_mask = 2^k − 1.  A bucket b < 2^(k−1) has already been split
 *   once in the current "round".  A bucket b ≥ 2^(k−1) that hasn't been
 *   opened yet is mapped back to its "sibling" via low_mask = 2^(k−1) − 1.
 */
uint32_t _pg_hash_get_bucket_no(const pg_hash_t *idx, uint32_t hashval)
{
    uint32_t b = hashval & idx->high_mask;

    if (b > idx->max_bucket) {
        b &= idx->low_mask;
    }
    return b;
}

/* -----------------------------------------------------------------------
 * Page helpers
 * ----------------------------------------------------------------------- */

pg_hash_page_t *_pg_hash_page_create(bool is_overflow)
{
    pg_hash_page_t *page = (pg_hash_page_t *)calloc(1, sizeof(pg_hash_page_t));

    if (!page) {
        return NULL;
    }
    /* 主页和溢出页共用同一结构，仅通过 is_overflow 区分角色。 */
    page->is_overflow = is_overflow;
    return page;
}

/*
 * _pg_hash_page_drop – free an entire overflow chain.
 *
 * Walks next_overflow links and frees every tuple as it goes.
 * The caller must not reference *page* after this call.
 */
void _pg_hash_page_drop(pg_hash_page_t *page)
{
    while (page) {
        pg_hash_page_t *next = page->next_overflow;
        uint32_t i;

        for (i = 0; i < page->ntuples; i++) {
            _pg_hash_tuple_drop(page->tuples[i]);
            page->tuples[i] = NULL;
        }
        free(page);
        page = next;
    }
}

/* -----------------------------------------------------------------------
 * Tuple helpers
 * ----------------------------------------------------------------------- */

/*
 * _pg_hash_tuple_create – allocate and populate a new tuple.
 *
 * Makes copies of key and value; the caller owns the originals.
 * value may be NULL / valuelen may be 0 for key-only storage.
 */
pg_hash_tuple_t *_pg_hash_tuple_create(uint32_t    hashval,
                                       const void *key,   uint32_t keylen,
                                       const void *value, uint32_t valuelen)
{
    pg_hash_tuple_t *tup;

    tup = (pg_hash_tuple_t *)malloc(sizeof(pg_hash_tuple_t));
    if (!tup) {
        return NULL;
    }

    tup->t_hashval  = hashval;
    tup->t_keylen   = keylen;
    tup->t_valuelen = valuelen;

    tup->t_key = malloc(keylen);
    if (!tup->t_key) {
        free(tup);
        return NULL;
    }
    memcpy(tup->t_key, key, keylen);

    if (valuelen > 0 && value) {
        tup->t_value = malloc(valuelen);
        if (!tup->t_value) {
            free(tup->t_key);
            free(tup);
            return NULL;
        }
        memcpy(tup->t_value, value, valuelen);
    } else {
        tup->t_value = NULL;
    }

    return tup;
}

void _pg_hash_tuple_drop(pg_hash_tuple_t *tup)
{
    if (!tup) {
        return;
    }
    free(tup->t_key);
    free(tup->t_value);
    free(tup);
}

/* -----------------------------------------------------------------------
 * Bucket-array expansion
 * ----------------------------------------------------------------------- */

/*
 * _pg_hash_expand_table – grow buckets[] to at least *new_size* slots.
 *
 * New slots beyond the old allocation are zero-initialised (NULL buckets).
 * Returns 0 on success, -1 on OOM.
 */
int _pg_hash_expand_table(pg_hash_t *idx, uint32_t new_size)
{
    pg_hash_page_t **nb;

    if (new_size <= idx->bucket_alloc) {
        return 0;
    }

    nb = (pg_hash_page_t **)realloc(idx->buckets,
                                    (size_t)new_size * sizeof(pg_hash_page_t *));
    if (!nb) {
        return -1;
    }

    memset(nb + idx->bucket_alloc, 0,
           (size_t)(new_size - idx->bucket_alloc) * sizeof(pg_hash_page_t *));

    idx->buckets      = nb;
    idx->bucket_alloc = new_size;
    return 0;
}

/* -----------------------------------------------------------------------
 * Index lifecycle
 * ----------------------------------------------------------------------- */

/*
 * pg_hash_create – create and initialise a linear hash index.
 *
 * fill_factor controls the split trigger:
 *   split when n_total > (fill_factor / 100.0) * PAGE_MAX_ITEMS * num_buckets
 * Pass 0 to use PG_HASH_DEFAULT_FILL_FACTOR (75).
 *
 * Initial state (mirrors PG's _hash_init):
 *   1 bucket (bucket 0), max_bucket = 0, high_mask = 1, low_mask = 0
 */
pg_hash_t *pg_hash_create(uint32_t fill_factor)
{
    pg_hash_t *idx;

    if (fill_factor == 0) {
        fill_factor = PG_HASH_DEFAULT_FILL_FACTOR;
    }
    if (fill_factor < PG_HASH_MIN_FILL_FACTOR) {
        fill_factor = PG_HASH_MIN_FILL_FACTOR;
    }
    if (fill_factor > PG_HASH_MAX_FILL_FACTOR) {
        fill_factor = PG_HASH_MAX_FILL_FACTOR;
    }

    /* 第一步: 规范化 fill factor，并创建索引主结构。 */
    idx = (pg_hash_t *)calloc(1, sizeof(pg_hash_t));
    if (!idx) {
        return NULL;
    }

    idx->fill_factor   = fill_factor;
    idx->bucket_alloc  = PG_HASH_INITIAL_BUCKETS;
    idx->buckets = (pg_hash_page_t **)calloc(PG_HASH_INITIAL_BUCKETS,
                                             sizeof(pg_hash_page_t *));
    if (!idx->buckets) {
        free(idx);
        return NULL;
    }

    /* Allocate primary page for bucket 0 */
    /* 第二步: 初始化 bucket 0 的主页。 */
    idx->buckets[0] = _pg_hash_page_create(false);
    if (!idx->buckets[0]) {
        free(idx->buckets);
        free(idx);
        return NULL;
    }

    /*
     * Initial linear-hashing state.
     *
     *   max_bucket = 0   → only bucket 0 exists
     *   high_mask  = 1   → next split will create bucket 1
     *   low_mask   = 0   → overflow from bucket 1 → 0 (until bucket 1 opens)
     *   n_total    = 0
     */
    /* 第三步: 建立 linear hashing 的初始掩码状态。 */
    idx->max_bucket = 0;
    idx->high_mask  = 1;
    idx->low_mask   = 0;
    idx->n_total    = 0;

    return idx;
}

/*
 * pg_hash_drop – destroy the index and free all memory.
 */
void pg_hash_drop(pg_hash_t *idx)
{
    uint32_t i;

    if (!idx) {
        return;
    }

    /* 顺着每个 bucket 的主页与溢出页链表依次释放。 */
    for (i = 0; i <= idx->max_bucket; i++) {
        _pg_hash_page_drop(idx->buckets[i]);
    }
    free(idx->buckets);
    free(idx);
}

/* -----------------------------------------------------------------------
 * Stat accessors
 * ----------------------------------------------------------------------- */

uint32_t pg_hash_size(const pg_hash_t *idx)
{
    return idx ? idx->n_total : 0;
}

uint32_t pg_hash_bucket_count(const pg_hash_t *idx)
{
    return idx ? (idx->max_bucket + 1) : 0;
}
