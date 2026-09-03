/* mathlib.c — 简单数学库实现 */

#include "mathlib.h"

int factorial(int n) {
    if (n < 0) return -1;  /* 错误：未定义 */
    if (n <= 1) return 1;
    int result = 1;
    for (int i = 2; i <= n; i++) result *= i;
    return result;
}

int is_prime(int n) {
    if (n <= 1) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
    for (int i = 3; i * i <= n; i += 2)
        if (n % i == 0) return 0;
    return 1;
}

int gcd(int a, int b) {
    /* 欧几里得算法：gcd(a,b) = gcd(b, a%b)，直到 b=0 */
    a = (a < 0) ? -a : a;
    b = (b < 0) ? -b : b;
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

int power(int base, int exp) {
    if (exp < 0) return -1;  /* 不支持负指数 */
    if (exp == 0) return 1;
    int result = 1;
    for (int i = 0; i < exp; i++) result *= base;
    return result;
}
