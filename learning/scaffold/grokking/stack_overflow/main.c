/**
 * 堆栈溢出 / 尾递归 演示
 *
 * 演示:
 * 1. 普通递归导致栈溢出
 * 2. 尾递归优化 (需 -O2 编译)
 * 3. 栈保护 (canary) 效果
 * 4. 变长数组 (VLA) 导致的栈溢出
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>

/* 用于捕获 SIGSEGV 进行演示 */
static sigjmp_buf g_jmp_buf;
static volatile int g_canary_hit = 0;

static void sigsegv_handler(int sig)
{
    (void)sig;
    g_canary_hit = 1;
    siglongjmp(g_jmp_buf, 1);
}

/* ==========================================================
 * 1. 普通递归 —— 深度过大导致栈溢出
 * ========================================================== */
static int normal_recursion(int n)
{
    if (n <= 0)
        return 0;
    /* 分配 256 字节局部变量加速栈帧耗尽 */
    char buf[256];
    memset(buf, 0, sizeof(buf));
    buf[0] = (char)(n & 0xFF);
    return normal_recursion(n - 1) + buf[0];
}

/* ==========================================================
 * 2. 尾递归 —— 编译器可优化为循环
 *    gcc -O2 会将此函数转换为迭代
 * ========================================================== */
static int tail_recursion_impl(int n, int acc)
{
    if (n <= 0)
        return acc;
    return tail_recursion_impl(n - 1, acc + n);
}

static int tail_recursion(int n)
{
    return tail_recursion_impl(n, 0);
}

/* ==========================================================
 * 3. 手动模拟尾递归 —— 不依赖编译器
 * ========================================================== */
static int manual_tail_recursion(int n)
{
    int acc = 0;
    while (n > 0) {
        acc += n;
        n--;
    }
    return acc;
}

/* ==========================================================
 * 4. 栈缓冲区溢出演示
 * ========================================================== */
static void vulnerable_function(const char *input)
{
    char buffer[16];
    /* 不安全: 不检查长度 */
    strcpy(buffer, input);
    printf("buffer = %s\n", buffer);
}

static void safe_function(const char *input)
{
    char buffer[16];
    size_t len = strlen(input);
    if (len >= sizeof(buffer)) {
        fprintf(stderr, "[安全] 输入过长 (%zu 字节), 拒绝复制\n", len);
        return;
    }
    strcpy(buffer, input);
    printf("buffer = %s\n", buffer);
}

/* ==========================================================
 * 5. 变长数组 (VLA) 栈溢出测试
 * ========================================================== */
static void vla_stack_test(size_t size)
{
    printf("  尝试在栈上分配 VLA: %zu 字节\n", size);
    char vla[size];
    memset(vla, 0, size);
    vla[0] = 'A';
    vla[size - 1] = 'Z';
    printf("  VLA 分配成功 (首=%c, 尾=%c)\n", vla[0], vla[size - 1]);
}

/* ==========================================================
 * 6. 栈帧分析: 打印栈上地址
 * ========================================================== */
static void stack_frame_analysis(int depth)
{
    int local_var = depth;
    printf("  深度 %2d: 局部变量地址 %p, 函数地址 %p\n",
           depth, (void *)&local_var,
           (void *)(uintptr_t)stack_frame_analysis);

    if (depth > 0) {
        stack_frame_analysis(depth - 1);
    }
}

/* ==========================================================
 * 7. 编译器优化检查 —— 确认尾递归是否真的被优化
 * ========================================================== */
static int check_optimization(void)
{
    /* 用一个大值测试尾递归是否会导致栈溢出 */
    int result = tail_recursion(100000);
    return result;
}

/* ==========================================================
 * 8. 模拟 canary (栈保护) 检测
 * ========================================================== */
static int canary_test(void)
{
    volatile uint64_t canary = 0xDEADBEEFCAFEBABEULL;
    char buffer[32];
    int result = 0;

    printf("  canary 值 = 0x%016llX\n", (unsigned long long)canary);
    printf("  buffer 地址 = %p, canary 地址 = %p\n",
           (void *)buffer, (void *)&canary);

    /* 模拟缓冲区溢出覆盖 canary */
    memset(buffer, 'A', sizeof(buffer) + 8);

    if (canary != 0xDEADBEEFCAFEBABEULL) {
        printf("  canary 被覆盖! 检测到栈溢出! (当前=0x%016llX)\n",
               (unsigned long long)canary);
        result = 1;
    } else {
        printf("  canary 完好\n");
    }

    return result;
}

/* ==========================================================
 * main
 * ========================================================== */
int main(void)
{
    printf("========================================\n");
    printf(" 堆栈溢出 / 尾递归 演示\n");
    printf("========================================\n\n");

    /* ---- 1. 栈帧分析 ---- */
    printf("[1] 栈帧分析 (递归深度 5):\n");
    stack_frame_analysis(5);
    printf("\n");

    /* ---- 2. 尾递归测试 ---- */
    printf("[2] 尾递归测试 (n=100000, 需 -O2 编译):\n");
    int tr = check_optimization();
    printf("  尾递归结果 = %d\n", tr);
    printf("  提示: 如果程序未崩溃, 说明尾递归优化生效\n\n");

    /* ---- 3. 手动尾递归 (迭代) ---- */
    printf("[3] 手动尾递归 (迭代模拟):\n");
    int mr = manual_tail_recursion(100);
    printf("  1+2+...+100 = %d\n\n", mr);

    /* ---- 4. 缓冲区溢出 ---- */
    printf("[4] 栈缓冲区溢出演示:\n");
    /* 安全输入 */
    safe_function("hello");
    /* 不安全输入, 使用信号处理捕获崩溃 */
    printf("  尝试不安全复制 (超长字符串)...\n");
    if (sigsetjmp(g_jmp_buf, 1) == 0) {
        signal(SIGSEGV, sigsegv_handler);
        /* 用足够长的字符串尝试触发溢出 */
        char huge[256];
        memset(huge, 'X', sizeof(huge) - 1);
        huge[sizeof(huge) - 1] = '\0';
        vulnerable_function(huge);
        printf("  未崩溃 (编译器/OS 有栈保护)\n");
    } else {
        printf("  -> SIGSEGV 捕获! 栈溢出已发生.\n");
    }
    printf("\n");

    /* ---- 5. VLA 测试 ---- */
    printf("[5] 变长数组 (VLA) 栈分配测试:\n");
    vla_stack_test(1024);       /* 1KB - 应该成功 */
    printf("  尝试过大 VLA (16MB), 可能栈溢出...\n");
    if (sigsetjmp(g_jmp_buf, 1) == 0) {
        vla_stack_test(16 * 1024 * 1024);
        printf("  分配成功\n");
    } else {
        printf("  -> SIGSEGV 捕获! VLA 导致栈溢出.\n");
    }
    printf("\n");

    /* ---- 6. Canary 测试 ---- */
    printf("[6] Canary (栈保护) 检测:\n");
    int overflowed = canary_test();
    printf("  栈溢出检测结果: %s\n\n",
           overflowed ? "检测到溢出" : "未检测到溢出");

    /* ---- 7. 普通递归的栈限制验证 ---- */
    printf("[7] 普通递归栈限制测试:\n");
    printf("  尝试深度 10000 的普通递归...\n");
    if (sigsetjmp(g_jmp_buf, 1) == 0) {
        int r = normal_recursion(10000);
        printf("  完成, 结果 = %d (未栈溢出)\n", r);
    } else {
        printf("  -> SIGSEGV 捕获! 普通递归导致栈溢出.\n");
    }

    printf("\n========================================\n");
    printf(" 演示完毕\n");
    printf("========================================\n");
    return 0;
}
