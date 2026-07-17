/**
 * @file main.c
 * @brief Linux 磁盘 I/O 诊断学习演示
 *
 * 演示内容：
 *   - 读取 /proc/diskstats 获取磁盘统计信息
 *   - 计算 iostat 关键指标（IOPS、吞吐量、延迟）
 *   - 模拟 I/O 操作并观察统计变化
 *
 * 编译：gcc -std=c11 -Wall -o disk_iostat main.c
 * 运行：./disk_iostat
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

/* ============================================================
 * 数据结构：磁盘统计信息
 * ============================================================ */
typedef struct {
    char name[32];          /* 设备名，如 sda, nvme0n1 */
    unsigned long reads;    /* 读取完成次数 */
    unsigned long reads_merged;
    unsigned long read_sectors;  /* 读取的扇区数 */
    unsigned long read_time_ms;  /* 读取花费的毫秒数 */
    unsigned long writes;   /* 写入完成次数 */
    unsigned long writes_merged;
    unsigned long write_sectors;
    unsigned long write_time_ms;
    unsigned long io_in_progress;  /* 当前进行中的 I/O 数 */
    unsigned long io_time_ms;      /* I/O 服务总时间 */
    unsigned long io_weight_time_ms;
} disk_stat_t;

/* ============================================================
 * 获取当前时间（毫秒）
 * ============================================================ */
static unsigned long get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* ============================================================
 * 读取 /proc/diskstats 获取磁盘统计
 * ============================================================ */
static int read_disk_stats(const char *target_dev, disk_stat_t *stat) {
    FILE *fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        perror("[disk_iostat] fopen /proc/diskstats 失败");
        return -1;
    }

    char line[256];
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        unsigned int major, minor;
        char name[32];

        /* 解析格式：major minor name reads ... */
        if (sscanf(line, "%u %u %31s", &major, &minor, name) == 3) {
            /* 检查是否是目标设备（支持 sda 或 sda1 分区） */
            if (strcmp(name, target_dev) == 0 ||
                strncmp(name, target_dev, strlen(target_dev)) == 0) {
                sscanf(line, "%u %u %s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                       &major, &minor, name,
                       &stat->reads, &stat->reads_merged,
                       &stat->read_sectors, &stat->read_time_ms,
                       &stat->writes, &stat->writes_merged,
                       &stat->write_sectors, &stat->write_time_ms,
                       &stat->io_in_progress,
                       &stat->io_time_ms, &stat->io_weight_time_ms);

                strncpy(stat->name, name, sizeof(stat->name) - 1);
                found = 1;
                break;
            }
        }
    }

    fclose(fp);
    return found ? 0 : -1;
}

/* ============================================================
 * 计算并打印 I/O 指标
 * ============================================================ */
static void print_io_stats(const disk_stat_t *start, const disk_stat_t *end,
                           unsigned long time_ms) {
    unsigned long reads_diff = end->reads - start->reads;
    unsigned long writes_diff = end->writes - start->writes;
    unsigned long read_sectors_diff = end->read_sectors - start->read_sectors;
    unsigned long write_sectors_diff = end->write_sectors - start->write_sectors;
    unsigned long read_time_diff = end->read_time_ms - start->read_time_ms;
    unsigned long write_time_diff = end->write_time_ms - start->write_time_ms;

    double t = time_ms / 1000.0;  /* 转换为秒 */

    printf("[disk_iostat] I/O 统计 (间隔 %.2f 秒)\n", t);
    printf("[disk_iostat]   设备: %s\n", start->name);
    printf("[disk_iostat]   ----- 读取 -----\n");
    printf("[disk_iostat]     IOPS: %.2f reads/s\n", reads_diff / t);
    printf("[disk_iostat]     吞吐量: %.2f MB/s\n",
           (read_sectors_diff * 512.0) / (1024 * 1024) / t);
    printf("[disk_iostat]     平均延迟: %.2f ms\n",
           reads_diff > 0 ? (double)read_time_diff / reads_diff : 0);

    printf("[disk_iostat]   ----- 写入 -----\n");
    printf("[disk_iostat]     IOPS: %.2f writes/s\n", writes_diff / t);
    printf("[disk_iostat]     吞吐量: %.2f MB/s\n",
           (write_sectors_diff * 512.0) / (1024 * 1024) / t);
    printf("[disk_iostat]     平均延迟: %.2f ms\n",
           writes_diff > 0 ? (double)write_time_diff / writes_diff : 0);

    unsigned long total_io = reads_diff + writes_diff;
    unsigned long total_sectors = read_sectors_diff + write_sectors_diff;
    printf("[disk_iostat]   ----- 合计 -----\n");
    printf("[disk_iostat]     总 IOPS: %.2f\n", total_io / t);
    printf("[disk_iostat]     总吞吐量: %.2f MB/s\n",
           (total_sectors * 512.0) / (1024 * 1024) / t);
}

/* ============================================================
 * Demo 1: 读取系统磁盘统计
 * ============================================================ */
static void demo_read_diskstats(void) {
    printf("[disk_iostat] 读取 /proc/diskstats\n");

    FILE *fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        perror("[disk_iostat] fopen 失败");
        return;
    }

    char line[256];
    int count = 0;
    printf("[disk_iostat]   可用磁盘设备:\n");

    while (fgets(line, sizeof(line), fp) && count < 10) {
        unsigned int major, minor;
        char name[32];
        unsigned long reads, writes;

        if (sscanf(line, "%u %u %31s %lu %*u %*u %*u %lu",
                   &major, &minor, name, &reads, &writes) == 5) {
            /* 过滤掉分区，只显示完整设备 */
            if (strchr(name + 1, digittoint(name[strlen(name)-1] ? name[strlen(name)-1] : 0)) == NULL ||
                strlen(name) <= 4) {  /* 简单过滤 */
                printf("[disk_iostat]     %s (major=%u, minor=%u, reads=%lu, writes=%lu)\n",
                       name, major, minor, reads, writes);
                count++;
            }
        }
    }
    fclose(fp);
}

/* ============================================================
 * Demo 2: 实时 I/O 监控
 * ============================================================ */
static void demo_realtime_stats(void) {
    printf("[disk_iostat] 演示实时 I/O 监控\n");

    /* 查找第一个可用设备 */
    const char *dev = "sda";
    FILE *fp = fopen("/proc/diskstats", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char name[32];
            if (sscanf(line, "%*u %*u %31s", name) == 1) {
                /* 跳过分区（如 sda1, sda2） */
                if (strlen(name) <= 4) {
                    dev = name;
                    break;
                }
            }
        }
        fclose(fp);
    }

    disk_stat_t start, end;

    printf("[disk_iostat]   监控设备: %s\n", dev);

    /* 第一次采样 */
    if (read_disk_stats(dev, &start) != 0) {
        printf("[disk_iostat]   无法读取设备 %s\n", dev);
        return;
    }

    /* 模拟一些 I/O 操作 */
    printf("[disk_iostat]   模拟顺序写入...\n");
    int fd = open("/tmp/iostat_test.dat", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        char buf[4096];
        memset(buf, 'X', sizeof(buf));
        for (int i = 0; i < 100; i++) {
            write(fd, buf, sizeof(buf));
        }
        fsync(fd);
        close(fd);
    }

    /* 等待一小段时间 */
    usleep(100000);  /* 100ms */

    /* 第二次采样 */
    if (read_disk_stats(dev, &end) == 0) {
        print_io_stats(&start, &end, 200);  /* 假设间隔 200ms */
    }

    /* 清理测试文件 */
    unlink("/tmp/iostat_test.dat");
}

/* ============================================================
 * Demo 3: 计算 iostat 等效指标
 * ============================================================ */
static void demo_iostat_metrics(void) {
    printf("[disk_iostat] 计算 iostat 等效指标\n");

    /* 模拟 iostat 输出格式 */
    printf("[disk_iostat]   Device            tps    kB_read/s    kB_writtn/s\n");

    /* 读取所有磁盘统计 */
    FILE *fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char name[32];
        unsigned long reads, writes, read_sectors, write_sectors;

        if (sscanf(line, "%*u %*u %31s %lu %*u %lu %*u %lu %*u %lu",
                   name, &reads, &read_sectors, &writes, &write_sectors) == 5) {
            /* 只显示主设备 */
            if (strlen(name) <= 4) {
                double tps = reads + writes;
                double kb_read = read_sectors * 512.0 / 1024.0;
                double kb_write = write_sectors * 512.0 / 1024.0;

                printf("[disk_iostat]   %-16s %8.2f %12.2f %14.2f\n",
                       name, tps, kb_read, kb_write);
            }
        }
    }
    fclose(fp);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 磁盘 I/O 诊断学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 读取 /proc/diskstats ---\n");
    demo_read_diskstats();
    printf("\n");

    printf("--- Demo 2: 实时 I/O 监控 ---\n");
    demo_realtime_stats();
    printf("\n");

    printf("--- Demo 3: iostat 指标计算 ---\n");
    demo_iostat_metrics();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
