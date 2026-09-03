#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

typedef struct node {
    int val;
    struct node *next;
} node_t;

typedef struct stack {
    int capacity;
    node_t *top;
} stack_t;

stack_t *stack_create()
{
    stack_t *stack = (stack_t *)malloc(sizeof(stack_t));
    if (!stack) {
        printf("malloc stack failed.\n");
        return NULL;
    }

    stack->capacity = 0;
    stack->top = NULL;

    return stack;
}

void stack_push(stack_t *stack, int val)
{
    assert(stack != NULL);

    node_t *new_node = (node_t *)malloc(sizeof(node_t));
    if (new_node == NULL) {
        printf("malloc new node failed.\n");
        return;
    }

    new_node->val = val;
    new_node->next = stack->top;

    stack->top = new_node;
    stack->capacity++;

    // printf("[stack_push]: %d.\n", val);
}

bool is_stack_empty(stack_t *stack)
{
    assert(stack != NULL);

    return stack->capacity == 0;
}

void stack_pop(stack_t *stack)
{
    assert(stack != NULL);

    if (is_stack_empty(stack)) {
        return;
    }

    // printf("[stack_pop]: %d.\n", stack->top->val);
    node_t *to_pop = stack->top;
    stack->top = to_pop->next;
    free(to_pop);
    stack->capacity--;
}

int stack_top(stack_t *stack)
{
    assert(stack != NULL);

    if (is_stack_empty(stack)) {
        return -1;
    }

    return stack->top->val;
}

int stack_length(stack_t *stack)
{
    assert(stack != NULL);

    return stack->capacity;
}

void stack_destroy(stack_t *stack)
{
    assert(stack != NULL);

    if (is_stack_empty(stack)) {
        return;
    }

    node_t *top = stack->top;
    while (top) {
        node_t *next = top->next;
        free(top);
        top = next;
    }

    free(stack);
}

void decimal_convert_binary(int num)
{
    printf("decimal = %d\n", num);

    stack_t *stack = stack_create();
    while (num != 0) {
        int remainder = num % 2;
        stack_push(stack, remainder);
        num /= 2;
    }

    printf("binary = ");
    while (!is_stack_empty(stack)) {
        int top = stack_top(stack);
        stack_pop(stack);
        printf("%d", top);
    }
    printf("\n");

    stack_destroy(stack);
}

int main()
{
    stack_t *stack = stack_create();;

    stack_push(stack, 3);
    stack_push(stack, 2);
    stack_push(stack, 1);
    printf("stack length: %d\n", stack_length(stack));

    stack_pop(stack);
    printf("stack length: %d\n", stack_length(stack));

    printf("stack top: %d\n", stack_top(stack));

    stack_destroy(stack);

    decimal_convert_binary(10);
    decimal_convert_binary(16);

    return 0;
}