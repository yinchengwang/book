/* control_flow scaffold — 控制流
 *
 * 复现命令：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *
 * 演示 6 段：
 *   [if]       — if/else 嵌套优化（早 return）
 *   [switch]   — switch 多 case + fallthrough
 *   [for]      — for 三段式
 *   [while]    — while vs do-while
 *   [goto]     — goto 跳出嵌套循环
 *   [break-continue] — break/continue 在 switch 与 for 中的差异
 */

#include <stdio.h>
#include <stdlib.h>

static int classify(int n) {
    /* [if] 早 return 减少嵌套层级 */
    if (n < 0)   return -1;   /* 负数 */
    if (n == 0)  return 0;    /* 零 */
    if (n < 10)  return 1;    /* 个位数 */
    if (n < 100) return 2;    /* 两位数 */
    return 3;                 /* 三位及以上 */
}

static void demo_switch(int op) {
    /* [switch] 多 case + fallthrough 演示 */
    switch (op) {
        case 0: printf("  case 0: zero\n");
                break;
        case 1: printf("  case 1: one\n");
                /* fallthrough：故意不 break */
        case 2: printf("  case 1/2: 合并分支\n");
                break;
        case 3:
        case 4:
        case 5: printf("  case 3/4/5: 三合一\n");
                break;
        default: printf("  default: 其他\n");
    }
}

static void demo_for(void) {
    /* [for] 三段式：初始化 / 条件 / 步进 */
    printf("  for i=0..4: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", i);
    }
    printf("\n");

    /* C99：for 循环初始化可声明变量 */
    printf("  for (int i=0; i<5; i++): ");
    for (int i = 0; i < 5; i++) printf("%d ", i);
    printf("\n");

    /* 空循环体用单语句 */
    int sum = 0;
    for (int i = 1; i <= 10; i++) sum += i;
    printf("  sum 1..10 = %d\n", sum);
}

static void demo_while(void) {
    /* [while] 先检查后执行（可能 0 次） */
    int x = 0;
    printf("  while (x<3): ");
    while (x < 3) printf("%d ", x++);
    printf("\n");

    /* [do-while] 先执行后检查（至少 1 次） */
    int y = 0;
    printf("  do-while (y<3): ");
    do { printf("%d ", y++); } while (y < 3);
    printf("\n");
}

static int find_in_matrix(int target) {
    /* [goto] 在嵌套循环中跳出 —— goto cleanup 是合法用法 */
    int matrix[3][3] = {{1,2,3},{4,5,6},{7,8,9}};
    int found_i = -1, found_j = -1;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (matrix[i][j] == target) {
                found_i = i;
                found_j = j;
                goto found;        /* 一跳出双层循环 */
            }
        }
    }
found:
    if (found_i >= 0) {
        printf("  found %d at [%d][%d]\n", target, found_i, found_j);
        return 1;
    } else {
        printf("  not found %d\n", target);
        return 0;
    }
}

int main(void) {
    /* === [if] 早 return === */
    printf("[if] === if/else 早 return 减少嵌套 ===\n");
    for (int n = -5; n <= 105; n += 20) {
        printf("  classify(%d) = %d\n", n, classify(n));
    }

    /* === [switch] 多 case === */
    printf("\n[switch] === switch 多 case + fallthrough ===\n");
    for (int op = 0; op <= 6; op++) {
        printf("  op=%d -> ", op);
        demo_switch(op);
    }

    /* === [for] 三段式 === */
    printf("\n[for] === for 循环 ===\n");
    demo_for();

    /* === [while vs do-while] === */
    printf("\n[while] === while 与 do-while 对比 ===\n");
    demo_while();

    /* === [goto] 跳出嵌套循环 === */
    printf("\n[goto] === goto 跳出嵌套循环 ===\n");
    find_in_matrix(5);
    find_in_matrix(99);

    /* === [break-continue] === */
    printf("\n[break-continue] === break/continue 行为 ===\n");

    /* break 跳出最近一层循环/switch */
    printf("  break: ");
    for (int i = 0; i < 10; i++) {
        if (i == 5) break;
        printf("%d ", i);
    }
    printf("\n");

    /* continue 跳到本轮末尾 */
    printf("  continue (skip 5): ");
    for (int i = 0; i < 10; i++) {
        if (i == 5) continue;
        printf("%d ", i);
    }
    printf("\n");

    printf("\n=== PASS ===\n");
    return 0;
}