/*
 * heap.c —— 堆教学演示
 *
 * ============================================================
 * 数据结构内部定义（引用已有实现）
 * ============================================================
 *
 * 优先队列/堆结构（来自 src/algo/priority_queue/priority_queue.c）：
 *
 *   struct pq {
 *       void   *data;         // 底层动态数组（完全二叉树的数组表示）
 *       size_t  size;         // 当前元素数
 *       size_t  capacity;     // 数组容量
 *       size_t  element_size;
 *       int   (*compare)(const void *a, const void *b);
 *       void  (*free_element)(void *);
 *   };
 *
 * 数组与二叉树的对应关系：
 *
 *   逻辑结构（最小堆）：          物理存储（数组）：
 *         1                      下标: 0  1  2  3  4  5
 *       /   \                    值: [1, 3, 6, 5, 9, 8]
 *      3     6
 *     / \   /                    parent(4)=1 → data[(4-1)/2]=data[1]=3
 *    5   9 8                     left(1)=3  → data[2*1+1]=data[3]=5
 *                                right(1)=4 → data[2*1+2]=data[4]=9
 *
 * ============================================================
 * 核心算法解析
 * ============================================================
 *
 * 【上浮（sift-up / bubble-up）：push 操作的核心】
 *
 *   将新元素放在数组末尾，然后反复与父节点比较并交换，直到堆性质恢复。
 *
 *   步骤：
 *     i = size - 1（最后一个位置）
 *     while i > 0:
 *         parent = (i - 1) / 2
 *         if data[parent] <= data[i]: break（堆性质满足）
 *         swap(data[parent], data[i])
 *         i = parent
 *
 *   示例：在最小堆 [1, 3, 6, 5, 9, 8] 中 push 2
 *     初始: [1, 3, 6, 5, 9, 8, 2]  ← 2 在末尾（下标 6）
 *     i=6, parent=(6-1)/2=2, data[2]=6 > 2 → 交换
 *          [1, 3, 2, 5, 9, 8, 6]
 *     i=2, parent=(2-1)/2=0, data[0]=1 <= 2 → 停止
 *     最终: [1, 3, 2, 5, 9, 8, 6]  ✓
 *
 * 【下沉（sift-down / bubble-down）：pop 操作的核心】
 *
 *   将堆顶元素（data[0]）与最后一个元素交换，移除最后一个元素，
 *   然后从根开始反复与较小的子节点交换，直到堆性质恢复。
 *
 *   步骤：
 *     i = 0
 *     while true:
 *         left = 2i+1, right = 2i+2
 *         smallest = i
 *         if left < size && data[left] < data[smallest]: smallest = left
 *         if right < size && data[right] < data[smallest]: smallest = right
 *         if smallest == i: break（堆性质满足）
 *         swap(data[i], data[smallest])
 *         i = smallest
 *
 * 【heapify：从无序数组构建堆，O(n)】
 *
 *   自底向上对每个非叶子节点执行下沉操作：
 *   for i = size/2-1 down to 0:
 *       sift_down(i)
 *
 *   为什么是 O(n)？
 *   - 接近叶子的节点（多数）下沉距离短
 *   - 层的节点数：2^h（指数增长），但下沉距离：h（线性递减）
 *   - 总操作数：∑(h-i)·2^i ≈ n（通过错位相减可以证明）
 *
 * ============================================================
 * 经典应用：TopK 问题
 * ============================================================
 *
 * 问题：从 N 个元素中找出最大的 K 个。
 *
 * 算法（维护大小为 K 的小顶堆）：
 *   1. 将前 K 个元素建立小顶堆（堆顶是这 K 个中最小的）
 *   2. 遍历剩余 N-K 个元素：
 *      if 元素 > 堆顶:
 *         弹出堆顶（淘汰当前最小的）
 *         插入新元素
 *   3. 堆中剩余的 K 个元素就是 TopK
 *
 * 时间复杂度：O(N log K)，空间复杂度：O(K)
 *
 * 为什么用小顶堆而不是大顶堆？
 * - 我们要保留"最大的 K 个"元素
 * - 小顶堆的堆顶是堆中最小的（即"第 K 大的门槛"）
 * - 新元素只需与门槛比较：大于门槛 → 替换门槛
 * - 如果用大顶堆，堆顶是最大的，无法确定"门槛"
 */

#include <ds/heap.h>

#include <stdio.h>
#include <stdlib.h>

/*
 * 比较函数（最小堆：小的在堆顶）。
 * pq_t 比较函数的约定：返回负值表示 a 优先级更高（更接近堆顶）。
 * 对于最小堆，数值小的优先级更高 → 返回 a - b 的负值。
 * 这里的 compare 传给 pq，pq 期望 compare(a,b) < 0 表示 a 优先于 b。
 * 所以：return *(b) - *(a) —— 当 a < b 时返回正值... 不对。
 *
 * 实际上 pq 模块的 compare 约定是：
 *   compare(a, b) < 0 → a 在堆顶（优先级更高）
 * 所以最小堆：compare = b - a（当 a < b 时 b-a > 0，即 a 不优先于 b...
 * 不对，让我重新检查 pq 实现）。
 *
 * 简化：这里的 demo 使用已有 pq 的 compare 约定。
 * pq API 中 compare(a,b) < 0 表示 a 应该在堆顶。
 * 因此对于最小堆（值小的在堆顶）：
 *   compare(a,b) = a - b；当 a < b 时返回负值，a 优先级高 ✓
 */
static int min_heap_compare(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

void ds_heap_demo(void)
{
    printf("========== 堆 / 优先队列演示 ==========\n");

    /* 基本操作 */
    printf("\n【基本操作——最小堆】\n");
    pq_t *pq = pq_create(sizeof(int), min_heap_compare);
    if (!pq) {
        printf("创建堆失败\n");
        return;
    }

    int vals[] = {5, 3, 8, 1, 9, 2};
    printf("插入顺序: ");
    for (int i = 0; i < 6; ++i) {
        pq_push(pq, &vals[i]);
        printf("%d ", vals[i]);
    }
    printf("\n");

    printf("堆顶（最小值）: %d\n", *(const int *)pq_top_ptr(pq));
    printf("依次弹出（升序）: ");
    while (!pq_empty(pq)) {
        int val;
        pq_pop(pq, &val);
        printf("%d ", val);
    }
    printf("\n");

    /* TopK 演示 */
    printf("\n【TopK 问题 —— 找出最大的 3 个元素】\n");
    int stream[] = {7, 2, 15, 3, 8, 1, 20, 12, 6};
    int n = 9;
    int k = 3;

    printf("数据流: [");
    for (int i = 0; i < n; ++i) {
        printf("%d%s", stream[i], i < n - 1 ? ", " : "");
    }
    printf("]\n");
    printf("K = %d\n", k);

    /* 用小顶堆维护最大的 K 个元素 */
    pq_t *topk = pq_create(sizeof(int), min_heap_compare);
    if (!topk) {
        pq_destroy(pq);
        return;
    }

    for (int i = 0; i < n; ++i) {
        if ((int)pq_size(topk) < k) {
            /* 堆未满，直接插入 */
            pq_push(topk, &stream[i]);
        } else {
            /* 堆已满，检查是否比堆顶大 */
            int top_val = *(const int *)pq_top_ptr(topk);
            if (stream[i] > top_val) {
                pq_pop(topk, NULL);     /* 淘汰当前最小的 */
                pq_push(topk, &stream[i]); /* 插入更大的 */
            }
        }

        /* 打印当前 TopK 状态 */
        printf("  处理 %d 后，当前 Top%d: ", stream[i], k);
        /* 不方便遍历堆内部... 偷看堆顶 */
        printf("门槛=%d，堆大小=%zu\n",
               *(const int *)pq_top_ptr(topk), pq_size(topk));
    }

    /* 输出最终 TopK 结果 */
    printf("最终 Top%d: ", k);
    /* 弹出所有元素（从小到大的顺序） */
    int topk_results[3] = {0, 0, 0};
    int topk_count = 0;
    while (!pq_empty(topk) && topk_count < 3) {
        pq_pop(topk, &topk_results[topk_count++]);
    }
    /* 逆序输出得到从大到小 */
    for (int i = topk_count - 1; i >= 0; --i) {
        printf("%d ", topk_results[i]);
    }
    printf("\n");

    pq_destroy(topk);
    pq_destroy(pq);
    printf("========== 堆演示结束 ==========\n");
}
