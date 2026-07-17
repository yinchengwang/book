/**
 * @file main.c
 * @brief perf 性能分析工具基础演示
 *
 * 演示 perf stat/record/report 基本用法
 * 展示 CPU 性能事件采样和热点分析
 *
 * 编译：gcc -std=c11 -o perf_basic main.c
 * 运行：./perf_basic
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* 执行 shell 命令并打印输出 */
static int run_command(const char *cmd) {
    printf("\n--- 执行: %s ---\n", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        printf("命令执行返回: %d\n", ret);
    }
    return ret;
}

/* 演示 CPU 密集型计算（用于 perf 采样） */
static void cpu_intensive_work(void) {
    printf("\n[演示] 执行 CPU 密集型计算...\n");
    volatile double sum = 0.0;
    for (int i = 0; i < 100000000; i++) {
        sum += (double)i * 0.0000001;
    }
    printf("计算完成: sum = %f\n", sum);
}

/* 演示内存密集型访问（用于缓存 miss 分析） */
static void memory_intensive_work(void) {
    printf("\n[演示] 执行内存密集型访问...\n");
    const int size = 1024 * 1024;
    int *arr = malloc(size * sizeof(int));
    if (!arr) {
        perror("malloc");
        return;
    }

    /* 顺序访问 */
    for (int i = 0; i < size; i++) {
        arr[i] = i;
    }

    /* 随机访问（触发更多缓存 miss） */
    for (int i = 0; i < size; i += 100) {
        volatile int val = arr[i];
        (void)val;
    }

    printf("内存访问完成，处理了 %d 字节\n", size * (int)sizeof(int));
    free(arr);
}

/* 检查 perf 是否可用 */
static int check_perf_available(void) {
    FILE *fp = popen("perf --version 2>/dev/null", "r");
    if (!fp) {
        return 0;
    }
    char buf[128];
    if (fgets(buf, sizeof(buf), fp)) {
        printf("perf 版本: %s", buf);
        pclose(fp);
        return 1;
    }
    pclose(fp);
    return 0;
}

int main(void) {
    printf("===========================================\n");
    printf("  perf 性能分析工具基础演示\n");
    printf("===========================================\n");

    /* 检查 perf 可用性 */
    if (!check_perf_available()) {
        printf("\n[警告] perf 工具不可用，可能需要:\n");
        printf("  1. 安装: sudo apt install linux-tools-common\n");
        printf("  2. 加载: sudo sysctl kernel.perf_event_paranoid=-1\n");
        printf("\n继续运行，演示代码分析部分...\n");
    }

    /* 1. perf stat：统计事件 */
    printf("\n\n=== 1. perf stat 统计模式 ===");
    printf("\n[说明] perf stat 用于统计程序执行的硬件/软件事件\n");

    pid_t pid = fork();
    if (pid == 0) {
        /* 子进程执行工作负载 */
        cpu_intensive_work();
        exit(0);
    } else {
        /* 父进程用 perf stat 统计 */
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "perf stat -e cycles,instructions,cache-references,cache-misses "
                 "-p %d 2>&1 | head -30", pid);
        printf("\n在另一个终端运行:\n  %s\n", cmd);
        wait(NULL);
    }

    /* 2. perf record：采样记录 */
    printf("\n\n=== 2. perf record 采样模式 ===");
    printf("\n[说明] perf record 对程序进行采样，生成 perf.data\n");
    printf("\n[演示] 生成 CPU 密集型工作负载...\n");
    cpu_intensive_work();

    printf("\n在另一个终端运行:\n");
    printf("  perf record -g -o perf.data ./perf_basic\n");
    printf("  perf report --stdio -i perf.data\n");

    /* 3. perf report：分析报告 */
    printf("\n\n=== 3. perf report 分析模式 ===");
    printf("\n[说明] perf report 解析 perf.data 并显示热点函数\n");
    printf("\n[示例输出格式]\n");
    printf("  # Overhead  Command          Shared Object        Symbol\n");
    printf("  # ........  ...............  ...................  ...................\n");
    printf("      45.23%%  perf_basic       perf_basic           [.] cpu_intensive_work\n");
    printf("      30.12%%  perf_basic       perf_basic           [.] memory_intensive_work\n");
    printf("      15.45%%  libc-2.31.so     libc.so.6            [.] __GI___strcmp_ssse3\n");

    /* 4. 热点分析实践 */
    printf("\n\n=== 4. 热点分析实践 ===\n");

    printf("\n4.1 CPU 密集型任务:");
    cpu_intensive_work();

    printf("\n4.2 内存密集型任务:");
    memory_intensive_work();

    /* 5. 常用 perf 命令速查 */
    printf("\n\n=== 5. 常用 perf 命令速查 ===\n");
    printf("  # 统计程序全局事件\n");
    printf("  perf stat ./program\n");
    printf("\n  # 采样 CPU 栈（需要 -g）\n");
    printf("  perf record -g ./program\n");
    printf("\n  # 分析特定事件\n");
    printf("  perf stat -e cache-misses,L1-dcache-load-misses ./program\n");
    printf("\n  # 分析正在运行的进程\n");
    printf("  perf stat -p <pid>\n");
    printf("\n  # 分析系统级事件（需要 root）\n");
    printf("  sudo perf stat -a -e cycles sleep 5\n");

    printf("\n===========================================\n");
    printf("  perf 基础演示完成\n");
    printf("===========================================\n");

    return 0;
}
