/**
 * @file calculator.h
 * @brief 计算器头文件
 */
#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_EXPR_LEN 256
#define MAX_STACK_SIZE 100

typedef struct {
    double data[MAX_STACK_SIZE];
    int top;
} Stack;

void stack_init(Stack *s);
int stack_push(Stack *s, double val);
double stack_pop(Stack *s);
double stack_peek(Stack *s);
int stack_is_empty(Stack *s);

double evaluate_expression(const char *expr);
void calculator_loop(void);

#endif // CALCULATOR_H