/**
 * @file main.c
 * @brief 计数原理：排列组合、容斥原理、卡特兰数
 *
 * 演示内容：
 * 1. 阶乘与排列组合计算
 * 2. 容斥原理求集合交并集计数
 * 3. 卡特兰数（括号匹配/二叉树计数）
 */

#include <stdio.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════════════════════════════
 * 阶乘与排列组合
 * ══════════════════════════════════════════════════════════════════════ */

/** 计算 n! (递归版) */
static long long factorial(int n)
{
    if (n <= 1) return 1;
    return (long long)n * factorial(n - 1);
}

/** 计算 n! (迭代版，避免栈溢出) */
static long long factorial_iter(int n)
{
    long long result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}

/** 计算排列数 P(n,r) = n! / (n-r)! */
static long long permutation(int n, int r)
{
    if (r > n) return 0;
    long long result = 1;
    for (int i = 0; i < r; i++) {
        result *= (n - i);
    }
    return result;
}

/** 计算组合数 C(n,r) = n! / (r! * (n-r)!) */
static long long combination(int n, int r)
{
    if (r > n) return 0;
    if (r == 0 || r == n) return 1;

    /* 优化：取 min(r, n-r) 减少乘法次数 */
    r = (r < n - r) ? r : (n - r);

    long long result = 1;
    for (int i = 0; i < r; i++) {
        result = result * (n - i) / (i + 1);  /* 整除保证准确 */
    }
    return result;
}

/* ══════════════════════════════════════════════════════════════════════
 * 容斥原理
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 使用容斥原理计算 1..n 中能被 a 或 b 整除的数的个数
 * |A ∪ B| = |A| + |B| - |A ∩ B|
 */
static long long count_divisible(int n, int a, int b)
{
    long long count_a = n / a;       /* 能被 a 整除的数 */
    long long count_b = n / b;       /* 能被 b 整除的数 */
    long long count_ab = n / (a * b); /* 能同时被 a,b 整除的数（交集） */

    return count_a + count_b - count_ab;
}

/**
 * 三集合容斥原理
 * |A ∪ B ∪ C| = |A| + |B| + |C|
 *                - |A∩B| - |A∩C| - |B∩C|
 *                + |A∩B∩C|
 */
static long long inclusion_exclusion_3(int n, int a, int b, int c)
{
    long long na = n / a, nb = n / b, nc = n / c;
    long long nab = n / ((long long)a * b);
    long long nac = n / ((long long)a * c);
    long long nbc = n / ((long long)b * c);
    long long nabc = n / ((long long)a * b * c);

    return na + nb + nc - nab - nac - nbc + nabc;
}

/* ══════════════════════════════════════════════════════════════════════
 * 卡特兰数
 * ══════════════════════════════════════════════════════════════════════ */

/** 卡特兰数 C_n = C(2n,n) / (n+1)，使用递推公式 C_{n+1} = C_n * 2(2n+1)/(n+2) */
static long long catalan(int n)
{
    if (n <= 1) return 1;

    long long c = 1;  /* C_0 = C_1 = 1 */
    for (int i = 1; i < n; i++) {
        /* C_{i+1} = C_i * 2(2i+1)/(i+2) */
        c = c * 2 * (2 * i + 1) / (i + 2);
    }
    return c;
}

/** 卡特兰数（组合数公式）C_n = C(2n,n) - C(2n,n+1) */
static long long catalan_comb(int n)
{
    return combination(2 * n, n) - combination(2 * n, n + 1);
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示函数
 * ══════════════════════════════════════════════════════════════════════ */

static void demo_factorial_combination(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: 阶乘、排列、组合计算\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("\n  阶乘计算 (递归):\n");
    for (int n = 0; n <= 10; n++) {
        printf("    %2d! = %lld\n", n, factorial(n));
    }

    printf("\n  排列数 P(n,r):\n");
    printf("    P(5,2) = %lld  (从5个中选2个排列)\n", permutation(5, 2));
    printf("    P(5,3) = %lld  (从5个中选3个排列)\n", permutation(5, 3));
    printf("    P(10,4) = %lld\n", permutation(10, 4));

    printf("\n  组合数 C(n,r):\n");
    printf("    C(5,2) = %lld  (从5个中选2个组合)\n", combination(5, 2));
    printf("    C(10,3) = %lld\n", combination(10, 3));
    printf("    C(20,10) = %lld\n", combination(20, 10));

    /* 杨辉三角验证组合数 */
    printf("\n  杨辉三角验证 (组合数对称性 C(n,k) = C(n,n-k)):\n");
    printf("    C(6,2) = %lld,  C(6,4) = %lld\n", combination(6, 2), combination(6, 4));
    printf("    C(8,3) = %lld,  C(8,5) = %lld\n", combination(8, 3), combination(8, 5));
}

static void demo_inclusion_exclusion(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: 容斥原理\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* 1..100 中能被 3 或 5 整除的数 */
    printf("\n  两集合: 1..100 中能被 3 或 5 整除的数\n");
    printf("    能被 3 整除: %lld 个\n", 100L / 3);
    printf("    能被 5 整除: %lld 个\n", 100L / 5);
    printf("    能同时被 3,5 整除: %lld 个\n", 100L / 15);
    printf("    合计 (容斥): %lld 个\n", count_divisible(100, 3, 5));
    printf("    验证: 3,5,6,9,10,12,15,...\n");

    /* 1..1000 中能被 2,3,5 之一整除的数 */
    printf("\n  三集合: 1..1000 中能被 2,3,5 之一整除的数\n");
    printf("    能被 2 整除: %lld 个\n", 1000L / 2);
    printf("    能被 3 整除: %lld 个\n", 1000L / 3);
    printf("    能被 5 整除: %lld 个\n", 1000L / 5);
    printf("    能被 2,3 同时整除: %lld 个\n", 1000L / 6);
    printf("    能被 2,5 同时整除: %lld 个\n", 1000L / 10);
    printf("    能被 3,5 同时整除: %lld 个\n", 1000L / 15);
    printf("    能被 2,3,5 同时整除: %lld 个\n", 1000L / 30);
    printf("    合计 (三集合容斥): %lld 个\n", inclusion_exclusion_3(1000, 2, 3, 5));
}

static void demo_catalan(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 3: 卡特兰数\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("\n  卡特兰数序列 (递推公式):\n");
    printf("    ");
    for (int n = 0; n <= 10; n++) {
        printf("%lld ", catalan(n));
    }
    printf("\n");

    printf("\n  组合数公式验证 (C(2n,n) - C(2n,n+1)):\n");
    for (int n = 0; n <= 8; n++) {
        printf("    C(%d) = %lld\n", n, catalan_comb(n));
    }

    printf("\n  卡特兰数的应用场景:\n");
    printf("    n=2: 2个节点二叉树种类 = %lld\n", catalan(2));
    printf("    n=3: 3对括号正确匹配 = %lld\n", catalan(3));
    printf("    n=4: 4步单调路径数 = %lld\n", catalan(4));
    printf("    n=10: 10边形三角剖分 = %lld\n", catalan(10));
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║              计数原理: 排列组合 / 容斥 / 卡特兰数               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_factorial_combination();
    demo_inclusion_exclusion();
    demo_catalan();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  核心公式速记:\n");
    printf("    排列: P(n,r) = n!/(n-r)!          (有序选择)\n");
    printf("    组合: C(n,r) = n!/(r!(n-r)!)      (无序选择)\n");
    printf("    容斥: |A∪B| = |A|+|B|-|A∩B|       (去重复)\n");
    printf("    卡特兰: C_n = C(2n,n)/(n+1)       (合法路径计数)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
