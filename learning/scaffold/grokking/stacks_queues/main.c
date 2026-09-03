/**
 * @file stacks_queues/main.c
 * @brief 栈与队列高频算法题演示
 *
 * 包含:
 * 1. 括号匹配（栈）
 * 2. 最小栈（带 min 操作）
 * 3. 约瑟夫环（队列模拟）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// ============================================================================
// 1. 括号匹配 - 栈
// ============================================================================

/**
 * 判断括号字符串是否有效 (LeetCode #20)
 * 左括号必须用同类型右括号闭合，且顺序正确
 *
 * 时间: O(n), 空间: O(n)
 *
 * @param s 括号字符串
 * @return 1 有效, 0 无效
 */
int is_valid_parentheses(const char *s) {
    int n = (int)strlen(s);
    // 奇数长度一定不匹配
    if (n % 2 != 0) return 0;

    // 用数组模拟栈
    char *stack = (char *)malloc(n * sizeof(char));
    int top = -1;

    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c == '(' || c == '{' || c == '[') {
            // 左括号入栈
            stack[++top] = c;
        } else {
            // 栈空但遇到右括号 → 无效
            if (top < 0) {
                free(stack);
                return 0;
            }
            // 检查是否匹配
            char left = stack[top--];
            if ((c == ')' && left != '(') ||
                (c == '}' && left != '{') ||
                (c == ']' && left != '[')) {
                free(stack);
                return 0;
            }
        }
    }

    int ok = (top == -1);  // 栈空才完全匹配
    free(stack);
    return ok;
}

// ============================================================================
// 2. 最小栈 - 辅助栈
// ============================================================================

/**
 * 最小栈数据结构
 * 支持 push, pop, top, get_min 操作
 * 所有操作 O(1)
 */
typedef struct {
    int *data;      // 数据栈
    int *min;       // 最小栈（同步记录当前最小值）
    int capacity;
    int top_data;   // 数据栈顶
    int top_min;    // 最小栈顶
} MinStack;

MinStack *min_stack_create(int capacity) {
    MinStack *s = (MinStack *)malloc(sizeof(MinStack));
    s->data = (int *)malloc(capacity * sizeof(int));
    s->min  = (int *)malloc(capacity * sizeof(int));
    s->capacity = capacity;
    s->top_data = -1;
    s->top_min  = -1;
    return s;
}

void min_stack_push(MinStack *s, int val) {
    if (s->top_data + 1 >= s->capacity) return;
    s->data[++s->top_data] = val;

    // 最小栈同步：当前最小值 = min(当前值, 栈顶最小值)
    if (s->top_min < 0) {
        s->min[++s->top_min] = val;
    } else {
        int cur_min = (val < s->min[s->top_min]) ? val : s->min[s->top_min];
        s->min[++s->top_min] = cur_min;
    }
}

void min_stack_pop(MinStack *s) {
    if (s->top_data < 0) return;
    s->top_data--;
    s->top_min--;
}

int min_stack_top(MinStack *s) {
    return s->data[s->top_data];
}

int min_stack_get_min(MinStack *s) {
    return s->min[s->top_min];
}

void min_stack_free(MinStack *s) {
    free(s->data);
    free(s->min);
    free(s);
}

// ============================================================================
// 3. 约瑟夫环 - 队列模拟
// ============================================================================

/**
 * 约瑟夫环问题 (环状报数)
 * n 个人围成一圈，从第 1 个开始报数，报到 k 的人出列
 * 返回最后剩下的人的原编号 (1-based)
 *
 * 用队列模拟: 反复将前 k-1 人移到队尾，第 k 人出队
 * 时间: O(n*k), 空间: O(n)
 *
 * @param n 总人数
 * @param k 报数步长
 * @return 最后剩下的人的编号
 */
int josephus(int n, int k) {
    // 用数组模拟循环队列
    // 最大需要: n + (n-1)*(k-1) <= n*k，留 5 的余量
    int capacity = n * k + 5;
    if (capacity < n) capacity = n;  // k=0 时兜底
    int *queue = (int *)malloc(capacity * sizeof(int));
    int front = 0, rear = 0;

    // 初始编号 1..n 入队
    for (int i = 1; i <= n; i++) {
        queue[rear++] = i;
    }

    while (rear - front > 1) {
        // 报数 1..k-1: 移到队尾
        for (int i = 1; i < k; i++) {
            queue[rear++] = queue[front++];
        }
        // 报数 k: 出队（淘汰）
        front++;
    }

    int survivor = queue[front];
    free(queue);
    return survivor;
}

// ============================================================================
// 主函数
// ============================================================================

int main(void) {
    printf("=== 栈与队列高频算法题演示 ===\n\n");

    // ------------------------------------------------------------
    // 1. 括号匹配测试
    // ------------------------------------------------------------
    printf("--- 1. 括号匹配 (栈 O(n)) ---\n");
    const char *tests[] = {
        "()",
        "()[]{}",
        "(]",
        "([)]",
        "{[]}",
        "((()))",
        "",
        "(",
    };
    int num_tests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < num_tests; i++) {
        int valid = is_valid_parentheses(tests[i]);
        printf("  \"%s\" -> %s\n", tests[i], valid ? "有效" : "无效");
    }

    // ------------------------------------------------------------
    // 2. 最小栈测试
    // ------------------------------------------------------------
    printf("\n--- 2. 最小栈 (O(1) min 操作) ---\n");
    MinStack *ms = min_stack_create(100);
    int push_vals[] = {3, 5, 2, 1, 4};
    int num_push = 5;

    printf("  入栈顺序: ");
    for (int i = 0; i < num_push; i++) {
        min_stack_push(ms, push_vals[i]);
        printf("%d ", push_vals[i]);
    }
    printf("\n");

    printf("  当前 min = %d\n", min_stack_get_min(ms));
    min_stack_pop(ms);
    printf("  pop 后 min = %d\n", min_stack_get_min(ms));
    min_stack_pop(ms);
    printf("  pop 后 min = %d\n", min_stack_get_min(ms));
    printf("  当前 top = %d\n", min_stack_top(ms));

    min_stack_free(ms);

    // ------------------------------------------------------------
    // 3. 约瑟夫环测试
    // ------------------------------------------------------------
    printf("\n--- 3. 约瑟夫环 (队列模拟) ---\n");
    printf("  n=7, k=3 -> 幸存者: %d\n", josephus(7, 3));
    printf("  n=5, k=2 -> 幸存者: %d\n", josephus(5, 2));
    printf("  n=41, k=3 -> 幸存者: %d\n", josephus(41, 3));

    printf("\n=== 演示完成 ===\n");
    return 0;
}
