/**
 * @file calculator.c
 * @brief 计算器核心实现
 */
#include "calculator.h"

void stack_init(Stack *s) {
    s->top = -1;
}

int stack_push(Stack *s, double val) {
    if (s->top >= MAX_STACK_SIZE - 1) {
        return -1;
    }
    s->data[++s->top] = val;
    return 0;
}

double stack_pop(Stack *s) {
    if (s->top < 0) {
        return 0;
    }
    return s->data[s->top--];
}

int stack_is_empty(Stack *s) {
    return s->top < 0;
}

double stack_peek(Stack *s) {
    if (s->top < 0) {
        return 0;
    }
    return s->data[s->top];
}

int precedence(char op) {
    switch (op) {
        case '+':
        case '-':
            return 1;
        case '*':
        case '/':
            return 2;
        case '^':
            return 3;
        default:
            return 0;
    }
}

double apply_op(double a, double b, char op) {
    switch (op) {
        case '+':
            return a + b;
        case '-':
            return a - b;
        case '*':
            return a * b;
        case '/':
            return a / b;
        case '^':
            return pow(a, b);
        default:
            return 0;
    }
}

double evaluate_expression(const char *expr) {
    Stack values;
    Stack ops;
    stack_init(&values);
    stack_init(&ops);

    int len = strlen(expr);
    int i = 0;

    while (i < len) {
        if (isspace(expr[i])) {
            i++;
            continue;
        }

        if (expr[i] == '(') {
            stack_push(&ops, '(');
            i++;
        } else if (expr[i] == ')') {
            while (!stack_is_empty(&ops) && (char)stack_pop(&ops) != '(') {
                double val2 = stack_pop(&values);
                double val1 = stack_pop(&values);
                char op = (char)stack_pop(&ops);
                stack_push(&values, apply_op(val1, val2, op));
            }
            stack_pop(&ops);
            i++;
        } else if (isdigit(expr[i]) || expr[i] == '.') {
            char *end;
            double val = strtod(&expr[i], &end);
            stack_push(&values, val);
            i += (end - &expr[i]);
        } else if (strchr("+-*/^", expr[i])) {
            while (!stack_is_empty(&ops) &&
                   precedence((char)stack_peek(&ops)) >= precedence(expr[i])) {
                double val2 = stack_pop(&values);
                double val1 = stack_pop(&values);
                char op = (char)stack_pop(&ops);
                stack_push(&values, apply_op(val1, val2, op));
            }
            stack_push(&ops, expr[i]);
            i++;
        }
    }

    while (!stack_is_empty(&ops)) {
        double val2 = stack_pop(&values);
        double val1 = stack_pop(&values);
        char op = (char)stack_pop(&ops);
        stack_push(&values, apply_op(val1, val2, op));
    }

    return stack_pop(&values);
}

void calculator_loop(void) {
    char input[MAX_EXPR_LEN];

    printf("=== 计算器 ===\n");
    printf("支持: +, -, *, /, ^, ()\n");
    printf("输入 'quit' 退出\n\n");

    while (1) {
        printf("> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "quit") == 0 || strcmp(input, "q") == 0) {
            printf("再见!\n");
            break;
        }

        if (strlen(input) == 0) {
            continue;
        }

        double result = evaluate_expression(input);
        printf("= %g\n", result);
    }
}