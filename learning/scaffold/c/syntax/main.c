/* syntax scaffold — 数据类型与运算符
 *
 * 复现命令：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *
 * 演示 4 段：
 *   [types]   — 12 种类型 size（sizeof 验证）
 *   [ops]     — 运算符优先级（用括号强制演示）
 *   [bounds]  — 整数类型边界值（INT_MAX/UINT_MAX）
 *   [cast]    — 类型转换的安全/不安全形态
 */

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>

int main(void) {
    /* === [types] 类型大小 === */
    printf("[types] === 12 种 C 类型大小 ===\n");

    printf("  char           : %zu byte\n", sizeof(char));
    printf("  short          : %zu byte\n", sizeof(short));
    printf("  int            : %zu byte\n", sizeof(int));
    printf("  long           : %zu byte\n", sizeof(long));
    printf("  long long      : %zu byte\n", sizeof(long long));
    printf("  float          : %zu byte\n", sizeof(float));
    printf("  double         : %zu byte\n", sizeof(double));
    printf("  long double    : %zu byte\n", sizeof(long double));
    printf("  void*          : %zu byte\n", sizeof(void*));
    printf("  int8_t         : %zu byte\n", sizeof(int8_t));
    printf("  int32_t        : %zu byte\n", sizeof(int32_t));
    printf("  int64_t        : %zu byte\n", sizeof(int64_t));

    /* === [ops] 运算符优先级 === */
    printf("\n[ops] === 运算符优先级演示 ===\n");

    /* 乘除高于加减：1 + 2 * 3 == 7 而非 9 */
    int a = 1 + 2 * 3;
    printf("  1 + 2 * 3     = %d (乘优先)\n", a);

    /* 左移高于加减：1 << 2 + 1 == 1 << 3 == 8 */
    int b = 1 << 2 + 1;
    printf("  1 << 2 + 1    = %d (<< 优先于 +)\n", b);

    /* 比较高于逻辑：0 < 1 && 1 < 2 == (0<1) && (1<2) */
    int c = 0 < 1 && 1 < 2;
    printf("  0<1 && 1<2    = %d (比较优先于 &&)\n", c);

    /* 位运算低于比较：flags & MASK == 0 容易出错 */
    unsigned flags = 0x03;
    unsigned MASK = 0x01;
    int d = flags & MASK == 0;   /* 等价于 flags & (MASK == 0) */
    printf("  flags&MASK==0 = %d (== 优先于 &，常见坑)\n", d);
    int e = (flags & MASK) == 0;
    printf("  (flags&MASK)==0 = %d (加括号才是本意)\n", e);

    /* === [bounds] 整数边界 === */
    printf("\n[bounds] === 整数类型边界 ===\n");

    printf("  INT_MAX       = %d\n", INT_MAX);
    printf("  INT_MIN       = %d\n", INT_MIN);
    printf("  UINT_MAX      = %u\n", UINT_MAX);
    printf("  LONG_MAX      = %ld\n", LONG_MAX);
    printf("  LLONG_MAX     = %lld\n", LLONG_MAX);
    printf("  FLT_MAX       = %e\n", (double)FLT_MAX);
    printf("  DBL_MAX       = %e\n", DBL_MAX);

    /* 演示有符号溢出是 UB，无符号溢出环绕 */
    unsigned u = UINT_MAX;
    printf("  UINT_MAX+1    = %u (无符号环绕)\n", u + 1);

    /* === [cast] 类型转换 === */
    printf("\n[cast] === 类型转换 ===\n");

    /* 隐式转换：int → double 精度提升 */
    int i = 42;
    double d1 = i;
    printf("  int %d → double %f (隐式，安全)\n", i, d1);

    /* 隐式截断：double → int 丢失小数 */
    double d2 = 3.7;
    int i2 = d2;
    printf("  double %f → int %d (隐式，截断)\n", d2, i2);

    /* 显式 cast：(int)3.7 仍是截断 */
    int i3 = (int)d2;
    printf("  (int)3.7      = %d (显式 cast，截断)\n", i3);

    /* 指针大小：void* 可装任何指针 */
    int x = 0;
    void *vp = &x;
    int *ip = vp;  /* 隐式转换 OK（void* 通用） */
    printf("  void*→int*    = %p (隐式，安全)\n", (void*)ip);

    printf("\n=== PASS ===\n");
    return 0;
}