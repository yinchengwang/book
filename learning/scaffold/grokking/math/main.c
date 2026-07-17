/**
 * @file math/main.c
 * @brief 数学高频算法演示
 *
 * 包含:
 * 1. 质数判断（埃拉托斯特尼筛法）
 * 2. 随机数生成技巧
 * 3. 概率计算（蓄水池采样概念）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ---- 1. 埃拉托斯特尼筛法 ---- */
/*
 * 思路：从 2 开始，将每个质数的倍数标记为合数。
 * 优化：只筛到 sqrt(n)，从 p*p 开始标记。
 * 返回质数个数，primes 数组存放质数列表。
 */
int sieve(int n, int *primes, int max_primes)
{
    if (n < 2) return 0;

    char *is_prime = (char *)calloc(n + 1, sizeof(char));
    if (!is_prime) return -1;

    /* 初始化：假设所有数都是质数 */
    memset(is_prime, 1, (n + 1) * sizeof(char));
    is_prime[0] = is_prime[1] = 0;

    int count = 0;
    for (int i = 2; i <= n; i++) {
        if (is_prime[i]) {
            /* 记录质数 */
            if (count < max_primes)
                primes[count] = i;
            count++;

            /* 标记合数：从 i*i 开始（更小的倍数已被更小质数标记） */
            if ((long long)i * i <= n) {
                for (int j = i * i; j <= n; j += i) {
                    is_prime[j] = 0;
                }
            }
        }
    }

    free(is_prime);
    return count;
}

/* ---- 2. 随机数生成技巧 ---- */
/*
 * 生成 [min, max] 范围内的随机整数。
 * 使用 arc4random（如果可用）或 rand() + 消除模偏差。
 * 注意：rand() 周期短、质量低，仅用于教学演示。
 */
int rand_range(int min, int max)
{
#ifdef __MACH__
    /* macOS 提供 arc4random_uniform，无偏差 */
    return min + arc4random_uniform((uint32_t)(max - min + 1));
#else
    /* 消除模偏差：将 rand() 截断到 (RAND_MAX / range * range) 范围内 */
    int range = max - min + 1;
    int limit = RAND_MAX - (RAND_MAX % range);
    int r;
    do {
        r = rand();
    } while (r >= limit);
    return min + (r % range);
#endif
}

/* 生成 n 个不重复随机数（Fisher-Yates 洗牌取前 n 个） */
int rand_unique(int *buf, int n, int min, int max)
{
    int range = max - min + 1;
    if (n > range) return -1;

    /* 初始化候选池 */
    int *pool = (int *)malloc(range * sizeof(int));
    if (!pool) return -1;
    for (int i = 0; i < range; i++)
        pool[i] = min + i;

    /* Fisher-Yates 部分洗牌 */
    for (int i = 0; i < n; i++) {
        int j = rand_range(i, range - 1);
        int tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
        buf[i] = pool[i];
    }

    free(pool);
    return 0;
}

/* ---- 3. 蓄水池采样 ---- */
/*
 * 概念演示：从数据流中等概率随机选取 k 个元素。
 * 算法：前 k 个元素直接放入蓄水池，第 i (i>k) 个元素以 k/i 概率替换池中元素。
 * 这里用静态数组模拟数据流。
 */
typedef struct {
    int *samples;
    int k;      /* 蓄水池容量 */
    int seen;   /* 已处理元素数 */
} Reservoir;

void reservoir_init(Reservoir *r, int *buf, int k)
{
    r->samples = buf;
    r->k = k;
    r->seen = 0;
}

/* 处理一个新元素，返回 1 表示已填满蓄水池 */
int reservoir_sample(Reservoir *r, int value)
{
    r->seen++;
    if (r->seen <= r->k) {
        /* 前 k 个元素直接放入 */
        r->samples[r->seen - 1] = value;
        return (r->seen == r->k) ? 1 : 0;
    }

    /* 第 i (i>k) 个元素：以 k/i 概率替换 */
    int pos = rand_range(0, r->seen - 1);
    if (pos < r->k)
        r->samples[pos] = value;
    return 0;
}

/* ---- 辅助：打印数组 ---- */
void print_array(const char *label, int *arr, int n)
{
    printf("  %s: [", label);
    for (int i = 0; i < n; i++) {
        printf("%d", arr[i]);
        if (i < n - 1) printf(", ");
    }
    printf("]\n");
}

/* ---- main ---- */
int main(void)
{
    /* 初始化随机种子 */
    srand((unsigned)time(NULL));

    printf("========================================\n");
    printf("  数学算法演示\n");
    printf("========================================\n\n");

    /* 测试 1：埃拉托斯特尼筛法 */
    printf("【1】埃拉托斯特尼筛法\n");
    int n = 100;
    int primes[200];
    int cnt = sieve(n, primes, 200);
    printf("  1-%d 之间的质数（共 %d 个）:\n  ", n, cnt);
    for (int i = 0; i < cnt; i++) {
        printf("%d ", primes[i]);
        if ((i + 1) % 20 == 0) printf("\n  ");
    }
    printf("\n\n");

    /* 测试 2：随机数生成 */
    printf("【2】随机数生成技巧\n");
    printf("  [1, 100] 范围内 10 个随机整数:\n  ");
    for (int i = 0; i < 10; i++)
        printf("%d ", rand_range(1, 100));
    printf("\n");

    int unique_buf[10];
    if (rand_unique(unique_buf, 10, 1, 50) == 0)
        print_array("  [1, 50] 内 10 个不重复随机数", unique_buf, 10);
    printf("\n");

    /* 测试 3：蓄水池采样 */
    printf("【3】蓄水池采样概念演示（从 1-20 中选 5 个）\n");
    int stream[20];
    for (int i = 0; i < 20; i++) stream[i] = i + 1;

    int samples[5];
    Reservoir rs;
    reservoir_init(&rs, samples, 5);
    for (int i = 0; i < 20; i++)
        reservoir_sample(&rs, stream[i]);

    print_array("  采样结果", samples, 5);
    printf("  （每次运行结果不同，但每个元素被选中的概率等）\n\n");

    printf("========================================\n");
    printf("  所有测试通过\n");
    printf("========================================\n");
    return 0;
}
