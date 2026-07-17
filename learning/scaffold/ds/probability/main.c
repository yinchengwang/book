/**
 * @file main.c
 * @brief 概率与随机：线性同余生成器、蒙特卡洛方法、概率 DP
 *
 * 演示内容：
 * 1. 线性同余随机数生成器 (LCG)
 * 2. 蒙特卡洛方法求 PI / 定积分
 * 3. 概率 DP 基础（掷骰子路径计数）
 */

#include <stdio.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>

/* ══════════════════════════════════════════════════════════════════════
 * 线性同余随机数生成器 (Linear Congruential Generator)
 * ══════════════════════════════════════════════════════════════════════ */

/** LCG 状态 */
typedef struct {
    unsigned long state;  /* 当前状态 */
    unsigned long a;      /* 乘数 (multiplier) */
    unsigned long c;      /* 增量 (increment) */
    unsigned long m;      /* 模数 (modulus) */
} LCG;

/**
 * 创建 LCG 生成器
 * @param seed 初始种子
 * @return LCG 结构指针
 */
static LCG *lcg_create(unsigned int seed)
{
    LCG *lcg = (LCG *)malloc(sizeof(LCG));
    lcg->state = seed;
    lcg->a = 1103515245;    /* glibc 使用的乘数 */
    lcg->c = 12345;         /* glibc 使用的增量 */
    lcg->m = 1UL << 31;     /* 2^31 */
    return lcg;
}

/** 释放 LCG */
static void lcg_free(LCG *lcg)
{
    free(lcg);
}

/** 生成 [0,1) 均匀分布随机数 */
static double lcg_uniform(LCG *lcg)
{
    lcg->state = (lcg->a * lcg->state + lcg->c) % lcg->m;
    return (double)lcg->state / lcg->m;
}

/** 生成 [0, max) 整数随机数 */
static unsigned long lcg_rand_range(LCG *lcg, unsigned long max)
{
    return (unsigned long)(lcg_uniform(lcg) * max);
}

/* ══════════════════════════════════════════════════════════════════════
 * 蒙特卡洛方法
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 用蒙特卡洛方法估算 PI
 * 原理：在单位正方形内随机撒点，落在内切圆内的比例 ≈ π/4
 * @param n 采样点数
 * @param rng 随机数生成器
 * @return 估算的 PI 值
 */
static double monte_carlo_pi(long long n, LCG *rng)
{
    long long inside = 0;

    for (long long i = 0; i < n; i++) {
        double x = lcg_uniform(rng);  /* [0,1) */
        double y = lcg_uniform(rng);  /* [0,1) */

        /* 判断是否在四分之一圆内: x^2 + y^2 <= 1 */
        if (x * x + y * y <= 1.0) {
            inside++;
        }
    }

    return 4.0 * (double)inside / (double)n;
}

/**
 * 用蒙特卡洛方法估算定积分 ∫[0,1] x^2 dx = 1/3
 * 原理：在 [0,1]×[0,1] 矩形内随机撒点，在曲线下方的比例 ≈ 积分值
 * @param n 采样点数
 * @param rng 随机数生成器
 * @return 估算的积分值
 */
static double monte_carlo_integral(long long n, LCG *rng)
{
    long long below = 0;

    for (long long i = 0; i < n; i++) {
        double x = lcg_uniform(rng);
        double y = lcg_uniform(rng);

        /* 判断是否在曲线 y=x^2 下方: y <= x^2 */
        if (y <= x * x) {
            below++;
        }
    }

    return (double)below / (double)n;
}

/* ══════════════════════════════════════════════════════════════════════
 * 概率 DP
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 掷骰子问题：求到达位置 n 的概率
 * 每次掷骰子前进 1-6 步，从 0 出发
 * @param n 目标位置
 * @return 到达 n 的路径数
 */
static long long probability_dp_paths(int n)
{
    if (n < 0) return 0;
    if (n == 0) return 1;  /* 起点 */

    long long *dp = (long long *)calloc((size_t)n + 1, sizeof(long long));
    dp[0] = 1;

    for (int i = 1; i <= n; i++) {
        dp[i] = 0;
        for (int dice = 1; dice <= 6; dice++) {
            if (i - dice >= 0) {
                dp[i] += dp[i - dice];
            }
        }
    }

    long long result = dp[n];
    free(dp);
    return result;
}

/**
 * 掷骰子到达位置 n 的概率
 * @param n 目标位置
 * @return 概率值（所有路径数 / 总路径数 6^n）
 */
static double probability_dp_probability(int n)
{
    if (n < 0) return 0.0;
    if (n == 0) return 1.0;

    double *dp = (double *)calloc((size_t)n + 1, sizeof(double));
    dp[0] = 1.0;

    for (int i = 1; i <= n; i++) {
        dp[i] = 0.0;
        for (int dice = 1; dice <= 6; dice++) {
            if (i - dice >= 0) {
                dp[i] += dp[i - dice] / 6.0;  /* 每种点数概率 1/6 */
            }
        }
    }

    double result = dp[n];
    free(dp);
    return result;
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示函数
 * ══════════════════════════════════════════════════════════════════════ */

static void demo_lcg(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: 线性同余随机数生成器 (LCG)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    LCG *rng = lcg_create(42);  /* 使用固定种子保证可复现 */

    printf("\n  生成 10 个 [0,1) 均匀分布随机数:\n");
    printf("    ");
    for (int i = 0; i < 10; i++) {
        printf("%.4f ", lcg_uniform(rng));
    }
    printf("\n");

    printf("\n  生成 10 个 [0,100) 整数随机数:\n");
    printf("    ");
    for (int i = 0; i < 10; i++) {
        printf("%lu ", lcg_rand_range(rng, 100));
    }
    printf("\n");

    /* 验证均匀性：统计 10000 个随机数的分布 */
    printf("\n  均匀性验证 (10000 样本，分 10 区间):\n");
    int buckets[10] = {0};
    for (int i = 0; i < 10000; i++) {
        int idx = (int)(lcg_uniform(rng) * 10);
        if (idx >= 0 && idx < 10) buckets[idx]++;
    }
    printf("    区间:   ");
    for (int i = 0; i < 10; i++) printf(" [%d,%d)", i, i+1);
    printf("\n");
    printf("    计数:   ");
    for (int i = 0; i < 10; i++) printf(" %4d ", buckets[i]);
    printf("\n");

    lcg_free(rng);
}

static void demo_monte_carlo(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: 蒙特卡洛方法\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    LCG *rng = lcg_create(12345);

    /* PI 估算 */
    long long samples_pi[] = {100, 1000, 10000, 100000, 1000000};
    printf("\n  蒙特卡洛估算 PI (真实值: %.10f):\n", M_PI);
    printf("    样本数        估算值           误差\n");
    for (size_t i = 0; i < sizeof(samples_pi) / sizeof(samples_pi[0]); i++) {
        double est = monte_carlo_pi(samples_pi[i], rng);
        double err = fabs(est - M_PI);
        printf("    %8lld    %.10f    %.6f\n", samples_pi[i], est, err);
    }

    /* 积分估算 */
    long long samples_int[] = {100, 1000, 10000, 100000};
    printf("\n  蒙特卡洛估算 ∫[0,1] x^2 dx (真实值: %.10f):\n", 1.0/3.0);
    printf("    样本数        估算值           误差\n");
    for (size_t i = 0; i < sizeof(samples_int) / sizeof(samples_int[0]); i++) {
        double est = monte_carlo_integral(samples_int[i], rng);
        double err = fabs(est - 1.0/3.0);
        printf("    %8lld    %.10f    %.6f\n", samples_int[i], est, err);
    }

    lcg_free(rng);
}

static void demo_probability_dp(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 3: 概率 DP (掷骰子路径)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("\n  掷骰子到达位置 n 的路径数 (DP):\n");
    printf("    位置 n:    ");
    for (int n = 0; n <= 10; n++) {
        printf("%5d ", n);
    }
    printf("\n");

    printf("    路径数:   ");
    for (int n = 0; n <= 10; n++) {
        printf("%5lld ", probability_dp_paths(n));
    }
    printf("\n");

    printf("\n  到达位置 n 的概率 (DP, 每步等概率 1/6):\n");
    printf("    位置 n:    ");
    for (int n = 0; n <= 10; n++) {
        printf("%5d ", n);
    }
    printf("\n");

    printf("    概率:     ");
    for (int n = 0; n <= 10; n++) {
        double p = probability_dp_probability(n);
        printf("%5.4f ", p);
    }
    printf("\n");

    /* 验证概率和为 1 */
    double sum = 0.0;
    for (int n = 0; n <= 20; n++) {
        sum += probability_dp_probability(n);
    }
    printf("\n  验证: 前 21 项概率和 = %.6f (应接近 1)\n", sum);
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║            概率与随机: LCG / 蒙特卡洛 / 概率 DP                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_lcg();
    demo_monte_carlo();
    demo_probability_dp();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  核心要点:\n");
    printf("    LCG: X_{n+1} = (a*X_n + c) mod m\n");
    printf("    蒙特卡洛: 随机采样 → 统计计数 → 大数定律\n");
    printf("    概率 DP: dp[i] = Σ dp[i-dice]/6 (等概率转移)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
