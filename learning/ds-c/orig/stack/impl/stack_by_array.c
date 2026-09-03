#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

typedef struct stack {
    int capacity;
    int current_count;
    int *data;
} stack_t;


stack_t *stack_create(int cap)
{
    assert(cap >= 0);

    int malloc_size = sizeof(stack_t) + sizeof(int) * cap;
    stack_t *stack = (stack_t *)malloc(malloc_size);
    if (!stack) {
        printf("malloc stack failed, cap: %d\n", cap);
        return NULL;
    }
    memset(stack, 0, malloc_size);

    stack->capacity = cap;
    stack->current_count = 0;
    stack->data = (int *)(stack + 1);

    return stack;
}

void stack_push(stack_t *stack, int element)
{
    if (!stack) {
        printf("[stack_push] stack is null.\n");
        return;
    }

    if (stack->current_count == stack->capacity) {
        printf("[stack_push] stack is full.\n");
        return;
    }

    stack->data[stack->current_count] = element;
    stack->current_count++;
    printf("[stack_push]: %d.\n", element);
}

bool is_stack_empty(stack_t *stack)
{
    if (!stack) {
        printf("[is_stack_empty] stack is null.\n");
        return -1;
    }

    return stack->current_count == 0;
}

int stack_top(stack_t *stack)
{
    if (!stack) {
        printf("[stack_top] stack is null.\n");
        return -1;
    }

    if (is_stack_empty(stack)) {
        printf("[stack_top] stack is empty.\n");
        return -1;
    }

    return stack->data[stack->current_count - 1];
}

void stack_pop(stack_t *stack)
{
    if (!stack) {
        printf("[stack_pop] stack is null.\n");
        return;
    }

    if (is_stack_empty(stack)) {
        printf("[stack_pop] stack is empty.\n");
        return;
    }

    printf("[stack_pop]: %d.\n", stack->data[stack->current_count - 1]);
    stack->current_count -= 1;
}

int stack_length(stack_t *stack)
{
    if (!stack) {
        printf("[stack_length] stack is null.\n");
        return -1;
    }

    return stack->current_count;
}

void stack_clear(stack_t *stack)
{
    if (!stack) {
        printf("[stack_clear] stack is null.\n");
        return;
    }

    stack->current_count = 0;
}

void stack_destroy(stack_t *stack)
{
    if (!stack) {
        printf("[stack_destroy] stack is null.\n");
        return;
    }

    free(stack);
}

int main()
{
    stack_t *stack = stack_create(5);

    stack_push(stack, 3);
    stack_push(stack, 2);
    stack_push(stack, 1);
    printf("stack length: %d\n", stack_length(stack));

    stack_pop(stack);
    printf("stack length: %d\n", stack_length(stack));

    printf("stack top: %d\n", stack_top(stack));

    stack_destroy(stack);

    return 0;
}