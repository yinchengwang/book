/**
 * @file main.c
 * @brief Linux ext4 / xfs 文件系统对比学习演示
 *
 * 演示内容：
 *   - ext4 vs xfs 特性对比（最大文件大小、最大卷大小、日志模式）
 *   - 延迟分配（delayed allocation）
 *   - extent 树、inline data
 *   - xfs 分配组（AG）
 *   - 使用 statfs 获取文件系统信息
 *
 * 编译：gcc -std=c11 -Wall -o ext4_xfs main.c
 * 运行：./ext4_xfs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

/* statfs 在 Linux 上可用，Windows 上不可用 */
#ifndef _WIN32
#include <sys/statfs.h>
#include <sys/vfs.h>
#endif

/* ============================================================
 * 辅助：格式化大小显示
 * ============================================================ */
static const char *format_size(unsigned long long bytes, char *buf, size_t size) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    int unit_idx = 0;
    double val = (double)bytes;

    while (val >= 1024.0 && unit_idx < 6) {
        val /= 1024.0;
        unit_idx++;
    }

    snprintf(buf, size, "%.2f %s", val, units[unit_idx]);
    return buf;
}

/* ============================================================
 * 辅助：通过 /proc/mounts 推断文件系统类型
 * ============================================================ */
static const char *get_fs_type(const char *path) {
#ifdef _WIN32
    (void)path;
    return "N/A (Windows)";
#else
    static char fs_type[32];
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) return "unknown";

    char device[256], mount[256], type[32], opts[256];
    int dummy1, dummy2;
    while (fscanf(fp, "%255s %255s %31s %255s %d %d\n",
                  device, mount, type, opts, &dummy1, &dummy2) == 6) {
        if (strcmp(mount, path) == 0 || strcmp(mount, "/") == 0) {
            strncpy(fs_type, type, sizeof(fs_type) - 1);
            fclose(fp);
            return fs_type;
        }
    }
    fclose(fp);
    return "unknown";
#endif
}

/* ============================================================
 * Demo 1: ext4 vs xfs 特性全面对比
 * ============================================================ */
static void demo_ext4_vs_xfs_overview(void) {
    printf("[ext4_xfs] === ext4 vs xfs 特性全面对比 ===\n\n");

    printf("[ext4_xfs] ┌──────────────────┬─────────────────────┬─────────────────────┐\n");
    printf("[ext4_xfs] │ 特性              │ ext4               │ xfs                 │\n");
    printf("[ext4_xfs] ├──────────────────┼─────────────────────┼─────────────────────┤\n");
    printf("[ext4_xfs] │ 最大文件大小      │ 16 TB              │ 8 EB               │\n");
    printf("[ext4_xfs] │ 最大卷大小        │ 1 EB               │ 8 EB               │\n");
    printf("[ext4_xfs] │ 最大文件名长度    │ 255 字节           │ 255 字节           │\n");
    printf("[ext4_xfs] │ 日志模式          │ journal/ordered/   │ 元数据日志         │\n");
    printf("[ext4_xfs] │                   │ writeback          │ (metadata only)    │\n");
    printf("[ext4_xfs] │ 分配方式          │ extent 树          │ B+tree extent      │\n");
    printf("[ext4_xfs] │ 延迟分配          │ 支持               │ 支持               │\n");
    printf("[ext4_xfs] │ 内联数据          │ inline_data        │ 不支持             │\n");
    printf("[ext4_xfs] │ 分配组 (AG)       │ 无                 │ 有（独立分配域）   │\n");
    printf("[ext4_xfs] │ 快照              │ 不支持             │ 不支持             │\n");
    printf("[ext4_xfs] │ 在线扩容          │ 支持               │ 支持               │\n");
    printf("[ext4_xfs] │ 在线缩容          │ 支持（离线也可）   │ 不支持             │\n");
    printf("[ext4_xfs] │ 写时复制 (CoW)    │ 不支持             │ 通过 reflink       │\n");
    printf("[ext4_xfs] │ 诞生年份          │ 2006 (ext3 升级)   │ 1994 (SGI IRIX)    │\n");
    printf("[ext4_xfs] └──────────────────┴─────────────────────┴─────────────────────┘\n");
}

/* ============================================================
 * Demo 2: 日志模式
 * ============================================================ */
static void demo_journal_modes(void) {
    printf("[ext4_xfs] === 日志模式 ===\n\n");

    printf("[ext4_xfs] ext4 三种日志模式:\n\n");

    printf("[ext4_xfs] 1. journal（最安全，最慢）:\n");
    printf("[ext4_xfs]    - 数据和元数据都先写入日志\n");
    printf("[ext4_xfs]    - 崩溃后完整恢复\n");
    printf("[ext4_xfs]    - 写入量翻倍（数据写两次）\n");
    printf("[ext4_xfs]    - mount -o data=journal\n\n");

    printf("[ext4_xfs] 2. ordered（默认，推荐）:\n");
    printf("[ext4_xfs]    - 元数据写入日志，数据直接写入磁盘\n");
    printf("[ext4_xfs]    - 保证: 元数据更新前数据必须已落盘\n");
    printf("[ext4_xfs]    - 性能好的同时保证文件系统一致性\n");
    printf("[ext4_xfs]    - mount -o data=ordered  (默认)\n\n");

    printf("[ext4_xfs] 3. writeback（最快，最不安全）:\n");
    printf("[ext4_xfs]    - 只有元数据写入日志\n");
    printf("[ext4_xfs]    - 不保证数据在元数据之前写入\n");
    printf("[ext4_xfs]    - 崩溃后可能看到旧数据（文件大小正确但内容是旧的）\n");
    printf("[ext4_xfs]    - mount -o data=writeback\n\n");

    printf("[ext4_xfs] xfs 日志:\n");
    printf("[ext4_xfs]    - 仅元数据日志（类似 ext4 ordered）\n");
    printf("[ext4_xfs]    - 不支持 data=journal 模式\n");
    printf("[ext4_xfs]    - 日志存储在内部的日志区域（可外置到独立设备）\n");
}

/* ============================================================
 * Demo 3: 延迟分配与 extent 树
 * ============================================================ */
static void demo_delalloc_extent(void) {
    printf("[ext4_xfs] === 延迟分配与 extent 树 ===\n\n");

    printf("[ext4_xfs] 1. 延迟分配 (Delayed Allocation):\n");
    printf("[ext4_xfs]    原理: write() 时不立即分配磁盘块\n");
    printf("[ext4_xfs]    而是延迟到数据从页缓存刷盘时再分配\n\n");
    printf("[ext4_xfs]    优势:\n");
    printf("[ext4_xfs]      - 更好的块分配决策（知道文件最终大小）\n");
    printf("[ext4_xfs]      - 减少碎片（可以分配连续的大 extent）\n");
    printf("[ext4_xfs]      - 减少 CPU 开销（批量分配）\n");
    printf("[ext4_xfs]      - 减少不必要的分配（短命临时文件可能从未刷盘）\n\n");
    printf("[ext4_xfs]    风险:\n");
    printf("[ext4_xfs]      - OOM 时可能导致数据丢失\n");
    printf("[ext4_xfs]      - 需要配合 fsync() 确保持久化\n\n");

    printf("[ext4_xfs] 2. extent 树:\n");
    printf("[ext4_xfs]    extent = (起始块号, 连续块数) 二元组\n");
    printf("[ext4_xfs]    替代传统的间接块映射表:\n\n");
    printf("[ext4_xfs]    ext3/ext2 间接块: 每 4KB 存储 1024 个块指针\n");
    printf("[ext4_xfs]      大文件需要多级间接块（三级 = 最多 4TB）\n\n");
    printf("[ext4_xfs]    ext4 extent:  一个 extent 可描述连续 128MB 的块\n");
    printf("[ext4_xfs]      10MB 文件: extent 树只需 1 个节点\n");
    printf("[ext4_xfs]      而间接块需要 2560 个指针 + 多个间接块\n\n");

    printf("[ext4_xfs]    xfs extent: 使用 B+tree 组织 extents\n");
    printf("[ext4_xfs]      支持更大的 extent 和更快的查找\n");
}

/* ============================================================
 * Demo 4: inline data 与分配组 (AG)
 * ============================================================ */
static void demo_inline_and_ag(void) {
    printf("[ext4_xfs] === inline data 与分配组 (AG) ===\n\n");

    printf("[ext4_xfs] 1. ext4 inline data:\n");
    printf("[ext4_xfs]    - 小文件（<60 字节）数据直接存储在 inode 中\n");
    printf("[ext4_xfs]    - 无需分配额外数据块\n");
    printf("[ext4_xfs]    - 节省空间 + 减少 I/O（一次读取即获得全部数据）\n");
    printf("[ext4_xfs]    - 启用: mkfs.ext4 -O inline_data\n");
    printf("[ext4_xfs]    - 适用: 大量小文件的场景（邮件、配置）\n\n");

    printf("[ext4_xfs] 2. xfs 分配组 (Allocation Group):\n");
    printf("[ext4_xfs]    - 将文件系统划分为多个独立的分配域\n");
    printf("[ext4_xfs]    - 每个 AG 有自己的:\n");
    printf("[ext4_xfs]        - 超级块副本\n");
    printf("[ext4_xfs]        - inode 管理\n");
    printf("[ext4_xfs]        - 空闲空间管理\n");
    printf("[ext4_xfs]        - 独立的分配锁\n\n");
    printf("[ext4_xfs]    - AG 带来并行性:\n");
    printf("[ext4_xfs]        - 多线程创建文件时可分配到不同 AG（无竞争）\n");
    printf("[ext4_xfs]        - 多线程空间分配时使用不同 AG 的空闲列表\n");
    printf("[ext4_xfs]        - 适合多核 CPU 大并发场景\n\n");
    printf("[ext4_xfs]    - 推荐 AG 数量: 4 (小于 1TB) ~ 32 (大于 8TB)\n");
}

#ifndef _WIN32
/* ============================================================
 * Demo 5: statfs 获取文件系统信息
 * ============================================================ */
static void demo_statfs(const char *path) {
    printf("[ext4_xfs] === statfs 获取文件系统信息 ===\n\n");

    struct statfs fs_info;

    if (statfs(path, &fs_info) < 0) {
        printf("[ext4_xfs] statfs 失败: %s\n", strerror(errno));
        return;
    }

    char buf[64];

    printf("[ext4_xfs] 路径: %s\n", path);
    printf("[ext4_xfs] 文件系统类型: %s (0x%lx)\n",
           get_fs_type(path), (unsigned long)fs_info.f_type);

    printf("[ext4_xfs] ─────────────────────────────────────\n");
    printf("[ext4_xfs] 块大小:    %ld 字节\n", fs_info.f_bsize);
    printf("[ext4_xfs] 片段大小:  %ld 字节\n", fs_info.f_frsize);
    printf("[ext4_xfs] ─────────────────────────────────────\n");
    printf("[ext4_xfs] 总块数:    %lu (%s)\n",
           fs_info.f_blocks,
           format_size((unsigned long long)fs_info.f_blocks * fs_info.f_bsize, buf, sizeof(buf)));
    printf("[ext4_xfs] 空闲块数:  %lu (%s)\n",
           fs_info.f_bfree,
           format_size((unsigned long long)fs_info.f_bfree * fs_info.f_bsize, buf, sizeof(buf)));
    printf("[ext4_xfs] 可用块数:  %lu (%s, 非 root 可用)\n",
           fs_info.f_bavail,
           format_size((unsigned long long)fs_info.f_bavail * fs_info.f_bsize, buf, sizeof(buf)));
    printf("[ext4_xfs] ─────────────────────────────────────\n");
    printf("[ext4_xfs] 总 inode:   %lu\n", fs_info.f_files);
    printf("[ext4_xfs] 空闲 inode: %lu\n", fs_info.f_ffree);
    printf("[ext4_xfs] 可用 inode: %lu\n", fs_info.f_favail);
    printf("[ext4_xfs] ─────────────────────────────────────\n");
    printf("[ext4_xfs] 最大文件名: %ld 字节\n", fs_info.f_namelen);

    double usage = 1.0 - (double)fs_info.f_bavail / (double)fs_info.f_blocks;
    printf("[ext4_xfs] 使用率:     %.1f%%\n", usage * 100.0);
    if (usage > 0.9) {
        printf("[ext4_xfs] ** 警告: 磁盘使用率超过 90%%！\n");
    }
}

/* ============================================================
 * Demo 6: 文件系统分型
 * ============================================================ */
static void demo_fs_identification(void) {
    printf("[ext4_xfs] === 文件系统类型识别 ===\n\n");

    struct statfs fs_info;

    const char *check_points[] = {"/", "/tmp", "/home", "/var", NULL};

    for (int i = 0; check_points[i] != NULL; i++) {
        if (statfs(check_points[i], &fs_info) == 0) {
            const char *type = get_fs_type(check_points[i]);
            unsigned long fstype = fs_info.f_type;

            const char *magic_name = "未知";
            if (fstype == 0xEF53)          magic_name = "ext2/ext3/ext4";
            else if (fstype == 0x58465342) magic_name = "xfs (XFSB)";
            else if (fstype == 0x9123683E) magic_name = "btrfs";
            else if (fstype == 0x6969)     magic_name = "nfs";
            else if (fstype == 0x01021994) magic_name = "tmpfs";
            else if (fstype == 0x65735546) magic_name = "fuse";
            else if (fstype == 0x6B414653) magic_name = "kafs";

            printf("[ext4_xfs] 挂载点: %-6s → fs=%s (魔数=0x%lX → %s)\n",
                   check_points[i], type, fstype, magic_name);
        }
    }
}
#else
/* ============================================================
 * Demo 5: statfs 模拟（Windows 平台）
 * ============================================================ */
static void demo_statfs(const char *path) {
    (void)path;
    printf("[ext4_xfs] === statfs 获取文件系统信息 ===\n\n");
    printf("[ext4_xfs] 注意: statfs 在 Windows 上不可用\n");
    printf("[ext4_xfs] 提示: 请在 Linux 环境中运行此演示\n");
    printf("[ext4_xfs] 相关概念: extent 分配、延迟分配、日志模式见上述 Demo\n");
}

/* ============================================================
 * Demo 6: 文件系统分型（Windows 平台）
 * ============================================================ */
static void demo_fs_identification(void) {
    printf("[ext4_xfs] === 文件系统类型识别 ===\n\n");
    printf("[ext4_xfs] 注意: 文件系统魔数识别在 Windows 上不可用\n");
    printf("[ext4_xfs] 提示: 请在 Linux 环境中使用以下命令验证:\n");
    printf("[ext4_xfs]    cat /proc/mounts        # 查看挂载的文件系统\n");
    printf("[ext4_xfs]    blkid /dev/sdX          # 查看 ext4/xfs 魔数\n");
}
#endif /* !_WIN32 */

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux ext4 / xfs 文件系统对比学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: ext4 vs xfs 全面对比 ---\n");
    demo_ext4_vs_xfs_overview();
    printf("\n");

    printf("--- Demo 2: 日志模式 ---\n");
    demo_journal_modes();
    printf("\n");

    printf("--- Demo 3: 延迟分配与 extent 树 ---\n");
    demo_delalloc_extent();
    printf("\n");

    printf("--- Demo 4: inline data 与分配组 ---\n");
    demo_inline_and_ag();
    printf("\n");

    printf("--- Demo 5: statfs 文件系统信息 ---\n\n");
    demo_statfs("/");
    printf("\n");

    printf("--- Demo 6: 文件系统类型识别 ---\n\n");
    demo_fs_identification();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
