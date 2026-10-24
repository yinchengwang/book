#ifndef LIST_H
#define LIST_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct list list_t;
typedef struct list_node list_node_t;

list_t *list_create(size_t element_size);
list_t *list_create_ex(size_t element_size, void (*free_element)(void *element));
void list_destroy(list_t *list);
int list_push_front(list_t *list, const void *element);
int list_push_back(list_t *list, const void *element);
int list_pop_front(list_t *list, void *out);
int list_pop_back(list_t *list, void *out);
const void *list_front_ptr(const list_t *list);
const void *list_back_ptr(const list_t *list);
const list_node_t *list_front_node(const list_t *list);
const list_node_t *list_back_node(const list_t *list);
const list_node_t *list_node_next(const list_node_t *node);
const list_node_t *list_node_prev(const list_node_t *node);
const void *list_node_value(const list_node_t *node);
size_t list_size(const list_t *list);
bool list_empty(const list_t *list);
void list_clear(list_t *list);

#ifdef __cplusplus
}
#endif

#endif /* LIST_H */