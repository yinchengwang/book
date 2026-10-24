#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct queue queue_t;

queue_t *queue_create(size_t element_size);
queue_t *queue_create_ex(size_t element_size,
                         size_t initial_capacity,
                         void (*free_element)(void *element));
void queue_destroy(queue_t *queue);
int queue_push(queue_t *queue, const void *element);
int queue_push_batch(queue_t *queue, const void *elements, size_t count);
int queue_pop(queue_t *queue, void *out);
int queue_front(const queue_t *queue, void *out);
int queue_back(const queue_t *queue, void *out);
const void *queue_front_ptr(const queue_t *queue);
const void *queue_back_ptr(const queue_t *queue);
size_t queue_size(const queue_t *queue);
size_t queue_capacity(const queue_t *queue);
bool queue_empty(const queue_t *queue);
void queue_clear(queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */