/**
 * @file main.c
 * @brief Btrfs 文件系统学习演示
 *
 * 演示内容：
 *   - Btrfs COW（写时复制）原理
 *   - 快照（Snapshot）创建与恢复
 *   - 透明压缩（zlib/lzo/zstd）
 *   - RAID 级别（RAID0/1/10/5/6）
 *
 * 编译：gcc -std=c11 -Wall -o btrfs main.c
 * 运行：./btrfs
 * 前提：部分演示功能需要实际 Btrfs 文件系统支持
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ============================================================
 * 辅助：检查文件是否存在
 * ============================================================ */
static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* ============================================================
 * Demo 1: Btrfs COW（写时复制）原理
 * ============================================================ */
static void demo_cow(void) {
    printf("[btrfs] === COW（写时复制）原理 ===\n\n");

    printf("[btrfs] 1. 什么是 COW（Copy-on-Write）？\n");
    printf("[btrfs]    传统文件系统：修改数据时直接覆盖原位\n");
    printf("[btrfs]    Btrfs：修改数据时先复制到新位置，再更新指针\n");
    printf("[btrfs]    旧数据保持不变，直到没有引用指向它\n\n");

    printf("[btrfs] 2. COW 的核心数据结构：B-Tree\n");
    printf("[btrfs]    Btrfs 用 B-Tree 组织所有元数据：\n");
    printf("[btrfs]    - 根节点（Root Node）指向子卷\n");
    printf("[btrfs]    - 叶子节点包含 extent 信息\n");
    printf("[btrfs]    - 修改任何节点都要 COW 到新位置\n\n");

    printf("[btrfs] 3. COW 操作示例：\n");
    printf("[btrfs]    原始状态:\n");
    printf("[btrfs]      根节点 -> [块A] -> [块B] -> [块C]\n\n");

    printf("[btrfs]    修改块B:\n");
    printf("[btrfs]      ① 分配新块 B', 复制 B 的内容到 B'\n");
    printf("[btrfs]      ② 在 B' 上执行修改\n");
    printf("[btrfs]      ③ 更新父节点（块A）的指针指向 B'\n");
    printf("[btrfs]      ④ 块A 也需要 COW 到 A'（级联更新到根）\n");
    printf("[btrfs]      ⑤ 旧块 B 标记为可回收\n\n");

    printf("[btrfs] 4. COW 的优势与代价：\n");
    printf("[btrfs]    优势: 天然支持快照 + 数据完整性（不破坏旧数据）\n");
    printf("[btrfs]    代价: 写入放大（修改 1 块可能触发多块 COW）\n");
    printf("[btrfs]    代价: 碎片化（extent 分散在磁盘各处）\n");
}

/* ============================================================
 * Demo 2: 快照（Snapshot）创建与恢复
 * ============================================================ */
static void demo_snapshot(void) {
    printf("\n[btrfs] === 快照（Snapshot）机制 ===\n\n");

    printf("[btrfs] 1. 快照原理：\n");
    printf("[btrfs]    快照 = 子卷在某一时刻的只读副本\n");
    printf("[btrfs]    利用 COW：快照创建时只复制根节点指针，不复制数据\n");
    printf("[btrfs]    后续写操作触发 COW，快照数据保持不变\n\n");

    printf("[btrfs] 2. Btrfs 快照命令：\n");
    printf("[btrfs]    # 创建子卷\n");
    printf("[btrfs]    btrfs subvolume create /mnt/btrfs/subvol\n");
    printf("[btrfs]    # 创建只读快照\n");
    printf("[btrfs]    btrfs subvolume snapshot -r /mnt/btrfs/subvol "
           "/mnt/btrfs/snap_20260101\n");
    printf("[btrfs]    # 列出快照\n");
    printf("[btrfs]    btrfs subvolume list /mnt/btrfs\n");
    printf("[btrfs]    # 从快照恢复（只读快照 → 可写快照）\n");
    printf("[btrfs]    btrfs subvolume snapshot /mnt/btrfs/snap_20260101 "
           "/mnt/btrfs/subvol_restored\n\n");

    printf("[btrfs] 3. 快照与增量备份：\n");
    printf("[btrfs]    # 发送增量快照到文件\n");
    printf("[btrfs]    btrfs send -p snap_old snap_new | gzip > snap.bin.gz\n");
    printf("[btrfs]    # 从文件恢复快照\n");
    printf("[btrfs]    gzip -d snap.bin.gz | btrfs receive /mnt/btrfs\n\n");

    printf("[btrfs] 4. 模拟快照创建流程（概念演示）：\n");
    printf("[btrfs]    Step 1: 当前文件系统树的根节点 refcount=1\n");
    printf("[btrfs]    Step 2: 创建快照 → 根节点 refcount=2（快照 + 原卷）\n");
    printf("[btrfs]    Step 3: 修改文件 → COW 复制被修改的节点\n");
    printf("[btrfs]    Step 4: 快照的节点 refcount=1, 原卷的节点 refcount=1\n");
}

/* ============================================================
 * Demo 3: 透明压缩
 * ============================================================ */
static void demo_compression(void) {
    printf("\n[btrfs] === 透明压缩 ===\n\n");

    printf("[btrfs] 1. Btrfs 支持的压缩算法：\n");
    printf("[btrfs]    zlib:   压缩率高，速度慢（默认级别 3）\n");
    printf("[btrfs]    lzo:    压缩率中等，速度最快\n");
    printf("[btrfs]    zstd:   压缩率高，速度快（推荐，内核 4.14+）\n\n");

    printf("[btrfs] 2. 启用压缩：\n");
    printf("[btrfs]    # 挂载时全局启用\n");
    printf("[btrfs]    mount -o compress=zstd /dev/sda /mnt\n\n");
    printf("[btrfs]    # 文件/目录级别启用\n");
    printf("[btrfs]    btrfs property set /mnt/data compression zstd\n\n");
    printf("[btrfs]    # 强制压缩（即使数据不可压缩也尝试）\n");
    printf("[btrfs]    mount -o compress-force=zstd /dev/sda /mnt\n\n");

    printf("[btrfs] 3. 压缩效果对比（以 extents 为单位，通常 128KB）：\n");
    printf("[btrfs]    算法     压缩率      速度\n");
    printf("[btrfs]    zlib     ~2.5:1      ~50 MB/s\n");
    printf("[btrfs]    lzo      ~2.0:1     ~400 MB/s\n");
    printf("[btrfs]    zstd     ~2.8:1     ~200 MB/s\n\n");

    printf("[btrfs] 4. 压缩与 COW 的关系：\n");
    printf("[btrfs]    压缩在 extent 写入前进行\n");
    printf("[btrfs]    写入流程: 数据 → 压缩 → 新 extent → COW 更新指针\n");
    printf("[btrfs]    读取流程: COW 找 extent → 读入 → 解压 → 返回数据\n");
}

/* ============================================================
 * Demo 4: RAID 级别
 * ============================================================ */
static void demo_raid(void) {
    printf("\n[btrfs] === RAID 级别支持 ===\n\n");

    printf("[btrfs] Btrfs 原生支持以下 RAID 级别（不需要 mdadm）：\n\n");

    printf("[btrfs] ① RAID 0（条带化）\n");
    printf("[btrfs]    数据分布: 条带分散到所有磁盘\n");
    printf("[btrfs]    容量: N × disk_size（100%% 利用率）\n");
    printf("[btrfs]    冗余: 无（一块盘坏全丢）\n");
    printf("[btrfs]    场景: 临时数据、缓存、性能优先\n\n");

    printf("[btrfs] ② RAID 1（镜像）\n");
    printf("[btrfs]    数据分布: 每份数据写 2 份副本（分布在不同盘）\n");
    printf("[btrfs]    容量: N/2 × disk_size（50%% 利用率）\n");
    printf("[btrfs]    冗余: 容忍 1 块盘故障\n");
    printf("[btrfs]    场景: 系统盘、元数据\n\n");

    printf("[btrfs] ③ RAID 10（条带 + 镜像）\n");
    printf("[btrfs]    先做镜像（2 盘一组），再做条带\n");
    printf("[btrfs]    容量: N/2 × disk_size\n");
    printf("[btrfs]    冗余: 每个镜像组容忍 1 块盘故障\n");
    printf("[btrfs]    场景: 高性能 + 高可靠 数据库\n\n");

    printf("[btrfs] ④ RAID 5（分布式奇偶校验）\n");
    printf("[btrfs]    1 块盘的容量用于校验，分布在所有盘中\n");
    printf("[btrfs]    容量: (N-1) × disk_size\n");
    printf("[btrfs]    冗余: 容忍 1 块盘故障\n");
    printf("[btrfs]    注意: Btrfs RAID5 有写洞问题，生产慎用\n\n");

    printf("[btrfs] ⑤ RAID 6（双奇偶校验）\n");
    printf("[btrfs]    2 块盘的容量用于校验\n");
    printf("[btrfs]    容量: (N-2) × disk_size\n");
    printf("[btrfs]    冗余: 容忍 2 块盘同时故障\n\n");

    printf("[btrfs] 创建多盘 Btrfs 文件系统：\n");
    printf("[btrfs]    mkfs.btrfs -d raid1 -m raid1 /dev/sda /dev/sdb\n");
    printf("[btrfs]    其中 -d 是数据 RAID 级别，-m 是元数据 RAID 级别\n");
    printf("[btrfs]    Btrfs 允许数据和元数据使用不同的 RAID 级别\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Btrfs 文件系统学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: COW 写时复制原理 ---\n");
    demo_cow();

    printf("\n--- Demo 2: 快照创建与恢复 ---\n");
    demo_snapshot();

    printf("\n--- Demo 3: 透明压缩 ---\n");
    demo_compression();

    printf("\n--- Demo 4: RAID 级别 ---\n");
    demo_raid();

    printf("\n========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
