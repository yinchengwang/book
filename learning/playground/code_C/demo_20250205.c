/*
 * @Author: yinchengwang
 * @Date: 2025-02-05 21:08:56
 * @LastEditors: yinchengwang
 * @LastEditTime: 2025-02-06 21:39:12
 * @Description: C++新经典 demo code
 * @FilePath: \book\demo_code\C\demo_20250205.c
 */

#include <stdio.h>
#include <string.h>
#include <typeinfo.h>
#include <math.h>

int main()
{
    int a1 = 3.12F;
    printf("a1 = %d\n", a1);

    float af = 1111111.111;
    double bf = 1111111.111;
    // 浮点数精度问题，导致实际存的数与赋值时候的数不一致，需要注意!
    // af = 1111111.125000, bf = 1111111.111000
    printf("af = %f, bf = %f\n", af, bf);
    af = 1234567898.12345;
    // af = 1234567936.000000
    printf("af = %f\n", af);

    // char cc = 'ab';  溢出
    // printf("cc = %c\n", cc);

    char c1 = 'A';
    char c2 = 'a';
    // c1 = A(65), c2 = a(97)
    printf("c1 = %c(%d), c2 = %c(%d)\n", c1, c1, c2, c2);
    // c1 + 4 = E(69), c2 + 4 = e(101)
    printf("c1 + 4 = %c(%d), c2 + 4 = %c(%d)\n", c1 + 4, c1 + 4, c2 + 4, c2 + 4);

    char str111[] = "abcdef";
    // strlen的原理实际为遍历，以当前字符是非终止符作为循环终止条件，因此终止符不算在长度内
    // sizeof的则会包括终止符
    // strlen(str111) = 6, sizeof(str111) = 7.
    printf("strlen(str111) = %zu, sizeof(str111) = %zu.\n", strlen(str111), sizeof(str111));

    int q3 = 5;
    int a3, b3, c3 = 3;
    // 未初始化的值为随机值, 使用时存在极大风险, 因此定义的时候最好都有一个明确的初值
    // a3 = 8, b3 = 0, c3 = 3, q3 = 5.
    printf("a3 = %d, b3 = %d, c3 = %d, q3 = %d.\n", a3, b3, c3, q3);

    // 数值的混合运算
    int a4 = 500;
    double d4 = 15.67;
    double res4 = a4 + d4;
    // 不同类型数值变量进行混合运算时，系统会尝试将它们的变量类型统一，然后再做混合运算
    // 并且系统会选取参与运算的变量中能表达最大数字的变量类型作为其他变量转换的目标类型
    // int + double = double  char + int = int
    // res4 = 515.670000
    printf("res4 = %f.\n", res4);

    float a5 = 0.5;
    int b5 = 1;
    float res5 = a5 + b5;
    // res5 = 1.500000 int类型与浮点类型计算时会隐式转换成浮点类型
    printf("res5 = %f.\n", res5);

    float a6 = 0.30000001;
    float b6 = 0.3;
    // 由于精度问题，直接比较浮点数是否相等可能会导致错误。通常需要设置一个容差值（epsilon）来进行比较
    if (a6 == b6) {
        // a6 == b6.
        printf("a6 == b6.\n");
    }

    if (fabs(a6 - b6) < 1e-6) {
        // fabs(a6 - b6) < 1e-6.
        printf("fabs(a6 - b6) < 1e-6.\n");
    }

    // 显示转换
    int a7 = 10;
    float b7 = (float)(a7 / 3);
    float c7 = (float)a7 / 3;
    // 强转不会影响原来的值，计算完再转换已经不能获取到想要的值
    // a7=10, b7=3.000000, c7=3.333333.
    printf("a7=%d, b7=%f, c7=%f.\n", a7, b7, c7);

    return 0;
}