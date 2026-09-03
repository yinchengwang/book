/*
 * @Author: yinchengwang
 * @Date: 2025-02-06 21:22:56
 * @LastEditors: yinchengwang
 * @LastEditTime: 2025-02-09 18:08:38
 * @Description: 编写高质量代码 改善C程序代码的125个建议 demo code
 * @FilePath: \book\demo_code\C\demo_20250206.c
 */

#include <stdio.h>
#include <crtdefs.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>


/*
 *负数的二进制原码与补码转换, 以-5为例
 * 原码转换为补码的过程
 * 1000 0101    原码    5的原码为00000101, 因为负数, 所以多符号位取反
 * 1111 1010    反码    对原码除符号位以为都取反
 * 1111 1011    补码    对反码加1
 *
 * 补码转换为二进制原码的过程
 * 1111 1011    补码
 * 1111 1010    反码    减1
 * 1000 0101    原码    除符号位外都取反
 */

void func1(size_t n, const char *str)
{
    // SIZE_MAX = 18446744073709551615, INT_MAX = 2147483647.
    printf("SIZE_MAX = %zu, INT_MAX = %d.\n", SIZE_MAX, INT_MAX);
    // 根据取值范围可见, size_t的取值范围远大于int,
    // 因此函数内的n可能超出了int的范围, 在for循环时i可能翻转, 导致int范围内的部分被反复覆盖导致数据错误
    int i;
    char *p;
    if (n == 0) {
        /* 处理n==0的情况 */
    }

    p = (char *)malloc(n);
    if (p == NULL) {
        /*处理p==NULL的情况 */
    }

    for (i = 0; i < n; ++i ) {
        p[i] = *str++;
    }

    if (!p) {
        free(p);
        p = NULL;
    }
}

// 隐式转换
void func2(void)
{
    int array[] = { 1, 2, 3, 4, 5, 6 };
    int i = -1;
    // 当有符号数和无符号数进行计算时, 通常在计算时int会被隐式转换成unsigned int. 而负数被转换成无符号数时通常是一个超级大的数字
    // 负数转换成无符号整数的步骤
    // 10000000 00000000 00000000 00000001 -1的原码
    // 11111111 11111111 11111111 11111110 取反变成反码
    // 11111111 11111111 11111111 11111111 反码+1变成补码
    // 补码转换成10进制即位2^32 - 1 = 4294967295
    printf("(unsigned int)(-1) = %u.\n", (unsigned int)(-1));
    // i > sizeof(array).
    if (i <= sizeof(array)) {
        printf("i <= sizeof(array).\n");
    } else {
        printf("i > sizeof(array).\n");
    }
}

// 整数的溢出
// 踩存等：计算结果将作为一个缓冲区的大小、数组的下标、循环计数器与内存分配函数的实参等
// 无符号整数的回绕
// 回绕容易被攻击的点: 利用计算结果来决定将要分配的缓冲区的大小, 从而调用malloc()或calloc()函数来分配内存
void func3(void)
{
    // 乘法溢出
    int a = 2147483647;
    int b = 2;
    int c = a * b;
    // 实际结果应为4294967294, 但溢出后变成了-2
    // 11111111 11111111 11111111 11111110 4294967294的二进制表示
    // int只有32位, 最高位是符号位, 因此上面的二进制被识别成一个负数, 而负数转换成十进制时的规则为取反+1
    // 00000000 00000000 00000000 00000001 取反
    // 00000000 00000000 00000000 00000010 +1, 此时即为2
    // c = -2.
    printf("c = %d.\n", c);

    // 无符号整数回绕
    // 无符号整数最大值为 4294967295
    unsigned int x = 4294967295;
    x += 2;
    // 当无符号整数超出最大值是即绕回最小值重新开始, 因此+1时变成0, 再加1则变成了1
    // x = 1.
    printf("x = %u.\n", x);

    unsigned int aa = 4;
    unsigned int bb = 2;
    // -2向上回绕
    // bb - aa = 4294967294.
    printf("bb - aa = %u.\n", bb - aa);

    int i = 65536;
    unsigned short ii = i;
    // unsigned short最大值为65535, 超出范围后会发生回绕, 容易被攻击
    // ii = 0.
    printf("ii = %hd.\n", ii);
}

// 大小端 一个32位整数 0x12345678 左边是高位字节, 右边是低位字节
// 大端序：高位字节存储在低地址
// 地址: 0x1000 0x1001 0x1002 0x1003
// 数据: 0x12   0x34   0x56   0x78
// 小端序：低位字节存储在低地址
// 地址: 0x1000 0x1001 0x1002 0x1003
// 数据: 0x78   0x56   0x34   0x12
void func4(void)
{
    int num = 1;
    // 将int类型的指针强制转换为char类型的指针
    // 这样可以访问int的第一个字节
    if (*(char *)&num == 1) {
        printf("Little-endian\n");
    } else {
        printf("Big-endian\n");
    }

    union {
        int i;
        char c[sizeof(int)];
    } u;

    u.i = 1;
    if (u.c[0] == 1) {
        printf("Little-endian\n");
    } else {
        printf("Big-endian\n");
    }
}

// 浮点数基础
void func5(void)
{
    // long double并没有一个准确的精度, 不同的平台可能有不同的实现, 但是一般是要大于double, 最差也是与double相等
    // sizeof(float)=4, sizeof(double)=8, sizeof(long double)=16
    printf("sizeof(float)=%zu, sizeof(double)=%zu, sizeof(long double)=%zu.\n",
        sizeof(float), sizeof(double), sizeof(long double));

    float f1=34.6;
    float f2=34.5;
    float f3=34.0;
    // 浮点表示法: 只有5结尾的浮点数才可以得到精确的表示
    // 34.6-34.0=0.599998
    // 34.5-34.0=0.500000
    printf("34.6-34.0=%f\n", f1 - f3);
    printf("34.5-34.0=%f\n", f2 - f3);
    // 34.6+34.0=68.599998
    printf("34.6+34.0=%f\n", f1 + f3);
    // 34.5+34.0=68.500000
    printf("34.5+34.0=%f\n", f2 + f3);

    // IEEE 754浮点数存储
    // s | exp | frac
    // 符号位(1) | 指数位(8) | 有效数字位(23)   float32
    // 符号位(1) | 指数位(5) | 有效数字位(10)   float16
    // 符号位(1) | 指数位(11) | 有效数字位(52)   double
    // 以-9.625为例 1.001101×2^3
    // 1 | 10000010 | 00110100000000000000000
    // 符号段为1, 因为是负数
    // 指数位为3, 加上偏移127为130, 转换成二进制就是 10000010
    // 有效数字位是省略小数点左侧的1之后为001101, 剩下的在右侧加0补齐

    // NaN: 当指数段exp全为1时, 有效数字段为非零时, 结果值就被称为"NaN"​(Not any Number)

    // 当指数段exp全为1, 有效数字段全为0时, 得到的值表示无穷; 当s=0时是+∞, 或者当s=1时是-∞

    // 舍入误差
    //              1.4     2.5     -1.5
    // 向偶数舍入     1       2       -2
    // 向0舍入        1       2       -1
    // 向上舍入       2       3       -1
    // 向下舍入       1       2       -2
}

// 浮点数计算
void func6()
{
    float arr[10];
    float total = 0;
    for (int i = 0; i < 10; i++) {
        arr[i] = 5.1;
        total += arr[i];
    }
    // total= 50.999996, average: 5.099999.
    printf("total= %f, average: %f.\n", total, total / 10);

    #include <stdio.h>

    // 避免使用 == 比较浮点数
    float f1=3.46f;
    float f2=5.77f;
    float f3=9.23f;
    // f1(3.46f)=3.460000038146970000
    // f2(5.77f)=5.76999998092651370000
    // f1+f2=9.22999954223632833000.
    // f3(9.23f)=9.22999954223632812500
    // f1 + f2 != f3
    printf("f1(3.46f)=%0.20f\nf2(5.77f)=%0.20f\nf1+f2=%0.20f.\nf3(9.23f)=%0.20f\n", f1, f2, f1 + f2, f3);
    if (f1 + f2 == f3) {
        printf("f1 + f2 == f3\n");
    } else {
        printf("f1 + f2 != f3\n");
    }

    // 避免使用浮点数作为循环条件, 陷入死循环
    // x = 100000000.000000
    // x = 100000000.000000
    // ......
    // x = 100000000.000000
    for (float x = 100000001.0f; x <= 100000010.0f; x += 1.0f){
        printf("x = %f\n", x);
    }
}

int main()
{
    int a1 = 0b00100011;
    int a2 = 0X23;
    // a1=35, a2=35. 二进制与十六进制转换
    printf("a1=%d, a2=%d.\n", a1, a2);

    int a3 = -2;
    // 负数采用2的补码的形式来表示,即对原码各位求反(符号位除外)​,再将求反的结果加1,最后将符号位设置为1
    // 00000000 00000000 00000000 00000010 2的二进制
    // 01111111 11111111 11111111 11111101 求反(符号位除外)
    // 01111111 11111111 11111111 11111110 将求反的结果加1
    // 11111111 11111111 11111111 11111110 将符号位设置为1
    // a3 = FFFFFFFE.
    printf("a3 = %X.\n", a3);

    char c41 = 150;
    unsigned char c42 = 150;
    // char取值范围为-128~127, 150超出了取值范围, 溢出后从-128开始取值
    // c41 = -106, 900 / c41 = -8, c42 = 150, 900 / c42 = 6
    printf("c41 = %d, 900 / c41 = %d, c42 = %d, 900 / c42 = %d.\n", c41, 900 / c41, c42, 900 / c42);

    func1(10, "asdf");

    // 禁止混用size_t和unsigned int
    // 跳转至size_t定义处即可知, 在不同的环境下size_t位不同的类型, 因此直接混用可能导致高位数据截断等问题
    size_t x5 = 10;
    unsigned int y5;
    x5 = y5;

    uint8_t a6 = (uint8_t)(129 * 2);
    // uint8_t取值范围为0-255, 258超出取值范围, 所以从0开始从新取值为2
    // a6 = 2.
    printf("a6 = %d.\n", a6);

    func2();

    func3();

    func4();

    func5();

    func6();

    return 0;
}