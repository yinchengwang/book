/* pointer scaffold — 指针四件核心：指针/解引用/算术/函数指针 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add_int(int a, int b) { return a + b; }
static int sub_int(int a, int b) { return a - b; }
static int mul_int(int a, int b) { return a * b; }

typedef int (*binop_t)(int, int);

static int dispatch(binop_t op, int x, int y) {
    return op(x, y);
}

int main(void) {
    int x = 42;
    int *p = &x;

    printf("[ptr]    x   = %d (addr=%p)\n",  x,  (void*)&x);
    printf("[ptr]    *p  = %d (p=%p)\n",    *p,  (void*)p);
    *p = 100;
    printf("[ptr]    after *p=100, x = %d\n", x);

    /* 指针算术 */
    int arr[5] = {10, 20, 30, 40, 50};
    int *q = arr;
    printf("[arith]  arr[2] via *(q+2) = %d\n", *(q + 2));
    printf("[arith]  arr[2] via q[2]   = %d\n",  q[2]);

    /* 函数指针分派 */
    binop_t ops[3] = { add_int, sub_int, mul_int };
    const char *names[3] = {"add", "sub", "mul"};
    for (int i = 0; i < 3; i++) {
        printf("[fnptr]  %s(6, 4) = %d\n", names[i], dispatch(ops[i], 6, 4));
    }

    /* NULL 防御 */
    int *np = NULL;
    if (np) printf("np non-null\n");
    else    printf("[null]  np == NULL, skip deref\n");

    return 0;
}
