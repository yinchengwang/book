/* func_pointer scaffold — 函数指针 + qsort + atexit */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*cmp_t)(const void *, const void *);
typedef int (*arith_t)(int, int);

static int cmp_int_asc(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

static int cmp_int_desc(const void *a, const void *b) {
    return -cmp_int_asc(a, b);
}

static int add(int x, int y) { return x + y; }
static int mul(int x, int y) { return x * y; }

static void atexit_clean(void) {
    fprintf(stderr, "[atexit] cleanup hook fired\n");
}

int main(void) {
    /* 1) qsort with callback */
    int arr[8] = {5, 2, 8, 1, 9, 3, 7, 4};
    qsort(arr, 8, sizeof(int), cmp_int_asc);
    printf("[qsort]  asc  :");
    for (int i = 0; i < 8; i++) printf(" %d", arr[i]);
    printf("\n");
    qsort(arr, 8, sizeof(int), cmp_int_desc);
    printf("[qsort]  desc :");
    for (int i = 0; i < 8; i++) printf(" %d", arr[i]);
    printf("\n");

    /* 2) 函数指针分派 */
    arith_t ops[2] = {add, mul};
    const char *names[2] = {"add", "mul"};
    for (int i = 0; i < 2; i++) {
        printf("[arith]  %s(3, 4) = %d\n", names[i], ops[i](3, 4));
    }

    /* 3) atexit 注册回调（与 main 返回同步）*/
    atexit(atexit_clean);

    return 0;
}
