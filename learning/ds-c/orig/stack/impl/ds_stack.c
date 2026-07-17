/*
 * ds_stack.c —— 数组栈 demo（学习层独立实现）
 *
 * ============================================================
 * 本文件是学习层独立实现的数组栈 demo，用于：
 * 1. 演示栈的核心操作（push、pop、top、empty）
 * 2. 作为括号匹配问题的实现示例（经典面试题）
 * 3. 不依赖 algo-prod 的 stack_t（ds_stack_t vs algo-prod stack_t）
 *
 * 历史：
 * - S1 时 stack.c 存在，依赖 algo-prod/stack/stack.h
 * - S5 实施双轨纪律时删除
 * - S7 重新独立实现（数组栈版）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "ds/stack.h"

#define DS_STACK_INITIAL_CAPACITY 8

/*
 * 数组栈：buffer 储存元素，top 指向栈顶位置（不含）
 * 自动扩容：capacity 满时 realloc 翻倍
 */
typedef struct {
    int *buffer;       /* 元素缓冲区 */
    int top;           /* 栈顶索引（指向第一个空位）*/
    int capacity;      /* 当前容量 */
} ds_stack_t;

static ds_stack_t *ds_stack_create(void);
static void ds_stack_destroy(ds_stack_t *stack);
static int ds_stack_push(ds_stack_t *stack, int value);
static int ds_stack_pop(ds_stack_t *stack, int *value);
static int ds_stack_top(const ds_stack_t *stack, int *value);
static bool ds_stack_empty(const ds_stack_t *stack);
static size_t ds_stack_size(const ds_stack_t *stack);

static ds_stack_t *ds_stack_create(void) {
    ds_stack_t *stack = (ds_stack_t *)calloc(1, sizeof(ds_stack_t));
    if (!stack) {
        return NULL;
    }
    stack->buffer = (int *)calloc(DS_STACK_INITIAL_CAPACITY, sizeof(int));
    if (!stack->buffer) {
        free(stack);
        return NULL;
    }
    stack->top = 0;
    stack->capacity = DS_STACK_INITIAL_CAPACITY;
    return stack;
}

static void ds_stack_destroy(ds_stack_t *stack) {
    if (!stack) {
        return;
    }
    free(stack->buffer);
    free(stack);
}

static int ds_stack_grow(ds_stack_t *stack) {
    int new_capacity = stack->capacity * 2;
    int *new_buffer = (int *)realloc(stack->buffer, new_capacity * sizeof(int));
    if (!new_buffer) {
        return -1;
    }
    stack->buffer = new_buffer;
    stack->capacity = new_capacity;
    return 0;
}

static int ds_stack_push(ds_stack_t *stack, int value) {
    if (!stack) {
        return -1;
    }
    if (stack->top >= stack->capacity) {
        if (ds_stack_grow(stack) != 0) {
            return -1;
        }
    }
    stack->buffer[stack->top++] = value;
    return 0;
}

static int ds_stack_pop(ds_stack_t *stack, int *value) {
    if (!stack || !value || stack->top == 0) {
        return -1;
    }
    *value = stack->buffer[--stack->top];
    return 0;
}

static int ds_stack_top(const ds_stack_t *stack, int *value) {
    if (!stack || !value || stack->top == 0) {
        return -1;
    }
    *value = stack->buffer[stack->top - 1];
    return 0;
}

static bool ds_stack_empty(const ds_stack_t *stack) {
    return !stack || stack->top == 0;
}

static size_t ds_stack_size(const ds_stack_t *stack) {
    return stack ? (size_t)stack->top : 0;
}

/*
 * 括号匹配（经典面试题）：输入包含 ()[]{} 的字符串，判断是否合法
 * 算法：左括号入栈，右括号出栈匹配；最终栈空且全部匹配
 * 返回：true=匹配，false=不匹配
 */
static bool ds_stack_match_brackets(const char *s) {
    ds_stack_t *stack = ds_stack_create();
    if (!stack) {
        return false;
    }
    bool ok = true;
    for (const char *p = s; *p && ok; p++) {
        char c = *p;
        if (c == '(' || c == '[' || c == '{') {
            /* 左括号：编码后入栈（用 1/2/3 表示左括号）*/
            int code = (c == '(') ? 1 : (c == '[') ? 2 : 3;
            ds_stack_push(stack, code);
        } else if (c == ')' || c == ']' || c == '}') {
            /* 右括号：出栈匹配 */
            int code;
            int expect = (c == ')') ? 1 : (c == ']') ? 2 : 3;
            if (ds_stack_pop(stack, &code) != 0 || code != expect) {
                ok = false;
            }
        }
    }
    /* 最终栈必须为空，且全过程未出错 */
    ok = ok && ds_stack_empty(stack);
    ds_stack_destroy(stack);
    return ok;
}

/*
 * 演示函数：展示栈的核心操作 + 括号匹配
 */
void ds_stack_demo(void) {
    printf("\n=== ds_stack_demo (数组栈 + 括号匹配) ===\n");

    /* 基础操作演示 */
    ds_stack_t *stack = ds_stack_create();
    if (!stack) {
        printf("  创建失败\n");
        return;
    }

    ds_stack_push(stack, 10);
    ds_stack_push(stack, 20);
    ds_stack_push(stack, 30);

    int v;
    ds_stack_top(stack, &v);
    printf("  压入 10/20/30 后，top=%d（期望 30）\n", v);
    printf("  size=%zu\n", ds_stack_size(stack));

    ds_stack_pop(stack, &v);
    printf("  出栈一次，值=%d\n", v);
    ds_stack_top(stack, &v);
    printf("  当前 top=%d\n", v);
    ds_stack_destroy(stack);

    /* 括号匹配演示 */
    printf("  --- 括号匹配 ---\n");
    const char *cases[] = {
        "()[]{}",        /* true  */
        "([{}])",        /* true  */
        "(]",            /* false */
        "([)]",          /* false */
        "{[]}",          /* true  */
        "(((((",         /* false (栈非空) */
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        bool result = ds_stack_match_brackets(cases[i]);
        printf("  %-12s -> %s\n", cases[i], result ? "OK" : "FAIL");
    }
    printf("=== ds_stack_demo 结束 ===\n\n");
}
