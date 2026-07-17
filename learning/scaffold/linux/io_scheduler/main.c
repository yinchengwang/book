/**
 * @file main.c
 * @brief Linux I/O 调度器演示
 *
 * 演示内容：
 *   - /sys/block/*/queue/scheduler 读取调度器
 *   - CFQ/Deadline/NOOP/Kyber 各调度策略
 *   - I/O 调度对数据库性能的影响
 *
 * 编译：gcc -std=c11 -Wall -o io_scheduler main.c
 * 运行：./io_scheduler
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

/* ============================================================
 * Demo 1: 检查 I/O 调度器
 * ============================================================ */
static void demo_check_scheduler(void) {
    printf("[io_sched] 检查块设备 I/O 调度器\n");

    DIR *dir = opendir("/sys/block");
    if (!dir) {
        perror("opendir /sys/block");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char sched_path[256];
        snprintf(sched_path, sizeof(sched_path),
                 "/sys/block/%s/queue/scheduler", entry->d_name);

        FILE *fp = fopen(sched_path, "r");
        if (!fp) continue;

        char line[256] = {0};
        if (fgets(line, sizeof(line), fp)) {
            printf("[io_sched]   %s: %s", entry->d_name, line);
        }
        fclose(fp);
    }
    closedir(dir);
}

/* ============================================================
 * Demo 2: 调度器类型讲解
 * ============================================================ */
static void demo_scheduler_types(void) {
    printf("[io_sched] I/O 调度器类型:\n\n");

    printf("[io_sched] 1. CFQ (完全公平排队):\n");
    printf("[io_sched]    - 每个进程一个 I/O 队列\n");
    printf("[io_sched]    - 按时间片轮转，保证公平性\n");
    printf("[io_sched]    - 适合: 多用户桌面/服务器\n");
    printf("[io_sched]    - 已被 BFQ/mq-deadline 取代\n\n");

    printf("[io_sched] 2. Deadline (截止时间):\n");
    printf("[io_sched]    - 读 500ms / 写 5s 截止时间\n");
    printf("[io_sched]    - 读请求优先，防止饥饿\n");
    printf("[io_sched]    - 适合: 数据库/DMBS\n");
    printf("[io_sched]    - 单队列→mq-deadline（多队列）\n\n");

    printf("[io_sched] 3. NOOP (空操作):\n");
    printf("[io_sched]    - 仅合并请求，不重排序\n");
    printf("[io_sched]    - 适合: SSD/NVMe（无寻道）\n\n");

    printf("[io_sched] 4. Kyber:\n");
    printf("[io_sched]    - 基于延迟调节调度\n");
    printf("[io_sched]    - 动态计算阈值，自适应\n");
    printf("[io_sched]    - 适合: 混合负载（读+写）\n\n");

    printf("[io_sched] 5. BFQ (预算公平排队):\n");
    printf("[io_sched]    - 按预算分配，支持 cgroup\n");
    printf("[io_sched]    - 低延迟保证\n");
    printf("[io_sched]    - 适合: 低延迟应用\n");
}

/* ============================================================
 * Demo 3: 调度器选择指南
 * ============================================================ */
static void demo_selection_guide(void) {
    printf("[io_sched] 调度器选择指南:\n\n");

    printf("[io_sched] HDD (机械硬盘): Deadline / BFQ\n");
    printf("[io_sched]   - 需要寻道优化和请求合并\n");
    printf("[io_sched]   - BFQ 提供更好的低延迟保证\n\n");

    printf("[io_sched] SSD (固态): mq-deadline / none\n");
    printf("[io_sched]   - 无寻道时间，重排序无意义\n");
    printf("[io_sched]   - 合并请求 + 直发给设备\n\n");

    printf("[io_sched] NVMe: none (noop)\n");
    printf("[io_sched]   - 设备自行调度（固件）\n");
    printf("[io_sched]   - 内核调度器反而增加延迟\n\n");

    printf("[io_sched] 修改调度器:\n");
    printf("[io_sched]   echo mq-deadline > /sys/block/sda/queue/scheduler\n");
}

/* ============================================================
 * Demo 4: 数据库 I/O 调度优化
 * ============================================================ */
static void demo_db_io_optimization(void) {
    printf("[io_sched] 数据库 I/O 调度优化:\n\n");

    printf("[io_sched] MySQL/InnoDB:\n");
    printf("[io_sched]   innodb_use_native_aio = 1\n");
    printf("[io_sched]   # 使用 Linux AIO 跳过调度器\n\n");

    printf("[io_sched] PostgreSQL:\n");
    printf("[io_sched]   effective_io_concurrency = 2\n");
    printf("[io_sched]   # 并发 I/O 请求数量\n\n");

    printf("[io_sched] 优化建议:\n");
    printf("[io_sched]   1. 数据目录用 mq-deadline / none\n");
    printf("[io_sched]   2. 日志目录用 Deadline（保证持久化）\n");
    printf("[io_sched]   3. 避免同一块盘放数据和日志\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux I/O 调度器演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 检查 I/O 调度器 ---\n");
    demo_check_scheduler();
    printf("\n");

    printf("--- Demo 2: 调度器类型 ---\n");
    demo_scheduler_types();
    printf("\n");

    printf("--- Demo 3: 调度器选择指南 ---\n");
    demo_selection_guide();
    printf("\n");

    printf("--- Demo 4: 数据库优化 ---\n");
    demo_db_io_optimization();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
