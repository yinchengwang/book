/**
 * two_pointers.c - 双指针技巧演示
 *
 * 用 C11 实现经典双指针算法：
 * 1. 三数之和（3Sum）
 * 2. 容器装水（接雨水）
 * 3. 环形链表入口（环形链表 II）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== 三数之和 ========== */

/** 比较函数用于 qsort */
static int cmp_int(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

/**
 * 找出数组中所有和为 0 的三元组
 * 结果写入 result（每个三元组三个 int），返回三元组个数
 */
static int three_sum(int *nums, int nums_size,
                     int result[][3], int result_cap)
{
    int count = 0;

    /* 排序是双指针的前提 */
    qsort(nums, (size_t)nums_size, sizeof(int), cmp_int);

    for (int i = 0; i < nums_size - 2 && count < result_cap; i++) {
        /* 跳过重复的第一个数 */
        if (i > 0 && nums[i] == nums[i - 1]) continue;

        int left  = i + 1;
        int right = nums_size - 1;

        while (left < right) {
            int sum = nums[i] + nums[left] + nums[right];

            if (sum == 0) {
                result[count][0] = nums[i];
                result[count][1] = nums[left];
                result[count][2] = nums[right];
                count++;

                /* 跳过重复元素 */
                int lv = nums[left];
                while (left < right && nums[left] == lv) left++;
                int rv = nums[right];
                while (left < right && nums[right] == rv) right--;
            } else if (sum < 0) {
                left++;
            } else {
                right--;
            }
        }
    }

    return count;
}

/* ========== 容器装水（接雨水） ========== */

/**
 * 给定 n 个非负整数表示柱子高度，计算接水量
 * 双指针法：O(n) 时间，O(1) 空间
 */
static int trap_rain_water(int *height, int height_size)
{
    if (height_size <= 2) return 0;

    int left  = 0;
    int right = height_size - 1;
    int left_max  = 0;
    int right_max = 0;
    int water = 0;

    while (left < right) {
        if (height[left] < height[right]) {
            if (height[left] >= left_max) {
                left_max = height[left];
            } else {
                water += left_max - height[left];
            }
            left++;
        } else {
            if (height[right] >= right_max) {
                right_max = height[right];
            } else {
                water += right_max - height[right];
            }
            right--;
        }
    }

    return water;
}

/* ========== 环形链表入口 ========== */

/** 链表节点定义 */
typedef struct list_node {
    int val;
    struct list_node *next;
} list_node_t;

/**
 * 检测环形链表入口节点
 * 快慢指针：相遇后从 head 和相遇点同步前进，再次相遇即为入口
 * 返回值：入口节点指针，无环返回 NULL
 */
static list_node_t *detect_cycle(list_node_t *head)
{
    if (!head || !head->next) return NULL;

    list_node_t *slow = head;
    list_node_t *fast = head;

    /* 第一阶段：快慢指针相遇 */
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) {
            /* 第二阶段：找入口 */
            list_node_t *entry = head;
            while (entry != slow) {
                entry = entry->next;
                slow  = slow->next;
            }
            return entry;
        }
    }

    return NULL; /* 无环 */
}

/** 创建链表（辅助函数） */
static list_node_t *list_create(int *vals, int n)
{
    if (n <= 0) return NULL;
    list_node_t *head = malloc(sizeof(list_node_t));
    head->val  = vals[0];
    head->next = NULL;
    list_node_t *cur = head;
    for (int i = 1; i < n; i++) {
        cur->next = malloc(sizeof(list_node_t));
        cur = cur->next;
        cur->val  = vals[i];
        cur->next = NULL;
    }
    return head;
}

/** 释放链表（辅助函数，不含环） */
static void list_free(list_node_t *head)
{
    while (head) {
        list_node_t *tmp = head;
        head = head->next;
        free(tmp);
    }
}

/* ========== 测试框架 ========== */

static int total_tests  = 0;
static int passed_tests = 0;

#define TEST(cond, fmt, ...) do {                                 \
    total_tests++;                                                \
    if (!(cond)) {                                                \
        printf("  FAIL: " fmt "\n", ##__VA_ARGS__);              \
    } else {                                                      \
        passed_tests++;                                           \
        printf("  PASS\n");                                       \
    }                                                             \
} while (0)

static void test_three_sum(void)
{
    printf("[三数之和]\n");

    int nums1[] = {-1, 0, 1, 2, -1, -4};
    int res1[10][3];
    int cnt1 = three_sum(nums1, 6, res1, 10);
    TEST(cnt1 == 2, "期望 2 个三元组，实际 %d", cnt1);

    int nums2[] = {0, 0, 0};
    int res2[10][3];
    int cnt2 = three_sum(nums2, 3, res2, 10);
    TEST(cnt2 == 1, "期望 1 个三元组，实际 %d", cnt2);

    int nums3[] = {1, 2, 3};
    int res3[10][3];
    int cnt3 = three_sum(nums3, 3, res3, 10);
    TEST(cnt3 == 0, "期望 0 个三元组，实际 %d", cnt3);
}

static void test_trap_water(void)
{
    printf("[容器装水（接雨水）]\n");

    int h1[] = {0,1,0,2,1,0,1,3,2,1,2,1};
    int w1 = trap_rain_water(h1, 12);
    TEST(w1 == 6, "期望 6，实际 %d", w1);

    int h2[] = {4,2,0,3,2,5};
    int w2 = trap_rain_water(h2, 6);
    TEST(w2 == 9, "期望 9，实际 %d", w2);

    int h3[] = {1,2,3};
    int w3 = trap_rain_water(h3, 3);
    TEST(w3 == 0, "期望 0，实际 %d", w3);

    int h4[] = {0};
    int w4 = trap_rain_water(h4, 1);
    TEST(w4 == 0, "期望 0，实际 %d", w4);
}

static void test_cycle_detect(void)
{
    printf("[环形链表入口]\n");

    /* 测试 1：无环链表 */
    int vals1[] = {1, 2, 3, 4};
    list_node_t *l1 = list_create(vals1, 4);
    TEST(detect_cycle(l1) == NULL, "无环链表期望 NULL");

    /* 测试 2：环在尾部 */
    int vals2[] = {1, 2, 3, 4, 5};
    list_node_t *l2 = list_create(vals2, 5);
    /* 找到尾部节点，回连到第 3 个节点（val=3） */
    list_node_t *tail2 = l2;
    list_node_t *entry2 = NULL;
    int idx = 0;
    while (tail2->next) {
        idx++;
        if (idx == 2) entry2 = tail2; /* 第 3 个节点 */
        tail2 = tail2->next;
    }
    tail2->next = entry2; /* 成环 */

    list_node_t *found2 = detect_cycle(l2);
    TEST(found2 == entry2, "环入口期望 val=3，实际 val=%d", found2 ? found2->val : -1);

    /* 断开环避免 free 卡死，清理 */
    tail2->next = NULL;
    list_free(l2);

    /* 测试 3：无环单节点 */
    list_node_t *l3 = malloc(sizeof(list_node_t));
    l3->val = 42;
    l3->next = NULL;
    TEST(detect_cycle(l3) == NULL, "单节点无环期望 NULL");
    free(l3);
}

int main(void)
{
    printf("===== 双指针技巧 =====\n\n");

    test_three_sum();
    putchar('\n');
    test_trap_water();
    putchar('\n');
    test_cycle_detect();
    putchar('\n');

    printf("结果: %d / %d 测试通过\n", passed_tests, total_tests);

    return (passed_tests == total_tests) ? 0 : 1;
}
