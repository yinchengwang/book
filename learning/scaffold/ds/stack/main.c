/**
 * @file main.c
 * @brief 数据结构栈学习卡片 - 演示顺序栈、链式栈与表达式求值
 *
 * 涵盖内容：
 * - 顺序栈（数组实现）：Push/Pop/Top/Size
 * - 链式栈（链表实现）：无容量限制
 * - 栈的应用：中缀表达式转后缀表达式（逆波兰表示法）
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* ==================== 顺序栈（数组实现） ==================== */

/**
 * 顺序栈结构
 * 使用动态数组实现，支持自动扩容
 */
#define STACK_INIT_CAPACITY 4

typedef struct {
    int *data;      /* 数据存储（动态数组） */
    size_t size;    /* 当前元素数 */
    size_t capacity; /* 容量 */
} ArrayStack;

/**
 * 创建顺序栈
 */
static ArrayStack *astack_create(void)
{
    ArrayStack *s = (ArrayStack *)malloc(sizeof(ArrayStack));
    if (!s) return NULL;
    s->data = (int *)malloc(STACK_INIT_CAPACITY * sizeof(int));
    if (!s->data) { free(s); return NULL; }
    s->size = 0;
    s->capacity = STACK_INIT_CAPACITY;
    return s;
}

/**
 * 释放顺序栈
 */
static void astack_free(ArrayStack *s)
{
    if (s) {
        free(s->data);
        free(s);
    }
}

/**
 * 顺序栈扩容（2倍策略）
 */
static int astack_resize(ArrayStack *s)
{
    size_t new_cap = s->capacity * 2;
    int *new_data = (int *)realloc(s->data, new_cap * sizeof(int));
    if (!new_data) return -1;
    printf("  [resize] 容量 %zu -> %zu\n", s->capacity, new_cap);
    s->data = new_data;
    s->capacity = new_cap;
    return 0;
}

/**
 * 入栈 Push
 */
static int astack_push(ArrayStack *s, int value)
{
    if (s->size >= s->capacity) {
        if (astack_resize(s) != 0) return -1;
    }
    s->data[s->size++] = value;
    return 0;
}

/**
 * 出栈 Pop（返回弹出的值）
 */
static int astack_pop(ArrayStack *s)
{
    if (s->size == 0) return -1;
    return s->data[--s->size];
}

/**
 * 获取栈顶元素（不出栈）
 */
static int astack_top(const ArrayStack *s)
{
    if (s->size == 0) return -1;
    return s->data[s->size - 1];
}

static void astack_print(const ArrayStack *s)
{
    printf("  栈内容 (top -> bottom): [");
    if (s->size > 0) {
        for (size_t i = s->size; i > 0; i--) {
            printf("%d%s", s->data[i - 1], i > 1 ? ", " : "");
        }
    }
    printf("], size=%zu\n", s->size);
}

/* ==================== 链式栈（链表实现） ==================== */

/**
 * 链式栈节点
 */
typedef struct ListNode {
    int value;
    struct ListNode *next;
} ListNode;

/**
 * 链式栈结构
 * 栈顶在链表头部，入栈就是头插，出栈就是头删
 */
typedef struct {
    ListNode *top;   /* 栈顶指针（链表头） */
    size_t size;     /* 元素数 */
} ListStack;

/**
 * 创建链式栈
 */
static ListStack *lstack_create(void)
{
    ListStack *s = (ListStack *)malloc(sizeof(ListStack));
    if (!s) return NULL;
    s->top = NULL;
    s->size = 0;
    return s;
}

/**
 * 释放链式栈
 */
static void lstack_free(ListStack *s)
{
    if (!s) return;
    ListNode *cur = s->top;
    while (cur) {
        ListNode *next = cur->next;
        free(cur);
        cur = next;
    }
    free(s);
}

/**
 * 入栈 Push（头插法）
 */
static int lstack_push(ListStack *s, int value)
{
    ListNode *node = (ListNode *)malloc(sizeof(ListNode));
    if (!node) return -1;
    node->value = value;
    node->next = s->top;
    s->top = node;
    s->size++;
    return 0;
}

/**
 * 出栈 Pop（头删法）
 */
static int lstack_pop(ListStack *s)
{
    if (s->size == 0) return -1;
    ListNode *node = s->top;
    int value = node->value;
    s->top = node->next;
    free(node);
    s->size--;
    return value;
}

static void lstack_print(const ListStack *s)
{
    printf("  栈内容 (top -> bottom): [");
    ListNode *cur = s->top;
    bool first = true;
    while (cur) {
        printf("%s%d", first ? "" : ", ", cur->value);
        first = false;
        cur = cur->next;
    }
    printf("], size=%zu\n", s->size);
}

/* ==================== 顺序栈演示 ==================== */

static void demo_array_stack(void)
{
    printf("[stack] 顺序栈（数组实现）演示\n");

    ArrayStack *s = astack_create();
    printf("  创建空栈\n");

    /* 入栈演示 */
    printf("  Push: 10, 20, 30\n");
    astack_push(s, 10);
    astack_push(s, 20);
    astack_push(s, 30);
    astack_print(s);

    /* 获取栈顶 */
    printf("  Top() = %d\n", astack_top(s));

    /* 出栈演示 */
    printf("  Pop() = %d\n", astack_pop(s));
    printf("  Pop() = %d\n", astack_pop(s));
    astack_print(s);

    /* 触发扩容 */
    printf("  Push: 40, 50, 60, 70, 80\n");
    astack_push(s, 40);
    astack_push(s, 50);
    astack_push(s, 60);
    astack_push(s, 70);
    astack_push(s, 80);
    astack_print(s);

    astack_free(s);
    printf("\n");
}

/* ==================== 链式栈演示 ==================== */

static void demo_linked_stack(void)
{
    printf("[stack] 链式栈（链表实现）演示\n");

    ListStack *s = lstack_create();
    printf("  创建空栈\n");

    /* 入栈演示 */
    printf("  Push: 100, 200, 300\n");
    lstack_push(s, 100);
    lstack_push(s, 200);
    lstack_push(s, 300);
    lstack_print(s);

    /* 出栈演示 */
    printf("  Pop() = %d\n", lstack_pop(s));
    printf("  Pop() = %d\n", lstack_pop(s));
    lstack_print(s);

    /* 继续操作 */
    printf("  Push: 400, 500\n");
    lstack_push(s, 400);
    lstack_push(s, 500);
    lstack_print(s);

    lstack_free(s);
    printf("\n");
}

/* ==================== 应用：中缀转后缀表达式 ==================== */

/**
 * 判断是否为运算符
 */
static bool is_operator(char c)
{
    return c == '+' || c == '-' || c == '*' || c == '/';
}

/**
 * 获取运算符优先级
 */
static int precedence(char op)
{
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

/**
 * 中缀表达式转后缀表达式（逆波兰表示法）
 * 算法：使用栈存储运算符
 *
 * 规则：
 * - 操作数：直接输出
 * - '('：入栈
 * - ')'：弹栈直到 '('
 * - 运算符：弹出比自己优先级高的运算符，再入栈
 *
 * 示例：2 + 3 * 4  =>  2 3 4 * +
 * 示例：(2 + 3) * 4 =>  2 3 + 4 *
 */
static void infix_to_postfix(const char *infix, char *output, size_t out_size)
{
    ArrayStack *s = astack_create();
    size_t out_idx = 0;

    printf("  转换: \"%s\" => ", infix);

    for (size_t i = 0; infix[i] && out_idx < out_size - 1; i++) {
        char c = infix[i];

        if (isspace(c)) continue;

        if (isdigit(c)) {
            /* 操作数：直接输出 */
            output[out_idx++] = c;
        } else if (c == '(') {
            astack_push(s, '(');
        } else if (c == ')') {
            /* 弹栈直到 '(' */
            while (s->size > 0 && astack_top(s) != '(') {
                output[out_idx++] = (char)astack_pop(s);
            }
            if (s->size > 0) astack_pop(s);  /* 弹出 '(' */
        } else if (is_operator(c)) {
            /* 运算符：弹出比自己优先级高的运算符 */
            while (s->size > 0 && astack_top(s) != '(' &&
                   precedence((char)astack_top(s)) >= precedence(c)) {
                output[out_idx++] = (char)astack_pop(s);
            }
            astack_push(s, c);
        }
    }

    /* 弹出剩余运算符 */
    while (s->size > 0 && out_idx < out_size - 1) {
        output[out_idx++] = (char)astack_pop(s);
    }

    output[out_idx] = '\0';
    astack_free(s);

    printf("\"%s\"\n", output);
}

/**
 * 后缀表达式求值
 */
static int eval_postfix(const char *postfix)
{
    ArrayStack *s = astack_create();

    printf("  求值: \"%s\"\n", postfix);

    for (size_t i = 0; postfix[i]; i++) {
        char c = postfix[i];

        if (isdigit(c)) {
            astack_push(s, c - '0');
            printf("    Push %d\n", c - '0');
        } else if (is_operator(c)) {
            int b = astack_pop(s);
            int a = astack_pop(s);
            int result;
            switch (c) {
                case '+': result = a + b; break;
                case '-': result = a - b; break;
                case '*': result = a * b; break;
                case '/': result = a / b; break;
                default: result = 0;
            }
            printf("    Pop %d %c %d = %d, Push %d\n", a, c, b, result, result);
            astack_push(s, result);
        }
    }

    int result = astack_pop(s);
    astack_free(s);
    return result;
}

static void demo_expression_eval(void)
{
    printf("[stack] 应用：中缀转后缀与表达式求值\n");

    /* 测试用例 */
    const char *expressions[] = {
        "2+3*4",        /* => 14 */
        "(2+3)*4",      /* => 20 */
        "2*3+4*5",      /* => 26 */
        "((2+3)*4-5)/3" /* => 5 */
    };

    char postfix[64];

    for (size_t i = 0; i < sizeof(expressions) / sizeof(expressions[0]); i++) {
        infix_to_postfix(expressions[i], postfix, sizeof(postfix));
        int result = eval_postfix(postfix);
        printf("    结果: %d\n\n", result);
    }
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 栈 ===\n\n");

    demo_array_stack();
    demo_linked_stack();
    demo_expression_eval();

    printf("=== PASS ===\n");
    return 0;
}
