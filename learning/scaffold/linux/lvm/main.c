/**
 * @file main.c
 * @brief Linux LVM 逻辑卷管理学习演示
 *
 * 演示内容：
 *   - LVM 架构（PV 物理卷 / VG 卷组 / LV 逻辑卷）
 *   - 快照卷创建（COW 写时复制）
 *   - thin-pool 精简配置
 *   - 动态扩容 / 缩容概念
 *
 * 编译：gcc -std=c11 -Wall -o lvm main.c
 * 运行：./lvm
 *
 * 注意：实际 LVM 操作需要 root 权限和 lvm2 工具包。
 *       本程序演示 LVM 概念模型并尝试读取系统 LVM 信息。
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
 * 辅助：执行命令并捕获输出
 * ============================================================ */
static void run_cmd(const char *cmd, char *out, size_t size) {
    FILE *fp = popen(cmd, "r");
    if (!fp) { out[0] = '\0'; return; }
    size_t total = 0;
    while (total < size - 1 && fgets(out + total, (int)(size - total), fp)) {
        total = strlen(out);
    }
    pclose(fp);
}

/* ============================================================
 * Demo 1: LVM 基本架构概念
 * ============================================================ */
static void demo_lvm_architecture(void) {
    printf("[lvm] === LVM 基本架构概念 ===\n\n");

    printf("[lvm] LVM (Logical Volume Manager) 三层架构:\n\n");

    printf("[lvm] ┌──────────────────────────────────────┐\n");
    printf("[lvm] │   LV (逻辑卷) - 用户可见的块设备        │\n");
    printf("[lvm] │   可动态扩容/缩容/快照                   │\n");
    printf("[lvm] ├──────────────────────────────────────┤\n");
    printf("[lvm] │   VG (卷组) - 存储池，聚合多个 PV        │\n");
    printf("[lvm] │   统一管理空间分配                        │\n");
    printf("[lvm] ├──────────────────────────────────────┤\n");
    printf("[lvm] │   PV (物理卷) - 物理磁盘/分区            │\n");
    printf("[lvm] │   /dev/sda1, /dev/sdb1 ...              │\n");
    printf("[lvm] └──────────────────────────────────────┘\n\n");

    printf("[lvm] 核心概念:\n");
    printf("[lvm]    PV (Physical Volume):\n");
    printf("[lvm]      - 物理磁盘或分区，初始化为 LVM 格式\n");
    printf("[lvm]      - 命令: pvcreate /dev/sdb1\n");
    printf("[lvm]      - 查看: pvs / pvdisplay\n\n");

    printf("[lvm]    VG (Volume Group):\n");
    printf("[lvm]      - 一个或多个 PV 组成的存储池\n");
    printf("[lvm]      - 命令: vgcreate myvg /dev/sdb1 /dev/sdc1\n");
    printf("[lvm]      - 查看: vgs / vgdisplay\n\n");

    printf("[lvm]    LV (Logical Volume):\n");
    printf("[lvm]      - 从 VG 中划分的逻辑卷\n");
    printf("[lvm]      - 命令: lvcreate -L 10G -n mylv myvg\n");
    printf("[lvm]      - 设备路径: /dev/myvg/mylv 或 /dev/mapper/myvg-mylv\n");
    printf("[lvm]      - 查看: lvs / lvdisplay\n");
}

/* ============================================================
 * Demo 2: 快照卷与 COW
 * ============================================================ */
static void demo_snapshot_cow(void) {
    printf("[lvm] === 快照卷与写时复制（COW） ===\n\n");

    printf("[lvm] LVM 快照原理:\n\n");

    printf("[lvm] 1. 创建快照:\n");
    printf("[lvm]    lvcreate -L 1G -s -n mysnap /dev/myvg/mylv\n");
    printf("[lvm]    -s: 创建快照卷\n");
    printf("[lvm]    -L 1G: 快照卷大小（用于存储变更数据）\n\n");

    printf("[lvm] 2. COW (Copy-On-Write) 写时复制:\n");
    printf("[lvm]    初始状态: 快照卷为空，所有数据指向原始 LV\n");
    printf("[lvm]    写入发生时:\n");
    printf("[lvm]      ① 将被修改的原始数据块复制到快照卷\n");
    printf("[lvm]      ② 更新 COW 表记录块映射关系\n");
    printf("[lvm]      ③ 原始 LV 写入新数据\n");
    printf("[lvm]    快照读取: 先从 COW 表查找 → 快照卷 → 原始 LV\n\n");

    printf("[lvm] 3. COW 表结构（简化）:\n");
    printf("[lvm]    struct cow_entry {\n");
    printf("[lvm]        uint64_t original_block;  // 原始块号\n");
    printf("[lvm]        uint64_t snapshot_block;  // 快照卷中的块号\n");
    printf("[lvm]        uint8_t  valid;           // 是否有效\n");
    printf("[lvm]    };\n\n");

    printf("[lvm] 4. 快照使用场景:\n");
    printf("[lvm]    - 数据库备份: 创建快照 → 备份快照 → 删除快照\n");
    printf("[lvm]    - 测试环境: 快照克隆 → 测试 → 丢弃\n");
    printf("[lvm]    - 升级回滚: 升级前快照 → 出问题后回滚\n");
}

/* ============================================================
 * Demo 3: thin-pool 精简配置
 * ============================================================ */
static void demo_thin_pool(void) {
    printf("[lvm] === thin-pool 精简配置 ===\n\n");

    printf("[lvm] 精简配置（Thin Provisioning）:\n");
    printf("[lvm]    - 按需分配，而非预分配全部空间\n");
    printf("[lvm]    - 多个 LV 共享同一个 thin-pool\n");
    printf("[lvm]    - 提交空间 > 物理空间（超分配）\n\n");

    printf("[lvm] 创建流程:\n");
    printf("[lvm]    # 1. 创建 thin-pool\n");
    printf("[lvm]    lvcreate --type thin-pool -L 100G -n thinpool myvg\n");
    printf("[lvm]    # 2. 从 thin-pool 创建精简 LV\n");
    printf("[lvm]    lvcreate --type thin -V 200G --thinpool thinpool -n thinlv\n");
    printf("[lvm]    #    物理只有 100G，但 LV 可见 200G（超分配 2:1）\n\n");

    printf("[lvm] thin-pool 优势:\n");
    printf("[lvm]    - 提高存储利用率（按实际使用量分配）\n");
    printf("[lvm]    - 支持数百个精简 LV 共享同一存储池\n");
    printf("[lvm]    - 配合监控自动扩容（dmeventd）\n\n");

    printf("[lvm] thin-pool 风险:\n");
    printf("[lvm]    - 物理空间耗尽导致所有 LV 只读\n");
    printf("[lvm]    - 需要监控 data%% 和 metadata%%\n");
    printf("[lvm]    - 建议设置 watermark 告警（如 80%%）\n");
}

/* ============================================================
 * Demo 4: 动态扩容与缩容
 * ============================================================ */
static void demo_resize(void) {
    printf("[lvm] === 动态扩容与缩容 ===\n\n");

    printf("[lvm] 扩容流程（在线扩容，无需卸载）:\n");
    printf("[lvm]    # 1. 扩展 LV\n");
    printf("[lvm]    lvextend -L +10G /dev/myvg/mylv\n");
    printf("[lvm]    # 2. 扩展文件系统\n");
    printf("[lvm]    resize2fs /dev/myvg/mylv      (ext4)\n");
    printf("[lvm]    xfs_growfs /mnt/data           (xfs)\n\n");

    printf("[lvm] 缩容流程（需要先卸载，ext4 支持，xfs 不支持）:\n");
    printf("[lvm]    # 1. 卸载文件系统\n");
    printf("[lvm]    umount /mnt/data\n");
    printf("[lvm]    # 2. 检查文件系统\n");
    printf("[lvm]    e2fsck -f /dev/myvg/mylv\n");
    printf("[lvm]    # 3. 缩容文件系统\n");
    printf("[lvm]    resize2fs /dev/myvg/mylv 5G\n");
    printf("[lvm]    # 4. 缩容 LV\n");
    printf("[lvm]    lvreduce -L 5G /dev/myvg/mylv\n\n");

    printf("[lvm] VG 扩容（添加新磁盘）:\n");
    printf("[lvm]    pvcreate /dev/sdd1\n");
    printf("[lvm]    vgextend myvg /dev/sdd1\n\n");

    printf("[lvm] VG 缩容（移除磁盘）:\n");
    printf("[lvm]    pvmove /dev/sdb1               # 迁移数据\n");
    printf("[lvm]    vgreduce myvg /dev/sdb1\n");
    printf("[lvm]    pvremove /dev/sdb1\n");
}

/* ============================================================
 * Demo 5: 探测系统 LVM 信息
 * ============================================================ */
static void demo_probe_system(void) {
    printf("[lvm] === 探测系统 LVM 信息 ===\n\n");

    char out[4096] = {0};

    /* 尝试读取 LVM 命令输出 */
    printf("[lvm] 1. PV 信息:\n");
    run_cmd("pvs --noheadings 2>/dev/null || echo '(未安装 lvm2 或无权限)'", out, sizeof(out));
    printf("[lvm]    %s\n", out[0] ? out : "    (无 LVM PV)");

    memset(out, 0, sizeof(out));
    printf("[lvm] 2. VG 信息:\n");
    run_cmd("vgs --noheadings 2>/dev/null || echo '(未安装 lvm2 或无权限)'", out, sizeof(out));
    printf("[lvm]    %s\n", out[0] ? out : "    (无 LVM VG)");

    memset(out, 0, sizeof(out));
    printf("[lvm] 3. LV 信息:\n");
    run_cmd("lvs --noheadings 2>/dev/null || echo '(未安装 lvm2 或无权限)'", out, sizeof(out));
    printf("[lvm]    %s\n", out[0] ? out : "    (无 LVM LV)");

    /* 检查 /dev/mapper/ 下的设备映射 */
    printf("[lvm] 4. 设备映射器条目:\n");
    DIR *dir = opendir("/dev/mapper");
    if (dir) {
        struct dirent *entry;
        int count = 0;
        while ((entry = readdir(dir)) != NULL && count < 5) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0 ||
                strcmp(entry->d_name, "control") == 0) continue;
            printf("[lvm]    /dev/mapper/%s\n", entry->d_name);
            count++;
        }
        if (count == 0) printf("[lvm]    (无额外映射设备)\n");
        closedir(dir);
    }

    printf("\n[lvm] 5. LVM 与设备映射器 (device-mapper):\n");
    printf("[lvm]    LVM 底层使用内核 device-mapper 框架\n");
    printf("[lvm]    每个 LV 在 /dev/mapper/ 下对应一个 dm 设备\n");
    printf("[lvm]    dmsetup ls 查看所有 dm 设备\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux LVM 逻辑卷管理学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: LVM 基本架构 ---\n");
    demo_lvm_architecture();
    printf("\n");

    printf("--- Demo 2: 快照卷与 COW ---\n");
    demo_snapshot_cow();
    printf("\n");

    printf("--- Demo 3: thin-pool 精简配置 ---\n");
    demo_thin_pool();
    printf("\n");

    printf("--- Demo 4: 动态扩容与缩容 ---\n");
    demo_resize();
    printf("\n");

    printf("--- Demo 5: 探测系统 LVM 信息 ---\n");
    demo_probe_system();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
