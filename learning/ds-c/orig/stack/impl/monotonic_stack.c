/*
 * monotonic_stack.c —— 单调栈实现
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 * 单调栈是在普通栈的基础上增加了"维护单调性"的逻辑。
 * 底层使用动态数组，结构同普通栈：
 *
 *   struct ds_mono_stack {
 *       void   *data;
 *       size_t  size;
 *       size_t  capacity;
 *       size_t  element_size;
 *       ds_mono_stack_compare_fn compare;  // 比较函数
 *   };
 *
 * ============================================================
 * 核心算法：push 维护单调性
 * ============================================================
 *
 * 以单调递减栈为例（找"下一个更大元素"）：
 *
 *   push(5):  栈: [5]
 *   push(3):  栈: [5, 3]       (3 <= 5, 保持递减)
 *   push(7):  弹出 3 和 5，栈: [7]  (7 > 3, 7 > 5, 全部弹出)
 *
 *   在弹出 3 时：3 的下一个更大元素 = 7
 *   在弹出 5 时：5 的下一个更大元素 = 7
 *
 * 伪代码：
 *   mono_stack_push(stack, x):
 *       while !empty(stack) && compare(top, x) < 0:
 *           // top < x（对于递减栈），需要弹出
 *           pop(stack)
 *       // 栈恢复单调性
 *       push(stack, x)
 *
 * ============================================================
 * 经典应用 1：下一个更大元素
 * ============================================================
 *
 * 问题：给定数组 nums，对每个位置 i，找到其右侧第一个比 nums[i] 大的元素。
 *
 * 算法（从右向左遍历 + 递减栈）：
 *   for i = n-1 down to 0:
 *       // 弹出所有 <= nums[i] 的元素（它们被 nums[i] 挡住了）
 *       while !empty && top <= nums[i]: pop()
 *       // 栈顶就是 nums[i] 的下一个更大元素
 *       result[i] = empty ? -1 : top
 *       push(nums[i])
 *
 * 示例：nums = [2, 1, 2, 4, 3]
 *   i=4: nums[4]=3, 栈空 → result[4]=-1, push 3      栈: [3]
 *   i=3: nums[3]=4, pop 3 → result[3]=-1, push 4     栈: [4]
 *   i=2: nums[2]=2, 4>2 → result[2]=4, push 2        栈: [4, 2]
 *   i=1: nums[1]=1, 2>1 → result[1]=2, push 1        栈: [4, 2, 1]
 *   i=0: nums[0]=2, pop 1, 2==2 pop, 4>2 → result[0]=4
 *   结果: [4, 2, 4, -1, -1]
 *
 * ============================================================
 * 经典应用 2：柱状图中最大矩形
 * ============================================================
 *
 * 问题：给定 heights 数组表示柱子高度，求能勾勒出的最大矩形面积。
 *
 * 算法（单调递增栈，找每个柱子的左右边界）：
 *   for i = 0 to n:
 *       while !empty && heights[top] > heights[i]:
 *           h = heights[pop()]
 *           width = empty ? i : i - top - 1
 *           max_area = max(max_area, h * width)
 *       push(i)
 *
 * 关键洞察：当 heights[i] < heights[栈顶] 时，栈顶柱子的"右边界"就是 i，
 * 而"左边界"是栈中它下面的那个元素。由此确定了以栈顶柱子为高的矩形范围。
 */

#include <ds/monotonic_stack.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DS_MONO_STACK_DEFAULT_CAPACITY 16u
#define DS_MONO_STACK_GROWTH_FACTOR 2u

struct ds_mono_stack {
    void  *data;
    size_t size;
    size_t capacity;
    size_t element_size;
    ds_mono_stack_compare_fn compare;
};

/* 辅助函数 */
static unsigned char *mono_stack_elem(ds_mono_stack_t *stack, size_t index)
{
    return (unsigned char *)stack->data + index * stack->element_size;
}

static const unsigned char *mono_stack_elem_const(const ds_mono_stack_t *stack, size_t index)
{
    return (const unsigned char *)stack->data + index * stack->element_size;
}

static int mono_stack_grow(ds_mono_stack_t *stack)
{
    size_t new_capacity;
    void  *new_data;

    new_capacity = stack->capacity * DS_MONO_STACK_GROWTH_FACTOR;
    if (new_capacity <= stack->capacity) {
        new_capacity = stack->capacity + 1u;
    }

    new_data = realloc(stack->data, new_capacity * stack->element_size);
    if (!new_data) {
        return -1;
    }

    stack->data = new_data;
    stack->capacity = new_capacity;
    return 0;
}

/* 公共 API */
ds_mono_stack_t *ds_mono_stack_create(size_t element_size, ds_mono_stack_compare_fn compare)
{
    ds_mono_stack_t *stack;

    if (element_size == 0u || !compare) {
        return NULL;
    }

    stack = (ds_mono_stack_t *)calloc(1u, sizeof(ds_mono_stack_t));
    if (!stack) {
        return NULL;
    }

    stack->data = malloc(DS_MONO_STACK_DEFAULT_CAPACITY * element_size);
    if (!stack->data) {
        free(stack);
        return NULL;
    }

    stack->capacity = DS_MONO_STACK_DEFAULT_CAPACITY;
    stack->element_size = element_size;
    stack->compare = compare;
    return stack;
}

void ds_mono_stack_destroy(ds_mono_stack_t *stack)
{
    if (!stack) {
        return;
    }
    free(stack->data);
    free(stack);
}

/*
 * push 操作：自动维护单调性。
 * 返回 pop 的次数（明确返回 int 型，可用于辅助计算跨度）。
 */
int ds_mono_stack_push(ds_mono_stack_t *stack, const void *element)
{
    int pop_count;

    if (!stack || !element) {
        return -1;
    }

    /* 弹出所有破坏单调性的栈顶元素 */
    pop_count = 0;
    while (stack->size > 0u) {
        const void *top = mono_stack_elem_const(stack, stack->size - 1u);
        /*
         * 调用 compare(element, top)：
         *   返回 > 0 表示 element > top（对于递减栈应 pop）
         *   返回 < 0 表示 element < top（对于递增栈应 pop）
         *
         * 具体判断交给调用者定义的 compare 函数处理。
         *
         * 对于找"下一个更大元素"场景（递减栈）：
         *   compare 应为：b - a（升序判断），栈顶 < 新元素时弹出。
         *
         * 更通用的方式：我们定义 compare(a, b) 返回 a-b 的值。
         * 当 compare(element, top) > 0 时，表示 element 大于 top，
         * 此时需要弹出 top 以维护栈的"递减"性质。
         *
         * 简化处理：每次 push 时由调用者通过 compare 的返回值
         * 来决定弹出逻辑。此处我们约定 compare(element, top) > 0
         * 时弹出（适用于递减栈场景）。
         */
        if (stack->compare(element, top) > 0) {
            /* 破坏单调性，弹出栈顶 */
            stack->size -= 1u;
            ++pop_count;
        } else {
            break;
        }
    }

    /* 确保有足够空间 */
    if (stack->size >= stack->capacity && mono_stack_grow(stack) != 0) {
        return -1;
    }

    memcpy(mono_stack_elem(stack, stack->size), element, stack->element_size);
    stack->size += 1u;
    return pop_count;
}

int ds_mono_stack_pop(ds_mono_stack_t *stack, void *out)
{
    if (!stack || stack->size == 0u) {
        return -1;
    }

    stack->size -= 1u;
    if (out) {
        memcpy(out, mono_stack_elem(stack, stack->size), stack->element_size);
    }
    return 0;
}

int ds_mono_stack_top(const ds_mono_stack_t *stack, void *out)
{
    if (!stack || stack->size == 0u || !out) {
        return -1;
    }

    memcpy(out, mono_stack_elem_const(stack, stack->size - 1u), stack->element_size);
    return 0;
}

const void *ds_mono_stack_top_ptr(const ds_mono_stack_t *stack)
{
    if (!stack || stack->size == 0u) {
        return NULL;
    }

    return mono_stack_elem_const(stack, stack->size - 1u);
}

size_t ds_mono_stack_size(const ds_mono_stack_t *stack)
{
    return stack ? stack->size : 0u;
}

bool ds_mono_stack_empty(const ds_mono_stack_t *stack)
{
    return !stack || stack->size == 0u;
}

void ds_mono_stack_clear(ds_mono_stack_t *stack)
{
    if (!stack) {
        return;
    }
    stack->size = 0u;
}

/* ============================================================
 * 演示函数
 * ============================================================ */

static int int_compare_asc(const void *a, const void *b)
{
    /* a - b：返回正值表示 a > b */
    return *(const int *)a - *(const int *)b;
}

void ds_mono_stack_demo(void)
{
    printf("========== 单调栈演示 ==========\n");

    /* 下一个更大元素演示 */
    printf("\n【下一个更大元素（从右向左 + 单调递减栈）】\n");
    int nums[] = {2, 1, 2, 4, 3};
    int n = 5;
    int result[5] = {-1, -1, -1, -1, -1};

    printf("输入数组: [");
    for (int i = 0; i < n; ++i) {
        printf("%d%s", nums[i], i < n - 1 ? ", " : "");
    }
    printf("]\n");

    ds_mono_stack_t *stack = ds_mono_stack_create(sizeof(int), int_compare_asc);
    if (!stack) {
        printf("创建单调栈失败\n");
        return;
    }

    /* 从右向左遍历，找右侧第一个更大的元素 */
    for (int i = n - 1; i >= 0; --i) {
        /* 弹出所有 <= nums[i] 的元素（它们被 nums[i] 挡住了） */
        while (!ds_mono_stack_empty(stack)) {
            int top;
            ds_mono_stack_top(stack, &top);
            if (top <= nums[i]) {
                ds_mono_stack_pop(stack, NULL);
            } else {
                break;
            }
        }

        /* 栈顶就是 nums[i] 的下一个更大元素 */
        if (!ds_mono_stack_empty(stack)) {
            ds_mono_stack_top(stack, &result[i]);
        } else {
            result[i] = -1;
        }

        ds_mono_stack_push(stack, &nums[i]);

        /* 打印当前状态 */
        printf("  i=%d (val=%d): 栈中元素从底到顶: [", i, nums[i]);
        /* 这里我们只能通过不断弹出再压回来累述...为了演示简洁，直接用 size */
        printf("size=%zu", ds_mono_stack_size(stack));
        printf("] → 下一个更大=%d\n", result[i]);
    }

    printf("结果: [");
    for (int i = 0; i < n; ++i) {
        printf("%d%s", result[i], i < n - 1 ? ", " : "");
    }
    printf("]\n");

    ds_mono_stack_destroy(stack);
    printf("========== 单调栈演示结束 ==========\n");
}
