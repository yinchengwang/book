/* redis's A simple dynamic strings */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>

#include "redis/redis_sds.h"
#include "sdsalloc.h"
#include "util.h"

const char *SDS_NOINIT = "REDIS_SDS_NOINIT";

static inline int redis_sds_hdr_size(char type)
{
    switch(type & SDS_TYPE_MASK) {
        case REDIS_SDS_TYPE_5:
            return sizeof(struct redis_sds_hdr5);
        case REDIS_SDS_TYPE_8:
            return sizeof(struct redis_sds_hdr8);
        case REDIS_SDS_TYPE_16:
            return sizeof(struct redis_sds_hdr16);
        case REDIS_SDS_TYPE_32:
            return sizeof(struct redis_sds_hdr32);
        case REDIS_SDS_TYPE_64:
            return sizeof(struct redis_sds_hdr64);
    }

    return 0;
}

static inline char redis_sds_req_type(size_t string_size)
{
    if (string_size < 1 << 5) {
        return REDIS_SDS_TYPE_5;
    } else if (string_size < 1 << 8) {
        return REDIS_SDS_TYPE_8;
    } else if (string_size < 1 << 16) {
        return REDIS_SDS_TYPE_16;
    /* check 64-bit environment */
#if (LONG_MAX == LLONG_MAX)
    } else if (string_size < 1ll << 32)
        return REDIS_SDS_TYPE_32;
    } else {
        return REDIS_SDS_TYPE_64;
    }
#else
    } else {
        return SDS_TYPE_32;
    }
#endif
}

static inline size_t redis_sds_type_max_size(char type)
{
    size_t size;
    switch (type) {
        case REDIS_SDS_TYPE_5:
            size = (1 << 5) - 1;
            break;
        case REDIS_SDS_TYPE_8:
            size = (1 << 8) - 1;
            break;
        case REDIS_SDS_TYPE_16:
            size = (1 << 16) - 1;
            break;
#if (LONG_MAX == LLONG_MAX)
        case REDIS_SDS_TYPE_32:
            size = (1ll << 32) - 1;
            break;
#endif
        default:
            /* Redis upgrades the SDS type when the string length exceeds the theoretical max of the current type */
            /* The unsigned is automatically converted to the maximum value */
            size = -1;
            break;
    }
}

void _redis_sds_init_hdr(redis_sds s, char type, size_t init_len, size_t usable)
{
    /* flags pointer. */
    unsigned char *fp = ((unsigned char *)s) - 1;

    switch(type) {
        case REDIS_SDS_TYPE_5: {
            *fp = type | (init_len << REDIS_SDS_TYPE_BITS);
            break;
        }
        case REDIS_SDS_TYPE_8: {
            REDIS_SDS_HDR_VAR(8, s);
            sh->len = init_len;
            sh->alloc = usable;
            *fp = type;
            break;
        }
        case REDIS_SDS_TYPE_16: {
            REDIS_SDS_HDR_VAR(16, s);
            sh->len = init_len;
            sh->alloc = usable;
            *fp = type;
            break;
        }
        case REDIS_SDS_TYPE_32: {
            REDIS_SDS_HDR_VAR(32, s);
            sh->len = init_len;
            sh->alloc = usable;
            *fp = type;
            break;
        }
        case REDIS_SDS_TYPE_64: {
            REDIS_SDS_HDR_VAR(64, s);
            sh->len = init_len;
            sh->alloc = usable;
            *fp = type;
            break;
        }
    }
}

/* 存储结构
 * +---------+---------+---------+---------+---+----+
 * |   len   |  alloc  |  flags  |   data...   | \0 |
 * +---------+---------+---------+---------+---+----+
 * ^         ^         ^         ^             ^
 * |         |         |         |             |
 * sh        sh+1      sh+2      s (返回给用户)  s+initlen
 */
redis_sds _redis_sds_new_len(const void *init, size_t init_len, int try_malloc)
{
    void *sh;
    redis_sds s;

    char type = redis_sds_req_type(init_len);
    /* Empty strings are usually created in order to append. Use type 8
     * since type 5 is not good at this.
     */
    if (type == REDIS_SDS_TYPE_5 && init_len == 0) {
        type = REDIS_SDS_TYPE_8;
    }

    int hdr_len = redis_sds_hdr_size(type);

    size_t malloc_size = init_len + hdr_len + 1;
    /* Catch size_t overflow */
    assert(malloc_size > init_len);

    size_t usable;
    sh = try_malloc ? s_trymalloc_usable(malloc_size, &usable) : s_malloc_usable(malloc_size, &usable);
    if (sh == NULL) {
        return NULL;
    }

    if (init == SDS_NOINIT) {
        init = NULL;
    } else if (!init) {
        memset(sh, 0, malloc_size);
    }

    s = sh + hdr_len;
    /* 1 bit is reserved for the terminator '\0' */
    usable = usable - hdr_len - 1;
    if (usable > redis_sds_type_max_size(type)) {
        usable = redis_sds_type_max_size(type);
    }

    _redis_sds_init_hdr(s, type, init_len, usable);

    if (init_len && init) {
        memcpy(s, init, init_len);
    }
    s[init_len] = '\0';
    return s;
}

redis_sds redis_sds_new_len(const void *init, size_t init_len)
{
    return _redis_sds_new_len(init, init_len, 0);
}

redis_sds redis_sds_try_new_len(const void *init, size_t init_len)
{
    return _redis_sds_new_len(init, init_len, 1);
}

/* Create a new sds string starting from a null terminated C string. */
redis_sds redis_sds_new(const char *init)
{
    size_t init_len = (init == NULL) ? 0 : strlen(init);
    return redis_sds_new_len(init, init_len);
}