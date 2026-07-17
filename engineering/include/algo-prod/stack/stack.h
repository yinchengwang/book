#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stack stack_t;

stack_t *stack_create(size_t element_size);
stack_t *stack_create_ex(size_t element_size,
                         size_t initial_capacity,
                         void (*free_element)(void *element));
void stack_destroy(stack_t *stack);
int stack_push(stack_t *stack, const void *element);
int stack_pop(stack_t *stack, void *out);
int stack_top(const stack_t *stack, void *out);
const void *stack_top_ptr(const stack_t *stack);
int stack_reserve(stack_t *stack, size_t capacity);
int stack_shrink_to_fit(stack_t *stack);
size_t stack_size(const stack_t *stack);
bool stack_empty(const stack_t *stack);
void stack_clear(stack_t *stack);

#ifdef __cplusplus
}
#endif

#endif /* STACK_H */