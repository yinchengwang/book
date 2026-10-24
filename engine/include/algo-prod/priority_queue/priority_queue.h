#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pq {
    void   *data;
    size_t  size;
    size_t  capacity;
    size_t  element_size;
    int   (*compare)(const void *a, const void *b);
    void  (*free_element)(void *element);
} pq_t;

pq_t *pq_create(size_t element_size, int (*compare)(const void *a, const void *b));
pq_t *pq_create_ex(size_t element_size,
                   size_t initial_capacity,
                   int (*compare)(const void *a, const void *b),
                   void (*free_element)(void *element));
pq_t *pq_heapify(const void *array,
                 size_t count,
                 size_t element_size,
                 int (*compare)(const void *a, const void *b));
void pq_destroy(pq_t *pq);
int pq_push(pq_t *pq, const void *element);
int pq_pop(pq_t *pq, void *out);
int pq_top(const pq_t *pq, void *out);
const void *pq_top_ptr(const pq_t *pq);
size_t pq_size(const pq_t *pq);
bool pq_empty(const pq_t *pq);
void pq_clear(pq_t *pq);

#ifdef __cplusplus
}
#endif

#endif /* PRIORITY_QUEUE_H */