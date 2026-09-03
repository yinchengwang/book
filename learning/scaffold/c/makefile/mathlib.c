#include "mathlib.h"
#include <math.h>
#include <stdio.h>

static int s_inited = 0;

void mathlib_init(void) {
    if (!s_inited) {
        fprintf(stderr, "[mathlib] init\n");
        s_inited = 1;
    }
}

double mathlib_pi(void) {
    return 3.14159265358979323846;
}

int mathlib_factorial(int n) {
    if (n < 0) return -1;
    int r = 1;
    for (int i = 2; i <= n; i++) r *= i;
    return r;
}

int mathlib_fibonacci(int n) {
    if (n < 0) return -1;
    if (n == 0) return 0;
    if (n == 1) return 1;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int t = a + b;
        a = b;
        b = t;
    }
    return b;
}

int mathlib_gcd(int a, int b) {
    while (b) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a < 0 ? -a : a;
}
