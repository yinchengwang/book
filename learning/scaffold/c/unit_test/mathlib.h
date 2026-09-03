/* mathlib.h — 被测试的简单数学库 */

#ifndef MATHLIB_H
#define MATHLIB_H

/* 计算阶乘 n!，n 为负数返回 -1 */
int factorial(int n);

/* 判断 n 是否为质数（n <= 1 返回 0） */
int is_prime(int n);

/* 最大公约数（欧几里得算法） */
int gcd(int a, int b);

/* 计算 base^exp（整数幂，非负指数） */
int power(int base, int exp);

#endif /* MATHLIB_H */
