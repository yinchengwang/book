#ifndef SDSALLOC_H
#define SDSALLOC_H

#include <stdlib.h>
#include <string.h>

/* Memory allocation wrappers */
#define s_malloc malloc
#define s_realloc realloc
#define s_free free
#define s_calloc calloc
#define s_strdup strdup

/* Usable memory functions - for systems without malloc_usable_size */
static inline void* s_malloc_usable(size_t size, size_t *usable) {
    void* ptr = malloc(size);
    if (usable && ptr) {
        *usable = size;  /* Simplified: just return requested size */
    }
    return ptr;
}

static inline void* s_trymalloc_usable(size_t size, size_t *usable) {
    return s_malloc_usable(size, usable);
}

static inline void* s_realloc_usable(void *ptr, size_t size, size_t *usable) {
    void* new_ptr = realloc(ptr, size);
    if (usable && new_ptr) {
        *usable = size;  /* Simplified: just return requested size */
    }
    return new_ptr;
}

#endif /* SDSALLOC_H */
