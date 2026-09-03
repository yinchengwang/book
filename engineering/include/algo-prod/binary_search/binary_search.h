#ifndef BINARY_SEARCH_H
#define BINARY_SEARCH_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*binary_search_compare_fn)(const void *lhs, const void *rhs);

bool binary_search(const void *base,
                   size_t count,
                   size_t element_size,
                   const void *key,
                   binary_search_compare_fn compare,
                   size_t *index);
size_t binary_search_lower_bound(const void *base,
                                 size_t count,
                                 size_t element_size,
                                 const void *key,
                                 binary_search_compare_fn compare);
size_t binary_search_upper_bound(const void *base,
                                 size_t count,
                                 size_t element_size,
                                 const void *key,
                                 binary_search_compare_fn compare);

#ifdef __cplusplus
}
#endif

#endif /* BINARY_SEARCH_H */