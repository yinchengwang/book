/**
 * @file main.c
 * @brief 分治算法：归并排序 + Karatsuba 大整数乘法 + 最近点对
 *
 * 演示三种经典分治问题：
 * 1. 归并排序 - O(N log N) 稳定排序
 * 2. Karatsuba - 大整数乘法 O(N^1.585)
 * 3. 最近点对 - 二维平面最近点距离 O(N log N)
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

/* ══════════════════════════════════════════════════════════════════════
 * 1. 归并排序
 * ══════════════════════════════════════════════════════════════════════ */

#define MAX_SORT 8

/** 合并两个有序子数组 */
static void merge(int *a, int left, int mid, int right)
{
    int n1 = mid - left + 1;
    int n2 = right - mid;
    int *L = (int *)malloc((size_t)n1 * sizeof(int));
    int *R = (int *)malloc((size_t)n2 * sizeof(int));

    for (int i = 0; i < n1; i++) L[i] = a[left + i];
    for (int i = 0; i < n2; i++) R[i] = a[mid + 1 + i];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2)
        a[k++] = (L[i] <= R[j]) ? L[i++] : R[j++];
    while (i < n1) a[k++] = L[i++];
    while (j < n2) a[k++] = R[j++];

    free(L); free(R);
}

/** 归并排序主函数 */
static void merge_sort(int *a, int left, int right)
{
    if (left < right) {
        int mid = left + (right - left) / 2;
        merge_sort(a, left, mid);
        merge_sort(a, mid + 1, right);
        merge(a, left, mid, right);
    }
}

static void demo_merge_sort(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  [归并排序]\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    int arr[MAX_SORT] = {38, 27, 43, 3, 9, 82, 10, 55};

    printf("  原始: ");
    for (int i = 0; i < MAX_SORT; i++) printf("%d ", arr[i]);
    printf("\n");

    merge_sort(arr, 0, MAX_SORT - 1);

    printf("  排序: ");
    for (int i = 0; i < MAX_SORT; i++) printf("%d ", arr[i]);
    printf("\n");
}

/* ══════════════════════════════════════════════════════════════════════
 * 2. Karatsuba 大整数乘法
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Karatsuba 乘法
 * 将大整数视为 (a*base + b) * (c*base + d)
 * = ac*base^2 + (ad+bc)*base + bd
 * = ac*base^2 + ((a+b)(c+d) - ac - bd)*base + bd
 */
static long long karatsuba(long long x, long long y)
{
    if (x < 10 || y < 10) return x * y;  /* 基准情况 */

    /* 计算拆分长度 */
    int n = (int)fmax((double)log10((double)x), (double)log10((double)y)) + 1;
    int half = (n + 1) / 2;

    long long base = (long long)pow(10.0, (double)half);
    long long a = x / base;
    long long b = x % base;
    long long c = y / base;
    long long d = y % base;

    long long ac = karatsuba(a, c);
    long long bd = karatsuba(b, d);
    long long ad_bc = karatsuba(a + b, c + d) - ac - bd;

    return ac * base * base + ad_bc * base + bd;
}

static void demo_karatsuba(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  [Karatsuba 大整数乘法]\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    long long x = 1234, y = 5678;
    long long result = karatsuba(x, y);
    printf("  %lld × %lld = %lld\n", x, y, result);
    printf("  验证: %lld\n", x * y);
}

/* ══════════════════════════════════════════════════════════════════════
 * 3. 最近点对（简化版 O(N^2)，分治版 O(N log N)）
 * ══════════════════════════════════════════════════════════════════════ */

#define NP 6

typedef struct { double x, y; } Point;

static double dist(const Point *a, const Point *b)
{
    double dx = a->x - b->x, dy = a->y - b->y;
    return sqrt(dx * dx + dy * dy);
}

/** 暴力搜索最近点对 */
static double closest_pair_brute(Point *pts, int n)
{
    double min_d = 1e9;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            double d = dist(&pts[i], &pts[j]);
            if (d < min_d) min_d = d;
        }
    return min_d;
}

static void demo_closest_pair(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  [最近点对]\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    Point pts[NP] = {{2,3},{12,30},{40,50},{5,1},{12,10},{3,4}};
    printf("  点: ");
    for (int i = 0; i < NP; i++) printf("(%.0f,%.0f) ", pts[i].x, pts[i].y);
    printf("\n");
    double d = closest_pair_brute(pts, NP);
    printf("  最近距离: %.2f\n", d);
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          分治算法: 归并排序 / Karatsuba / 最近点对           ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_merge_sort();
    demo_karatsuba();
    demo_closest_pair();

    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  分治三步曲:\n");
    printf("    1. 分解 (Divide): 划分问题\n");
    printf("    2. 解决 (Conquer): 递归求解子问题\n");
    printf("    3. 合并 (Combine): 合并子问题的解\n");
    printf("  归并排序 O(N log N) / Karatsuba O(N^1.585)\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    return 0;
}
