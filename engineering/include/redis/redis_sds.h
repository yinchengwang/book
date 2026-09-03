/* redis's A simple dynamic strings */

#ifndef REDIS_SDS_H
#define REDIS_SDS_H

#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REDIS_SDS_MAX_PREALLOC (1024*1024)

extern const char *REDIS_SDS_NOINIT;

typedef char *redis_sds;

/* Note: sds_hdr5 is never used, we just access the flags byte directly.
 * However is here to document the layout of type 5 SDS strings.
 */
struct __attribute__ ((__packed__)) redis_sds_hdr5 {
    unsigned char flags; /* 3 lsb of type, and 5 msb of string length */
    char buf[];
}; 

struct __attribute__ ((__packed__)) redis_sds_hdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

struct __attribute__ ((__packed__)) redis_sds_hdr16 {
    uint16_t len; /* used */
    uint16_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

struct __attribute__ ((__packed__)) redis_sds_hdr32 {
    uint32_t len; /* used */
    uint32_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

struct __attribute__ ((__packed__)) redis_sds_hdr64 {
    uint64_t len; /* used */
    uint64_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

#define REDIS_SDS_TYPE_5  0
#define REDIS_SDS_TYPE_8  1
#define REDIS_SDS_TYPE_16 2
#define REDIS_SDS_TYPE_32 3
#define REDIS_SDS_TYPE_64 4
#define REDIS_SDS_TYPE_MASK 7

/*
 *低地址 -> [ len | alloc | flags ] [ buf[0] | buf[1] | ... ] <- 高地址
 *          ^                     ^
 *          |                     |
 *          sh                    s
 */
#define REDIS_SDS_HDR_VAR(T, s) \
    struct redis_sds_hdr##T *sh = (struct redis_sds_hdr##T *)((s) - (sizeof(struct redis_sds_hdr##T)));

#define REDIS_SDS_HDR(T, s) ((struct redis_sds_hdr##T *)((s)-(sizeof(struct redis_sds_hdr##T))))

#define REDIS_SDS_TYPE_BITS 3
/* Structure of flags 
 * +---------+--------+
 * | type(3) | len(4) |
 * +---------+--------+
 */
#define REDIS_SDS_TYPE_5_LEN(f) ((f) >> REDIS_SDS_TYPE_BITS)


static inline size_t redis_sds_len(const redis_sds s) {
    unsigned char flags = s[-1];
    switch(flags & REDIS_SDS_TYPE_MASK) {
        case REDIS_SDS_TYPE_5:
            return REDIS_SDS_TYPE_5_LEN(flags);
        case REDIS_SDS_TYPE_8:      
            return REDIS_SDS_HDR(8,s)->len;
        case REDIS_SDS_TYPE_16:
            return REDIS_SDS_HDR(16,s)->len;
        case REDIS_SDS_TYPE_32:
            return REDIS_SDS_HDR(32,s)->len;
        case REDIS_SDS_TYPE_64:
            return REDIS_SDS_HDR(64,s)->len;
    }
    return 0;
}

static inline size_t redis_sds_avail(const redis_sds s) {
    unsigned char flags = s[-1];
    switch(flags & REDIS_SDS_TYPE_MASK) {
        case REDIS_SDS_TYPE_5: {
            return 0;
        }
        case REDIS_SDS_TYPE_8: {
            REDIS_SDS_HDR_VAR(8, s);
            return sh->alloc - sh->len;
        }
        case REDIS_SDS_TYPE_16: {
            REDIS_SDS_HDR_VAR(16, s);
            return sh->alloc - sh->len;
        }
        case REDIS_SDS_TYPE_32: {
            REDIS_SDS_HDR_VAR(32, s);
            return sh->alloc - sh->len;
        }
        case REDIS_SDS_TYPE_64: {
            REDIS_SDS_HDR_VAR(64, s);
            return sh->alloc - sh->len;
        }
    }
    return 0;
}

static inline void redis_sds_set_len(redis_sds s, size_t newlen) {
    unsigned char flags = s[-1];
    switch(flags & REDIS_SDS_TYPE_MASK) {
        case REDIS_SDS_TYPE_5:
            {
                unsigned char *fp = ((unsigned char *)s) - 1;
                *fp = REDIS_SDS_TYPE_5 | (newlen << REDIS_SDS_TYPE_BITS);
            }
            break;
        case REDIS_SDS_TYPE_8:
            REDIS_SDS_HDR(8,s)->len = newlen;
            break;
        case REDIS_SDS_TYPE_16:
            REDIS_SDS_HDR(16,s)->len = newlen;
            break;
        case REDIS_SDS_TYPE_32:
            REDIS_SDS_HDR(32,s)->len = newlen;
            break;
        case REDIS_SDS_TYPE_64:
            REDIS_SDS_HDR(64,s)->len = newlen;
            break;
    }
}

static inline void redis_sds_inc_len(redis_sds s, size_t inc) {
    unsigned char flags = s[-1];
    switch(flags & REDIS_SDS_TYPE_MASK) {
        case REDIS_SDS_TYPE_5:
            {
                unsigned char *fp = ((unsigned char *)s) - 1;
                unsigned char newlen = REDIS_SDS_TYPE_5_LEN(flags) + inc;
                *fp = REDIS_SDS_TYPE_5 | (newlen << REDIS_SDS_TYPE_BITS);
            }
            break;
        case REDIS_SDS_TYPE_8:
            REDIS_SDS_HDR(8, s)->len += inc;
            break;
        case REDIS_SDS_TYPE_16:
            REDIS_SDS_HDR(16, s)->len += inc;
            break;
        case REDIS_SDS_TYPE_32:
            REDIS_SDS_HDR(32, s)->len += inc;
            break;
        case REDIS_SDS_TYPE_64:
            REDIS_SDS_HDR(64, s)->len += inc;
            break;
    }
}

/* alloc = avail + len */
static inline size_t redis_sds_alloc(const redis_sds s) {
    unsigned char flags = s[-1];
    switch(flags & REDIS_SDS_TYPE_MASK) {
        case REDIS_SDS_TYPE_5:
            return REDIS_SDS_TYPE_5_LEN(flags);
        case REDIS_SDS_TYPE_8:
            return REDIS_SDS_HDR(8, s)->alloc;
        case REDIS_SDS_TYPE_16:
            return REDIS_SDS_HDR(16, s)->alloc;
        case REDIS_SDS_TYPE_32:
            return REDIS_SDS_HDR(32, s)->alloc;
        case REDIS_SDS_TYPE_64:
            return REDIS_SDS_HDR(64, s)->alloc;
    }
    return 0;
}

static inline void redis_sds_set_alloc(redis_sds s, size_t newlen) {
    unsigned char flags = s[-1];
    switch(flags & REDIS_SDS_TYPE_MASK) {
        case REDIS_SDS_TYPE_5:
            /* Nothing to do, this type has no total allocation info. */
            break;
        case REDIS_SDS_TYPE_8:
            REDIS_SDS_HDR(8,s)->alloc = newlen;
            break;
        case REDIS_SDS_TYPE_16:
            REDIS_SDS_HDR(16,s)->alloc = newlen;
            break;
        case REDIS_SDS_TYPE_32:
            REDIS_SDS_HDR(32,s)->alloc = newlen;
            break;
        case REDIS_SDS_TYPE_64:
            REDIS_SDS_HDR(64,s)->alloc = newlen;
            break;
    }
}

/* init a new redis sds */
redis_sds redis_sds_new_len(const void *init, size_t init_len);

redis_sds redis_sds_try_new_len(const void *init, size_t init_len);

redis_sds redis_sds_new(const char *init);

#ifdef __cplusplus
}
#endif // extern "C"

#endif // REDIS_SDS_H