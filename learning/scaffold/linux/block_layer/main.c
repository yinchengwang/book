/**
 * @file main.c
 * @brief Linux 块设备层学习演示
 *
 * 演示内容：
 *   - 块设备抽象层（bio、request、request_queue）
 *   - blk-mq 多队列架构
 *   - 请求合并（merge）与 IO 调度算法概念
 *   - 通过 /sys/block/ 读取块设备信息
 *
 * 编译：gcc -std=c11 -Wall -o block_layer main.c
 * 运行：./block_layer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

/* ============================================================
 * 辅助：统计目录下的条目数
 * ============================================================ */
static int count_dir_entries(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return -1;

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') count++;
    }
    closedir(dir);
    return count;
}

/* ============================================================
 * 辅助：读取 sysfs 文件内容（单行数值）
 * ============================================================ */
static long read_sysfs_long(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    long val = -1;
    fscanf(fp, "%ld", &val);
    fclose(fp);
    return val;
}

/* ============================================================
 * 辅助：读取 sysfs 文件内容（字符串）
 * ============================================================ */
static void read_sysfs_str(const char *path, char *buf, size_t size) {
    FILE *fp = fopen(path, "r");
    if (!fp) { buf[0] = '\0'; return; }
    if (!fgets(buf, (int)size, fp)) buf[0] = '\0';
    fclose(fp);
    /* 去除末尾换行 */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
}

/* ============================================================
 * Demo 1: 块设备抽象层基本概念
 * ============================================================ */
static void demo_block_layer_concepts(void) {
    printf("[block_layer] === 块设备抽象层基本概念 ===\n\n");

    printf("[block_layer] 1. bio（Block I/O）\n");
    printf("[block_layer]    - 内核中最小的 I/O 请求单元\n");
    printf("[block_layer]    - 包含: 目标设备、扇区偏移、数据缓冲区、读写方向\n");
    printf("[block_layer]    - struct bio 定义在 <linux/bio.h>\n\n");

    printf("[block_layer] 2. request\n");
    printf("[block_layer]    - 多个 bio 合并成一个 request\n");
    printf("[block_layer]    - 合并策略: 相邻扇区合并、后向合并、前向合并\n");
    printf("[block_layer]    - struct request 定义在 <linux/blkdev.h>\n\n");

    printf("[block_layer] 3. request_queue\n");
    printf("[block_layer]    - 每个块设备有一个请求队列\n");
    printf("[block_layer]    - 负责: 排队、合并、调度、分发到驱动\n");
    printf("[block_layer]    - struct request_queue 定义在 <linux/blkdev.h>\n\n");

    printf("[block_layer] 块设备 I/O 栈:\n");
    printf("[block_layer]    用户空间 (read/write)\n");
    printf("[block_layer]       |\n");
    printf("[block_layer]    VFS / 页缓存\n");
    printf("[block_layer]       |\n");
    printf("[block_layer]    文件系统 (ext4/xfs)\n");
    printf("[block_layer]       |\n");
    printf("[block_layer]    bio 层 (submit_bio)\n");
    printf("[block_layer]       |\n");
    printf("[block_layer]    request_queue + IO 调度器\n");
    printf("[block_layer]       |\n");
    printf("[block_layer]    块设备驱动 (NVMe/SCSI)\n");
}

/* ============================================================
 * Demo 2: blk-mq 多队列架构
 * ============================================================ */
static void demo_blk_mq(void) {
    printf("[block_layer] === blk-mq 多队列架构 ===\n\n");

    printf("[block_layer] blk-mq (Multi-Queue Block Layer):\n");
    printf("[block_layer]    - Linux 3.13+ 引入，取代单队列架构\n");
    printf("[block_layer]    - 解决多核 CPU 下锁竞争问题\n\n");

    printf("[block_layer] 两级队列模型:\n");
    printf("[block_layer]    第一级: 软件暂存队列（Per-CPU）\n");
    printf("[block_layer]       - 每个 CPU 核有独立的提交队列\n");
    printf("[block_layer]       - 无锁插入，减少 CPU 间竞争\n");
    printf("[block_layer]    第二级: 硬件调度队列（Per-HW-Queue）\n");
    printf("[block_layer]       - 直接映射到设备硬件队列\n");
    printf("[block_layer]       - NVMe 设备可支持 64K 个队列\n\n");

    printf("[block_layer] 优势:\n");
    printf("[block_layer]    - 更好的 NUMA 亲和性\n");
    printf("[block_layer]    - 更高的 IOPS（百万级）\n");
    printf("[block_layer]    - 更低的延迟\n");

    /* 检查 sysfs 中的 mq 信息 */
    printf("\n[block_layer] 检查系统块设备 mq 信息:\n");
    DIR *dir = opendir("/sys/block");
    if (dir) {
        struct dirent *entry;
        int shown = 0;
        while ((entry = readdir(dir)) != NULL && shown < 3) {
            if (entry->d_name[0] == '.') continue;

            char mq_path[256];
            snprintf(mq_path, sizeof(mq_path), "/sys/block/%s/mq", entry->d_name);
            int cpu_count = count_dir_entries(mq_path);
            if (cpu_count > 0) {
                printf("[block_layer]   设备 %s: blk-mq 已启用, CPU 队列数=%d\n",
                       entry->d_name, cpu_count);
                shown++;
            }
        }
        if (shown == 0) {
            printf("[block_layer]   (未检测到 blk-mq 设备信息，可能需要 root)\n");
        }
        closedir(dir);
    }
}

/* ============================================================
 * Demo 3: IO 调度算法
 * ============================================================ */
static void demo_io_schedulers(void) {
    printf("[block_layer] === IO 调度算法 ===\n\n");

    printf("[block_layer] 1. noop / none（无调度）\n");
    printf("[block_layer]    - 最简单的 FIFO 队列\n");
    printf("[block_layer]    - 不做任何重排序或合并\n");
    printf("[block_layer]    - 适用: SSD、NVMe、虚拟机\n\n");

    printf("[block_layer] 2. mq-deadline（多队列截止时间）\n");
    printf("[block_layer]    - 保证每个请求的延迟上限\n");
    printf("[block_layer]    - 读优先（默认 500ms）、写（默认 5000ms）\n");
    printf("[block_layer]    - 按 LBA 排序以优化寻道\n");
    printf("[block_layer]    - 适用: HDD、混合负载\n\n");

    printf("[block_layer] 3. kyber（令牌桶调度）\n");
    printf("[block_layer]    - 基于令牌桶的速率限制\n");
    printf("[block_layer]    - 动态调整读写比例\n");
    printf("[block_layer]    - 适用: 低延迟 SSD\n\n");

    printf("[block_layer] 4. bfq（Budget Fair Queueing）\n");
    printf("[block_layer]    - 为每个进程分配 I/O 预算\n");
    printf("[block_layer]    - 保证公平性和低延迟\n");
    printf("[block_layer]    - 适用: 桌面/交互式应用\n");

    /* 读取当前设备的调度器 */
    printf("\n[block_layer] 检查当前调度器配置:\n");
    DIR *dir = opendir("/sys/block");
    if (dir) {
        struct dirent *entry;
        int shown = 0;
        while ((entry = readdir(dir)) != NULL && shown < 2) {
            if (entry->d_name[0] == '.') continue;

            char sched_path[256];
            snprintf(sched_path, sizeof(sched_path),
                     "/sys/block/%s/queue/scheduler", entry->d_name);
            char sched[256] = {0};
            read_sysfs_str(sched_path, sched, sizeof(sched));
            if (sched[0]) {
                printf("[block_layer]   设备 %s 调度器: %s", entry->d_name, sched);
                shown++;
            }
        }
        if (shown == 0) {
            printf("[block_layer]   (无法读取调度器信息，可能需要 root)\n");
        }
        closedir(dir);
    }
}

/* ============================================================
 * Demo 4: 请求合并机制
 * ============================================================ */
static void demo_request_merge(void) {
    printf("[block_layer] === 请求合并机制 ===\n\n");

    printf("[block_layer] 请求合并是将相邻的 bio 合并成一个 request:\n\n");

    printf("[block_layer] 1. 前向合并（Front Merge）:\n");
    printf("[block_layer]    - 新 bio 的结束扇区 == 现有 request 的起始扇区\n");
    printf("[block_layer]    - bio 插入到 request 前面\n\n");

    printf("[block_layer] 2. 后向合并（Back Merge）:\n");
    printf("[block_layer]    - 新 bio 的起始扇区 == 现有 request 的结束扇区\n");
    printf("[block_layer]    - bio 追加到 request 后面\n");
    printf("[block_layer]    - 最常见、最高效的合并方式\n\n");

    printf("[block_layer] 3. 合并的好处:\n");
    printf("[block_layer]    - 减少 request 数量\n");
    printf("[block_layer]    - 减少中断次数\n");
    printf("[block_layer]    - 提高吞吐量（尤其是顺序 I/O）\n\n");

    printf("[block_layer] 合并统计查看:\n");
    printf("[block_layer]    cat /sys/block/sda/stat\n");
    printf("[block_layer]    字段: 读IO 读合并 读扇区 读时间 写IO 写合并 ...\n");
    printf("[block_layer]    其中 \"读合并\" \"写合并\" 就是合并次数\n");
}

/* ============================================================
 * Demo 5: 读取块设备信息
 * ============================================================ */
static void demo_read_block_info(void) {
    printf("[block_layer] === 读取块设备信息 ===\n\n");

    DIR *dir = opendir("/sys/block");
    if (!dir) {
        printf("[block_layer] 无法打开 /sys/block\n");
        return;
    }

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL && count < 5) {
        if (entry->d_name[0] == '.') continue;

        char path[256];

        /* 设备大小（扇区数） */
        snprintf(path, sizeof(path), "/sys/block/%s/size", entry->d_name);
        long sectors = read_sysfs_long(path);
        double size_gb = (sectors > 0) ? (sectors * 512.0) / (1024.0 * 1024.0 * 1024.0) : 0;

        /* 是否为可移动设备 */
        snprintf(path, sizeof(path), "/sys/block/%s/removable", entry->d_name);
        long removable = read_sysfs_long(path);

        /* 是否为只读 */
        snprintf(path, sizeof(path), "/sys/block/%s/ro", entry->d_name);
        long readonly = read_sysfs_long(path);

        /* 队列深度 */
        snprintf(path, sizeof(path), "/sys/block/%s/queue/nr_requests", entry->d_name);
        long nr_requests = read_sysfs_long(path);

        printf("[block_layer] 设备: /dev/%s\n", entry->d_name);
        printf("[block_layer]   大小=%.2f GB (%ld 扇区)\n", size_gb, sectors);
        printf("[block_layer]   可移动=%ld  只读=%ld  队列深度=%ld\n",
               removable, readonly, nr_requests);
        printf("\n");
        count++;
    }
    closedir(dir);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 块设备层学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 块设备抽象层概念 ---\n");
    demo_block_layer_concepts();
    printf("\n");

    printf("--- Demo 2: blk-mq 多队列架构 ---\n");
    demo_blk_mq();
    printf("\n");

    printf("--- Demo 3: IO 调度算法 ---\n");
    demo_io_schedulers();
    printf("\n");

    printf("--- Demo 4: 请求合并机制 ---\n");
    demo_request_merge();
    printf("\n");

    printf("--- Demo 5: 读取块设备信息 ---\n");
    demo_read_block_info();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
