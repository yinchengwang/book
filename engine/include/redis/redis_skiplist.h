/* redis's a skiplist implementation */

#ifndef REDIS_SKIPLIST_H
#define REDIS_SKIPLIST_H

#include "redis/redis_sds.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct redis_skiplist_node {
    redis_sds ele;
    double score;
    struct redis_skiplist_node *backward;
    struct redis_skiplist_level {
        struct redis_skiplist_node *forward;
        unsigned long span;
    } level[];
} redis_skiplist_node_t;

typedef struct redis_skiplist {
    redis_skiplist_node_t *header;
    redis_skiplist_node_t *tail;
    unsigned long length;
    int level;
} redis_skiplist_t;


#ifdef __cplusplus
}
#endif // extern "C"

#endif // REDIS_SKIPLIST_H