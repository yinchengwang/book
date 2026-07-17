/**
 * @file main.c
 * @brief Linux SSD 优化学习演示
 *
 * 演示内容：
 *   - SSD 硬件特性（NAND 闪存结构、FTL 层）
 *   - TRIM / DISCARD 操作
 *   - 写入均衡（Wear Leveling）与 GC 垃圾回收
 *   - 队列深度对性能的影响
 *
 * 编译：gcc -std=c11 -Wall -o disk_ssd main.c
 * 运行：./disk_ssd
 *
 * 注意：TRIM/DISCARD 实际测试需要 root 权限和 SSD 设备。
 *       本程序演示概念并读取系统 SSD 相关信息。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

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
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
}

/* ============================================================
 * 辅助：模拟写入延迟（微秒）
 * ============================================================ */
static void simulate_write_delay(int us_delay) {
    usleep(us_delay);
}

/* ============================================================
 * Demo 1: SSD 硬件特性
 * ============================================================ */
static void demo_ssd_hardware(void) {
    printf("[disk_ssd] === SSD 硬件特性 ===\n\n");

    printf("[disk_ssd] 1. NAND 闪存结构:\n");
    printf("[disk_ssd]    Die（晶片）\n");
    printf("[disk_ssd]      └── Plane（面）\n");
    printf("[disk_ssd]            └── Block（块, 擦除单位, 通常 256KB~4MB）\n");
    printf("[disk_ssd]                  └── Page（页, 读写单位, 通常 4KB~16KB）\n");
    printf("[disk_ssd]                        └── Cell（单元, 存储 1~4 bit）\n\n");

    printf("[disk_ssd] 2. NAND 类型对比:\n");
    printf("[disk_ssd]    SLC (1 bit/cell): 最快、最耐久(~100K P/E)、最贵\n");
    printf("[disk_ssd]    MLC (2 bit/cell): 较快、较耐久(~10K P/E)、中等\n");
    printf("[disk_ssd]    TLC (3 bit/cell): 较慢、一般(~3K P/E)、主流消费级\n");
    printf("[disk_ssd]    QLC (4 bit/cell): 最慢、最低(~1K P/E)、大容量廉价\n\n");

    printf("[disk_ssd] 3. SSD 核心限制:\n");
    printf("[disk_ssd]    - 写入前必须擦除（Erase-Before-Write）\n");
    printf("[disk_ssd]    - 最小写入单位: Page（4KB~16KB）\n");
    printf("[disk_ssd]    - 最小擦除单位: Block（256KB~4MB，通常 64~256 页）\n");
    printf("[disk_ssd]    - 每个 Block 有 P/E（编程/擦除）寿命限制\n");
}

/* ============================================================
 * Demo 2: FTL 闪存转换层
 * ============================================================ */
static void demo_ftl(void) {
    printf("[disk_ssd] === FTL 闪存转换层 ===\n\n");

    printf("[disk_ssd] FTL (Flash Translation Layer) 核心功能:\n\n");

    printf("[disk_ssd] 1. 地址映射:\n");
    printf("[disk_ssd]    逻辑块地址 (LBA) → 物理块地址 (PBA)\n");
    printf("[disk_ssd]    映射表存储在 SSD 内部 DRAM 或 NAND 中\n");
    printf("[disk_ssd]    粒度: 页级映射（精准但表大）或块级映射（表小但慢）\n\n");

    printf("[disk_ssd] 2. 垃圾回收 (GC):\n");
    printf("[disk_ssd]    当空闲 Block 不足时触发\n");
    printf("[disk_ssd]    过程:\n");
    printf("[disk_ssd]      ① 选择回收目标 Block（含有效+无效页）\n");
    printf("[disk_ssd]      ② 将有效页复制到新 Block\n");
    printf("[disk_ssd]      ③ 擦除旧 Block，标记为空闲\n");
    printf("[disk_ssd]    写放大 (WAF): 实际写入 NAND / 主机写入\n\n");

    printf("[disk_ssd] 3. 写入均衡 (Wear Leveling):\n");
    printf("[disk_ssd]    - 动态均衡: 新数据写入写入次数少的 Block\n");
    printf("[disk_ssd]    - 静态均衡: 将冷数据迁移到写入次数多的 Block\n");
    printf("[disk_ssd]    - 目的: 避免某些 Block 提前耗尽（写穿）\n\n");

    printf("[disk_ssd] 4. 预留空间 (Over-Provisioning):\n");
    printf("[disk_ssd]    - SSD 实际 NAND 容量 > 用户可用容量\n");
    printf("[disk_ssd]    - 典型 OP 比例: 7%%~28%%\n");
    printf("[disk_ssd]    - 用于: GC 缓冲、坏块替换、性能保障\n");
}

/* ============================================================
 * Demo 3: TRIM / DISCARD
 * ============================================================ */
static void demo_trim_discard(void) {
    printf("[disk_ssd] === TRIM / DISCARD ===\n\n");

    printf("[disk_ssd] 为什么需要 TRIM:\n");
    printf("[disk_ssd]    - 删除文件时，文件系统只标记 metadata，不通知 SSD\n");
    printf("[disk_ssd]    - SSD 不知道哪些页已失效 → GC 浪费精力搬运无效数据\n");
    printf("[disk_ssd]    - TRIM 让文件系统通知 SSD: \"这些 LBA 已释放\"\n\n");

    printf("[disk_ssd] TRIM 操作方式:\n\n");

    printf("[disk_ssd] 1. 在线 TRIM (mount -o discard):\n");
    printf("[disk_ssd]    - 每次删除文件时立即发送 TRIM 命令\n");
    printf("[disk_ssd]    - 优点: 实时回收空间\n");
    printf("[disk_ssd]    - 缺点: 影响删除性能\n\n");

    printf("[disk_ssd] 2. 批量 TRIM (fstrim):\n");
    printf("[disk_ssd]    # 手动执行\n");
    printf("[disk_ssd]    fstrim -v /mnt/ssd\n");
    printf("[disk_ssd]    # systemd 定时器（通常每周执行）\n");
    printf("[disk_ssd]    systemctl enable fstrim.timer\n\n");

    printf("[disk_ssd] 3. blkdiscard（整盘 TRIM）:\n");
    printf("[disk_ssd]    blkdiscard /dev/nvme0n1\n");
    printf("[disk_ssd]    # 警告: 会清除所有数据！仅用于重置设备\n\n");

    printf("[disk_ssd] 4. fallocate PUNCH_HOLE:\n");
    printf("[disk_ssd]    fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,\n");
    printf("[disk_ssd]              offset, length);\n");
    printf("[disk_ssd]    - 在文件中间打洞，释放对应磁盘空间\n");
    printf("[disk_ssd]    - 数据库常用: 释放已删除记录占用的空间\n");

    /* 检查挂载点的 discard 选项 */
    printf("\n[disk_ssd] 检查当前挂载点 discard 选项:\n");
    FILE *fp = fopen("/proc/mounts", "r");
    if (fp) {
        char line[512];
        int found = 0;
        while (fgets(line, sizeof(line), fp) && found < 3) {
            if (strstr(line, "discard")) {
                printf("[disk_ssd]   %s", line);
                found++;
            }
        }
        if (found == 0) printf("[disk_ssd]   (未发现启用 discard 的挂载点)\n");
        fclose(fp);
    }
}

/* ============================================================
 * Demo 4: 队列深度对性能的影响
 * ============================================================ */
static void demo_queue_depth(void) {
    printf("[disk_ssd] === 队列深度对性能的影响 ===\n\n");

    printf("[disk_ssd] 队列深度 (Queue Depth, QD):\n");
    printf("[disk_ssd]    - 同时发起的未完成 I/O 请求数\n");
    printf("[disk_ssd]    - NVMe 协议支持最多 64K 个队列，每队列 64K 命令\n\n");

    printf("[disk_ssd] SSD 并行性来源:\n");
    printf("[disk_ssd]    1. 通道并行: 多个 NAND 通道独立工作\n");
    printf("[disk_ssd]    2. Die 并行: 同通道内多个 Die 可交错操作\n");
    printf("[disk_ssd]    3. Plane 并行: 同 Die 内多 Plane 可同时操作\n\n");

    printf("[disk_ssd] 队列深度与性能关系（概念模型）:\n");
    printf("[disk_ssd]    IOPS = f(队列深度)\n");
    printf("[disk_ssd]    低 QD (1-4):  线性增长（SSD 内部并行未充分利用）\n");
    printf("[disk_ssd]    中 QD (8-32): 接近峰值（充分利用了内部并行）\n");
    printf("[disk_ssd]    高 QD (>64):  饱和甚至下降（控制器处理瓶颈/延迟增加）\n\n");

    printf("[disk_ssd] 性能估算示例 (NVMe SSD):\n");
    printf("[disk_ssd]    QD=1:   顺序读 800MB/s,  随机读 50K IOPS\n");
    printf("[disk_ssd]    QD=4:   顺序读 3200MB/s, 随机读 200K IOPS\n");
    printf("[disk_ssd]    QD=16:  顺序读 7000MB/s, 随机读 600K IOPS\n");
    printf("[disk_ssd]    QD=32:  顺序读 7000MB/s, 随机读 800K IOPS\n\n");

    printf("[disk_ssd] 调度器影响:\n");
    printf("[disk_ssd]    NVMe SSD: 使用 none (noop) 调度器\n");
    printf("[disk_ssd]      原因: 无需 LBA 排序，越直接越好\n");
}

/* ============================================================
 * Demo 5: 检查 SSD 设备信息
 * ============================================================ */
static void demo_probe_ssd_info(void) {
    printf("[disk_ssd] === 检查 SSD 设备信息 ===\n\n");

    DIR *dir = opendir("/sys/block");
    if (!dir) {
        printf("[disk_ssd] 无法打开 /sys/block\n");
        return;
    }

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL && count < 5) {
        if (entry->d_name[0] == '.') continue;

        char path[256];

        /* 判断是否为旋转设备 (0=SSD, 1=HDD) */
        snprintf(path, sizeof(path), "/sys/block/%s/queue/rotational", entry->d_name);
        long rotational = read_sysfs_long(path);

        /* 读取 TRIM 支持 (discard 能力) */
        snprintf(path, sizeof(path), "/sys/block/%s/queue/discard_max_bytes", entry->d_name);
        long discard_max = read_sysfs_long(path);

        /* 读取调度器 */
        snprintf(path, sizeof(path), "/sys/block/%s/queue/scheduler", entry->d_name);
        char sched[256] = {0};
        read_sysfs_str(path, sched, sizeof(sched));

        /* 读取 nr_requests */
        snprintf(path, sizeof(path), "/sys/block/%s/queue/nr_requests", entry->d_name);
        long nr_requests = read_sysfs_long(path);

        const char *type = (rotational == 0) ? "SSD" :
                           (rotational == 1) ? "HDD" : "未知";

        printf("[disk_ssd] 设备: /dev/%s  类型=%s\n", entry->d_name, type);
        printf("[disk_ssd]   调度器: %s", sched[0] ? sched : "N/A");
        printf("[disk_ssd]   队列深度=%ld  discard_max=%ld 字节\n",
               nr_requests, discard_max);
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
    printf("  Linux SSD 优化学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: SSD 硬件特性 ---\n");
    demo_ssd_hardware();
    printf("\n");

    printf("--- Demo 2: FTL 闪存转换层 ---\n");
    demo_ftl();
    printf("\n");

    printf("--- Demo 3: TRIM / DISCARD ---\n");
    demo_trim_discard();
    printf("\n");

    printf("--- Demo 4: 队列深度与性能 ---\n");
    demo_queue_depth();
    printf("\n");

    printf("--- Demo 5: 探测 SSD 信息 ---\n");
    demo_probe_ssd_info();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
