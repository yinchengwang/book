/*
 * fenwick_tree.c —— 树状数组实现
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 *   struct ds_fenwick {
 *       int64_t *tree;   // 树状数组（1-indexed）
 *       size_t   n;      // 原始数组大小
 *   };
 *
 * ============================================================
 * lowbit 操作
 * ============================================================
 *
 *   lowbit(x) = x & (-x)
 *
 * 补码表示：
 *   x  =  6 →  0...00110
 *  ~x  =    →  1...11001
 *  -x  = ~x+1 → 1...11010
 *   x & -x =  0...00010 = 2  ← 提取最低位的 1
 *
 * lowbit 决定了每个 tree[i] 负责的区间长度：
 *   tree[i] 负责 sum(arr[i - lowbit(i) + 1 .. i])
 *
 * ============================================================
 * 更新操作：arr[idx] += delta
 * ============================================================
 *
 * 算法：从 idx 开始，不断向上跳，更新所有包含 idx 的区间。
 *
 *   while (idx <= n):
 *       tree[idx] += delta
 *       idx += lowbit(idx)   // 跳到下一个包含 idx 的父节点
 *
 * 示例：更新 arr[5]（n=8）
 *   idx=5: tree[5] += d, 5 + lowbit(5)=5+1=6
 *   idx=6: tree[6] += d, 6 + lowbit(6)=6+2=8
 *   idx=8: tree[8] += d, 8 + lowbit(8)=8+8=16 > n → 停止
 *
 *   更新了 tree[5], tree[6], tree[8]  ← 这些节点的区间都包含 arr[5]
 *
 * ============================================================
 * 查询操作：prefix_sum(idx) = sum(arr[1..idx])
 * ============================================================
 *
 * 算法：从 idx 开始，不断向下跳，累加所有不重叠的前缀区间。
 *
 *   while (idx > 0):
 *       result += tree[idx]
 *       idx -= lowbit(idx)   // 跳到下一个前缀区间的末尾
 *
 * 示例：prefix_sum(7)（n=8）
 *   idx=7: result += tree[7] (arr[7]), 7 - lowbit(7)=7-1=6
 *   idx=6: result += tree[6] (arr[5]+arr[6]), 6 - lowbit(6)=6-2=4
 *   idx=4: result += tree[4] (arr[1]+arr[2]+arr[3]+arr[4]), 4 - 4=0 → 停止
 *
 *   tree[7] 负责 [7,7], tree[6] 负责 [5,6], tree[4] 负责 [1,4]
 *   = arr[1..7] 全部覆盖！
 *
 * ============================================================
 * 对比线段树
 * ============================================================
 * | 维度     | 树状数组    | 线段树       |
 * |---------|------------|-------------|
 * | 代码行数  | ~10 行      | ~50 行       |
 * | 空间     | n+1        | 4n           |
 * | 单点更新  | O(log n)   | O(log n)     |
 * | 区间更新  | 需差分      | lazy O(log n)|
 * | 区间查询  | O(log n)   | O(log n)     |
 * | 功能      | 前缀为主    | 任意可合并操作 |
 */

#include <ds/fenwick_tree.h>

#include <stdio.h>
#include <stdlib.h>

struct ds_fenwick {
    int64_t *tree;
    size_t   n;
};

/* lowbit 操作 */
static inline size_t lowbit(size_t x)
{
    return x & (~x + 1u); /* x & (-x)，用无符号实现以免 undefined behavior */
}

ds_fenwick_t *ds_fenwick_create(const int64_t *arr, size_t n)
{
    ds_fenwick_t *bit;

    if (!arr || n == 0u) {
        return NULL;
    }

    bit = (ds_fenwick_t *)calloc(1u, sizeof(ds_fenwick_t));
    if (!bit) {
        return NULL;
    }

    /* 下标从 1 开始，所以需要 n+1 个元素 */
    bit->tree = (int64_t *)calloc(n + 1u, sizeof(int64_t));
    if (!bit->tree) {
        free(bit);
        return NULL;
    }

    bit->n = n;

    /*
     * O(n log n) 建树方法：对每个位置调用一次 update。
     *
     * 更优的 O(n) 建树方法：
     *   1. 计算前缀和
     *   2. tree[i] = prefix[i] - prefix[i - lowbit(i)]
     * 这里使用 O(n log n) 方法，代码更简洁。
     */
    for (size_t i = 0u; i < n; ++i) {
        ds_fenwick_update(bit, i + 1u, arr[i]);
    }

    return bit;
}

void ds_fenwick_destroy(ds_fenwick_t *bit)
{
    if (!bit) return;
    free(bit->tree);
    free(bit);
}

int ds_fenwick_update(ds_fenwick_t *bit, size_t idx, int64_t delta)
{
    if (!bit || idx == 0u || idx > bit->n) {
        return -1;
    }

    /* 向上遍历：更新所有包含 idx 的区间 */
    while (idx <= bit->n) {
        bit->tree[idx] += delta;
        idx += lowbit(idx);
    }

    return 0;
}

int64_t ds_fenwick_prefix_sum(const ds_fenwick_t *bit, size_t idx)
{
    int64_t sum;

    if (!bit || idx > bit->n) {
        return 0;
    }

    sum = 0;
    /* 向下遍历：累加互不重叠的前缀区间 */
    while (idx > 0u) {
        sum += bit->tree[idx];
        idx -= lowbit(idx);
    }

    return sum;
}

int64_t ds_fenwick_range_sum(const ds_fenwick_t *bit, size_t L, size_t R)
{
    if (!bit || L == 0u || L > R || R > bit->n) {
        return 0;
    }

    /* 区间和 = 前缀和(R) - 前缀和(L-1) */
    return ds_fenwick_prefix_sum(bit, R) - ds_fenwick_prefix_sum(bit, L - 1u);
}

/* ============================================================
 * 演示函数
 * ============================================================ */

void ds_fenwick_tree_demo(void)
{
    printf("========== 树状数组演示 ==========\n");

    int64_t arr[] = {1, 3, 5, 7, 9, 11};
    size_t n = 6;

    printf("\n原数组: [");
    for (size_t i = 0u; i < n; ++i) printf("%lld%s", (long long)arr[i], i < n-1 ? ", " : "");
    printf("]\n");

    ds_fenwick_t *bit = ds_fenwick_create(arr, n);
    if (!bit) { printf("创建失败\n"); return; }

    printf("前缀和[1..3]: %lld\n", (long long)ds_fenwick_prefix_sum(bit, 3));
    printf("前缀和[1..6]: %lld\n", (long long)ds_fenwick_prefix_sum(bit, 6));
    printf("区间和[2..5]: %lld\n", (long long)ds_fenwick_range_sum(bit, 2, 5));

    printf("\n【单点更新：arr[3] += 10】\n");
    ds_fenwick_update(bit, 3, 10);
    printf("前缀和[1..3] 更新后: %lld\n", (long long)ds_fenwick_prefix_sum(bit, 3));
    printf("区间和[2..5] 更新后: %lld\n", (long long)ds_fenwick_range_sum(bit, 2, 5));

    /* 逆序对数量演示 */
    printf("\n【应用：求逆序对数量】\n");
    int test_arr[] = {5, 3, 2, 4, 1};
    printf("数组: [5, 3, 2, 4, 1]\n");

    /*
     * 离散化 + BIT 求逆序对：
     * 1. 将元素映射到其排名（离散化）
     * 2. 从右向左遍历，对每个元素：
     *    a. 查询比它小的元素数量 = prefix_sum(rank-1)
     *    b. 在 rank 位置 +1
     */
    /* 简化：直接使用数值（假设数值范围在位数级内） */
    /* 对于 [5,3,2,4,1]，BIT 大小取最大值 5 */
    int64_t zero_arr[6] = {0};
    ds_fenwick_t *inv_bit = ds_fenwick_create(zero_arr, 5);
    int64_t inv_count = 0;

    for (int i = 4; i >= 0; --i) {
        int val = test_arr[i];
        /* 查询比 val 小的元素数（已处理的） */
        if (val > 1) {
            inv_count += ds_fenwick_prefix_sum(inv_bit, (size_t)(val - 1));
        }
        /* 将当前元素标记为"已处理" */
        ds_fenwick_update(inv_bit, (size_t)val, 1);
    }
    printf("逆序对总数: %lld\n", (long long)inv_count);

    ds_fenwick_destroy(inv_bit);
    ds_fenwick_destroy(bit);
    printf("========== 树状数组演示结束 ==========\n");
}
