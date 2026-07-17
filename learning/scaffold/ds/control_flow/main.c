/**
 * @file main.c
 * @brief 数据结构控制流学习卡片 - 演示条件语句、循环与状态机
 *
 * 涵盖内容：
 * - 条件语句：if-else、switch
 * - 循环结构：for、while、do-while
 * - 状态机模式：有限状态自动机实现
 * - goto 合法使用：错误处理清理
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 条件语句演示 ==================== */

/**
 * if-else 条件语句演示
 * 演示嵌套 if-else 和 else-if 链
 */
static void demo_if_else(void)
{
    printf("[control_flow] if-else 条件语句演示\n");

    int score = 85;

    /* 简单 if-else */
    printf("  分数 = %d\n", score);
    if (score >= 60) {
        printf("  结果: 及格\n");
    } else {
        printf("  结果: 不及格\n");
    }

    /* else-if 链：成绩等级判断 */
    printf("  成绩等级判断:\n");
    if (score >= 90) {
        printf("    A (90-100)\n");
    } else if (score >= 80) {
        printf("    B (80-89)\n");
    } else if (score >= 70) {
        printf("    C (70-79)\n");
    } else if (score >= 60) {
        printf("    D (60-69)\n");
    } else {
        printf("    F (<60)\n");
    }
}

/**
 * switch 语句演示
 * 适合离散值的多分支判断，比 if-else 链更清晰
 */
static void demo_switch(void)
{
    printf("\n[control_flow] switch 语句演示\n");

    int op = 2;
    int a = 10, b = 3;

    printf("  操作码 = %d, a = %d, b = %d\n", op, a, b);

    switch (op) {
        case 1:
            printf("  结果: a + b = %d\n", a + b);
            break;
        case 2:
            printf("  结果: a - b = %d\n", a - b);
            break;
        case 3:
            printf("  结果: a * b = %d\n", a * b);
            break;
        case 4:
            if (b != 0) {
                printf("  结果: a / b = %d\n", a / b);
            } else {
                printf("  错误: 除数为零\n");
            }
            break;
        default:
            printf("  未知操作码\n");
            break;
    }
}

/* ==================== 循环结构演示 ==================== */

/**
 * for 循环演示
 * 适合已知迭代次数的场景
 */
static void demo_for_loop(void)
{
    printf("\n[control_flow] for 循环演示\n");

    /* 计算 1+2+...+10 */
    int sum = 0;
    printf("  计算 1+2+...+10:\n  ");
    for (int i = 1; i <= 10; i++) {
        sum += i;
        printf("%d%s", i, (i < 10) ? "+" : "");
    }
    printf(" = %d\n", sum);

    /* 遍历数组 */
    int arr[] = {5, 10, 15, 20, 25};
    size_t n = sizeof(arr) / sizeof(arr[0]);
    printf("  遍历数组 [5,10,15,20,25]:\n  ");
    for (size_t i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

/**
 * while 循环演示
 * 适合未知迭代次数、前置条件判断
 */
static void demo_while_loop(void)
{
    printf("\n[control_flow] while 循环演示\n");

    /* 计算阶乘 5! */
    int n = 5;
    int factorial = 1;
    int i = n;
    printf("  计算 %d!:\n  ", n);
    while (i > 1) {
        factorial *= i;
        printf("%d", i);
        if (i > 2) printf(" * ");
        i--;
    }
    printf(" = %d\n", factorial);

    /* 找到第一个大于 1000 的 2 的幂 */
    int power = 1;
    int count = 0;
    printf("  第一个大于 1000 的 2 的幂:\n  ");
    while (power <= 1000) {
        power *= 2;
        count++;
    }
    printf("  2^%d = %d\n", count, power);
}

/**
 * do-while 循环演示
 * 适合至少执行一次的场景
 */
static void demo_do_while(void)
{
    printf("\n[control_flow] do-while 循环演示\n");

    /* 菜单驱动的输入模拟（至少执行一次） */
    int choice;
    printf("  模拟菜单选择（至少执行一次）:\n");
    choice = 1;  /* 初始化为继续 */
    do {
        printf("    请选择: 1-继续, 0-退出\n");
        printf("    选择: %d\n", choice);
        if (choice == 1) {
            printf("    执行操作...\n");
            choice = 0;  /* 模拟下次选择退出 */
        }
    } while (choice != 0);
    printf("    退出循环\n");

    /* 猜数字游戏模拟（至少猜一次） */
    int secret = 42;
    int guess = 0;
    int attempts = 0;
    printf("  猜数字游戏模拟:\n");
    do {
        attempts++;
        /* 模拟几次猜测 */
        if (attempts == 1) guess = 30;
        else if (attempts == 2) guess = 50;
        else guess = secret;

        printf("    猜测 %d: %d ", attempts, guess);
        if (guess < secret) {
            printf("(太小)\n");
        } else if (guess > secret) {
            printf("(太大)\n");
        } else {
            printf("(正确!)\n");
        }
    } while (guess != secret);
    printf("    共猜了 %d 次\n", attempts);
}

/* ==================== 状态机模式演示 ==================== */

/**
 * HTTP 请求解析状态机（简化版）
 * 展示有限状态自动机在实际中的应用
 */
typedef enum {
    STATE_START,
    STATE_METHOD,
    STATE_PATH,
    STATE_VERSION,
    STATE_DONE,
    STATE_ERROR
} HttpParserState;

/**
 * HTTP 请求解析状态机
 * 状态转换：START -> METHOD -> PATH -> VERSION -> DONE
 */
static void demo_state_machine(void)
{
    printf("\n[control_flow] HTTP 解析状态机演示\n");

    /* 模拟解析 HTTP 请求行 "GET /index.html HTTP/1.1" */
    const char *request = "GET /index.html HTTP/1.1";
    const char *p = request;

    HttpParserState state = STATE_START;
    const char *method_start = NULL;
    const char *path_start = NULL;
    size_t method_len = 0;
    size_t path_len = 0;

    printf("  解析请求: \"%s\"\n", request);

    while (*p) {  /* 遍历直到字符串结束 */
        char ch = *p;

        switch (state) {
            case STATE_START:
                if (ch != ' ') {
                    method_start = p;
                    method_len = 1;  /* 第一个字符已计入 */
                    state = STATE_METHOD;
                }
                break;

            case STATE_METHOD:
                if (ch == ' ') {
                    state = STATE_PATH;
                    path_start = p + 1;
                    path_len = 0;
                } else {
                    method_len++;
                }
                break;

            case STATE_PATH:
                if (ch == ' ') {
                    state = STATE_VERSION;
                } else {
                    path_len++;
                }
                break;

            case STATE_VERSION:
                /* VERSION 状态：继续处理直到字符串结束 */
                break;

            default:
                break;
        }
        p++;
    }

    /* 字符串结束后，根据最后一个状态判断结果 */
    if (state == STATE_VERSION && method_start && path_start) {
        printf("  解析成功:\n");
        printf("    方法: %.*s\n", (int)method_len, method_start);
        printf("    路径: %.*s\n", (int)path_len, path_start);
        printf("    版本: HTTP/1.1\n");
    } else {
        printf("  解析失败，状态: %d\n", state);
    }
}

/* ==================== goto 错误处理演示 ==================== */

/**
 * 文件处理与资源清理
 * 演示 goto 在错误处理中的合法使用
 */
static void demo_goto_error_handling(void)
{
    printf("\n[control_flow] goto 错误处理演示\n");

    int *ptr1 = NULL;
    int *ptr2 = NULL;
    int *ptr3 = NULL;
    FILE *fp = NULL;
    int result = -1;

    printf("  模拟多资源分配与清理:\n");

    /* 分配资源 1 */
    ptr1 = (int *)malloc(sizeof(int) * 10);
    if (!ptr1) {
        printf("  [错误] 分配 ptr1 失败\n");
        goto cleanup;
    }
    printf("  [成功] 分配 ptr1 (40 bytes)\n");

    /* 分配资源 2 */
    ptr2 = (int *)malloc(sizeof(int) * 20);
    if (!ptr2) {
        printf("  [错误] 分配 ptr2 失败\n");
        goto cleanup;
    }
    printf("  [成功] 分配 ptr2 (80 bytes)\n");

    /* 分配资源 3 */
    ptr3 = (int *)malloc(sizeof(int) * 30);
    if (!ptr3) {
        printf("  [错误] 分配 ptr3 失败\n");
        goto cleanup;
    }
    printf("  [成功] 分配 ptr3 (120 bytes)\n");

    /* 打开文件 */
    fp = fopen("nonexistent.txt", "r");
    if (!fp) {
        printf("  [错误] 打开文件失败\n");
        goto cleanup;
    }
    printf("  [成功] 打开文件\n");

    /* 正常流程处理... */
    printf("  执行正常业务流程...\n");
    result = 0;

cleanup:
    printf("  [清理] 释放所有资源:\n");
    if (fp) {
        printf("    关闭文件\n");
        fclose(fp);
    }
    if (ptr3) {
        printf("    释放 ptr3 (120 bytes)\n");
        free(ptr3);
    }
    if (ptr2) {
        printf("    释放 ptr2 (80 bytes)\n");
        free(ptr2);
    }
    if (ptr1) {
        printf("    释放 ptr1 (40 bytes)\n");
        free(ptr1);
    }
    printf("  最终结果: %s\n", (result == 0) ? "成功" : "失败");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 控制流 ===\n");

    /* 条件语句 */
    demo_if_else();
    demo_switch();

    /* 循环结构 */
    demo_for_loop();
    demo_while_loop();
    demo_do_while();

    /* 状态机 */
    demo_state_machine();

    /* goto 错误处理 */
    demo_goto_error_handling();

    printf("\n=== PASS ===\n");
    return 0;
}
