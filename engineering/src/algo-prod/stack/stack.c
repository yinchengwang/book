#include <algo-prod/stack/stack.h>

#include <stdlib.h>
#include <string.h>

#define STACK_DEFAULT_CAPACITY 16u
#define STACK_GROWTH_FACTOR 2u

struct stack {
    void *data;
    size_t size;
    size_t capacity;
    size_t element_size;
    void (*free_element)(void *element);
};

static unsigned char *stack_elem(stack_t *stack, size_t index)
{
    return (unsigned char *)stack->data + index * stack->element_size;
}

static const unsigned char *stack_elem_const(const stack_t *stack, size_t index)
{
    return (const unsigned char *)stack->data + index * stack->element_size;
}

static int stack_set_capacity(stack_t *stack, size_t new_capacity)
{
    void *new_data;

    if (!stack) {
        return -1;
    }
    if (new_capacity < stack->size) {
        return -1;
    }

    if (new_capacity == 0u) {
        free(stack->data);
        stack->data = NULL;
        stack->capacity = 0u;
        return 0;
    }

    new_data = realloc(stack->data, new_capacity * stack->element_size);
    if (!new_data) {
        return -1;
    }

    stack->data = new_data;
    stack->capacity = new_capacity;
    return 0;
}

static int stack_grow(stack_t *stack)
{
    size_t new_capacity;

    new_capacity = stack->capacity * STACK_GROWTH_FACTOR;
    if (new_capacity == stack->capacity) {
        new_capacity = stack->capacity + 1u;
    }

    return stack_set_capacity(stack, new_capacity);
}

stack_t *stack_create(size_t element_size)
{
    return stack_create_ex(element_size, 0u, NULL);
}

stack_t *stack_create_ex(size_t element_size,
                         size_t initial_capacity,
                         void (*free_element)(void *element))
{
    stack_t *stack;

    if (element_size == 0u) {
        return NULL;
    }
    if (initial_capacity == 0u) {
        initial_capacity = STACK_DEFAULT_CAPACITY;
    }

    stack = (stack_t *)calloc(1, sizeof(stack_t));
    if (!stack) {
        return NULL;
    }

    stack->data = malloc(initial_capacity * element_size);
    if (!stack->data) {
        free(stack);
        return NULL;
    }

    stack->capacity = initial_capacity;
    stack->element_size = element_size;
    stack->free_element = free_element;
    return stack;
}

void stack_destroy(stack_t *stack)
{
    if (!stack) {
        return;
    }

    stack_clear(stack);
    free(stack->data);
    free(stack);
}

int stack_push(stack_t *stack, const void *element)
{
    if (!stack || !element) {
        return -1;
    }
    if (stack->size >= stack->capacity && stack_grow(stack) != 0) {
        return -1;
    }

    memcpy(stack_elem(stack, stack->size), element, stack->element_size);
    stack->size += 1u;
    return 0;
}

int stack_reserve(stack_t *stack, size_t capacity)
{
    if (!stack) {
        return -1;
    }
    if (capacity <= stack->capacity) {
        return 0;
    }

    return stack_set_capacity(stack, capacity);
}

int stack_shrink_to_fit(stack_t *stack)
{
    if (!stack) {
        return -1;
    }

    return stack_set_capacity(stack, stack->size);
}

int stack_pop(stack_t *stack, void *out)
{
    unsigned char *top;

    if (!stack || stack->size == 0u) {
        return -1;
    }

    stack->size -= 1u;
    top = stack_elem(stack, stack->size);
    if (out) {
        memcpy(out, top, stack->element_size);
    } else if (stack->free_element) {
        stack->free_element(top);
    }
    return 0;
}

int stack_top(const stack_t *stack, void *out)
{
    if (!stack || stack->size == 0u || !out) {
        return -1;
    }

    memcpy(out, stack_elem_const(stack, stack->size - 1u), stack->element_size);
    return 0;
}

const void *stack_top_ptr(const stack_t *stack)
{
    if (!stack || stack->size == 0u) {
        return NULL;
    }

    return stack_elem_const(stack, stack->size - 1u);
}

size_t stack_size(const stack_t *stack)
{
    return stack ? stack->size : 0u;
}

bool stack_empty(const stack_t *stack)
{
    return !stack || stack->size == 0u;
}

void stack_clear(stack_t *stack)
{
    if (!stack) {
        return;
    }

    if (stack->free_element) {
        while (stack->size > 0u) {
            stack->size -= 1u;
            stack->free_element(stack_elem(stack, stack->size));
        }
    } else {
        stack->size = 0u;
    }
}