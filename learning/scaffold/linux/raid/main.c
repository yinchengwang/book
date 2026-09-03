/**
 * @file main.c
 * @brief Linux RAID 阵列学习演示
 *
 * 演示内容：
 *   - RAID 0（条带化）：数据拆分到多盘，无冗余，性能最优
 *   - RAID 1（镜像）：数据完整复制到多盘，冗余度 50%
 *   - RAID 5（分布式奇偶校验）：单盘冗余 + 条带化读写
 *   - RAID 10（镜像+条带）：先镜像再条带，兼顾性能与冗余
 *   - 磁盘重建模拟：奇偶校验恢复丢失数据
 *
 * 编译：gcc -std=c11 -Wall -o raid main.c
 * 运行：./raid
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * RAID 配置常量
 * ============================================================ */
#define SECTOR_SIZE      512        /* 扇区大小（字节） */
#define DISKS_MAX          8        /* 最大磁盘数 */
#define STRIPE_SIZE        4        /* 条带单元大小（扇区数） */

/* ============================================================
 * 模拟磁盘：每个磁盘是一个字节数组
 * ============================================================ */
typedef struct {
    int    id;                       /* 磁盘编号 */
    size_t size;                     /* 磁盘大小（扇区数） */
    char  *data;                     /* 磁盘数据 */
} disk_t;

/* ============================================================
 * 磁盘管理
 * ============================================================ */
static disk_t *disk_create(int id, size_t sectors) {
    disk_t *d = (disk_t *)malloc(sizeof(disk_t));
    d->id   = id;
    d->size = sectors;
    d->data = (char *)calloc(sectors, SECTOR_SIZE);
    return d;
}

static void disk_free(disk_t *d) {
    if (d) {
        free(d->data);
        free(d);
    }
}

static void disk_write_sector(disk_t *d, size_t sector, const char *data) {
    if (sector < d->size) {
        memcpy(d->data + sector * SECTOR_SIZE, data, SECTOR_SIZE);
    }
}

static const char *disk_read_sector(disk_t *d, size_t sector) {
    if (sector < d->size) {
        return d->data + sector * SECTOR_SIZE;
    }
    return NULL;
}

/* ============================================================
 * XOR 奇偶校验计算
 * ============================================================ */
static void xor_parity(const char **blocks, int nblocks, char *parity) {
    memset(parity, 0, SECTOR_SIZE);
    for (int i = 0; i < SECTOR_SIZE; i++) {
        for (int b = 0; b < nblocks; b++) {
            if (blocks[b]) parity[i] ^= blocks[b][i];
        }
    }
}

/* ============================================================
 * 打印磁盘内容摘要（前 8 字节 + 校验和）
 * ============================================================ */
static void print_disk_summary(disk_t *d, size_t start_sector, size_t count) {
    printf("[raid]   磁盘 %d:\n", d->id);
    for (size_t s = start_sector; s < start_sector + count && s < d->size; s++) {
        const char *data = disk_read_sector(d, s);
        uint32_t sum = 0;
        for (int i = 0; i < SECTOR_SIZE; i++) sum += (unsigned char)data[i];
        printf("[raid]     扇区 %2zu: "
               "前8=[%02x %02x %02x %02x %02x %02x %02x %02x] sum=%u\n",
               s,
               (unsigned char)data[0], (unsigned char)data[1],
               (unsigned char)data[2], (unsigned char)data[3],
               (unsigned char)data[4], (unsigned char)data[5],
               (unsigned char)data[6], (unsigned char)data[7],
               sum);
    }
}

/* ============================================================
 * 生成测试数据
 * ============================================================ */
static void generate_test_data(char *buf, int seed) {
    for (int i = 0; i < SECTOR_SIZE; i++) {
        buf[i] = (char)((seed + i * 7) & 0xFF);
    }
}

/* ============================================================
 * Demo 1: RAID 0 — 条带化（无冗余）
 * ============================================================ */
static void demo_raid0(void) {
    printf("[raid] RAID 0 条带化演示 (2 盘, 条带=4 扇区):\n\n");

    int ndisks = 2;
    int stripes = STRIPE_SIZE;
    disk_t *disks[DISKS_MAX];

    for (int i = 0; i < ndisks; i++) {
        disks[i] = disk_create(i, stripes * 2);
    }

    /* 写入 4 个条带（交替分配到不同磁盘） */
    printf("[raid]   写入 4 个条带 (交替分配):\n");
    for (int stripe = 0; stripe < 4; stripe++) {
        char data[SECTOR_SIZE];
        generate_test_data(data, stripe * 100);
        int disk_id = stripe % ndisks;  /* 条带交替分配 */
        int sector  = stripe / ndisks;

        printf("[raid]     条带 %d → 磁盘 %d 扇区 %d (数据=0x%02x...)\n",
               stripe, disk_id, sector, (unsigned char)data[0]);
        disk_write_sector(disks[disk_id], sector, data);
    }

    printf("\n[raid]   RAID 0 特征:\n");
    printf("[raid]     - 总容量 = %zu 扇区 (100%% 利用率)\n",
           disks[0]->size + disks[1]->size);
    printf("[raid]     - 无冗余，任意磁盘故障 → 数据全部丢失\n");
    printf("[raid]     - 读写性能 = N 倍单盘 (N=磁盘数)\n");
    printf("[raid]     - 适用: 临时数据、缓存、日志\n");

    print_disk_summary(disks[0], 0, 2);
    print_disk_summary(disks[1], 0, 2);

    for (int i = 0; i < ndisks; i++) disk_free(disks[i]);
}

/* ============================================================
 * Demo 2: RAID 1 — 镜像（100% 冗余）
 * ============================================================ */
static void demo_raid1(void) {
    printf("[raid] RAID 1 镜像演示 (2 盘):\n\n");

    int ndisks = 2;
    disk_t *disks[DISKS_MAX];

    for (int i = 0; i < ndisks; i++) {
        disks[i] = disk_create(i, 4);
    }

    /* 写入数据：同时写入两块磁盘 */
    char data[SECTOR_SIZE];
    generate_test_data(data, 42);
    printf("[raid]   写入数据: 0x%02x%02x%02x%02x...\n",
           (unsigned char)data[0], (unsigned char)data[1],
           (unsigned char)data[2], (unsigned char)data[3]);

    for (int i = 0; i < ndisks; i++) {
        disk_write_sector(disks[i], 0, data);
    }

    printf("[raid]   读取: 可从任意磁盘读取相同数据\n");

    /* 模拟磁盘 0 故障 */
    printf("\n[raid]   模拟磁盘 0 故障...\n");
    printf("[raid]   从磁盘 1 读取 (数据完好):\n");

    const char *recovered = disk_read_sector(disks[1], 0);
    printf("[raid]     恢复数据: 0x%02x%02x%02x%02x...\n",
           (unsigned char)recovered[0], (unsigned char)recovered[1],
           (unsigned char)recovered[2], (unsigned char)recovered[3]);

    printf("\n[raid]   RAID 1 特征:\n");
    printf("[raid]     - 总容量 = 单盘容量 (50%% 利用率)\n");
    printf("[raid]     - 写性能 = 单盘性能 (需同时写两份)\n");
    printf("[raid]     - 读性能 = N 倍单盘 (可从多盘并发读)\n");
    printf("[raid]     - 可承受 N-1 块磁盘故障\n");
    printf("[raid]     - 适用: 关键元数据、系统盘\n");

    for (int i = 0; i < ndisks; i++) disk_free(disks[i]);
}

/* ============================================================
 * Demo 3: RAID 5 — 分布式奇偶校验
 * ============================================================ */
static void demo_raid5(void) {
    printf("[raid] RAID 5 分布式奇偶校验演示 (3 盘):\n\n");

    int ndisks = 3;
    disk_t *disks[DISKS_MAX];

    for (int i = 0; i < ndisks; i++) {
        disks[i] = disk_create(i, 4);
    }

    /* 写入 2 个条带（每个条带含 2 数据块 + 1 校验块） */
    printf("[raid]   写入 2 个数据条带:\n");

    /* 条带 0: 数据在盘 0、1，校验在盘 2 */
    char d0[SECTOR_SIZE], d1[SECTOR_SIZE], parity[SECTOR_SIZE];
    generate_test_data(d0, 100);
    generate_test_data(d1, 200);

    const char *blocks_0[] = { d0, d1 };
    xor_parity(blocks_0, 2, parity);

    disk_write_sector(disks[0], 0, d0);
    disk_write_sector(disks[1], 0, d1);
    disk_write_sector(disks[2], 0, parity);

    printf("[raid]     条带 0: D0→盘0, D1→盘1, P→盘2\n");
    printf("[raid]       D0=0x%02x..., D1=0x%02x..., P=0x%02x...\n",
           (unsigned char)d0[0], (unsigned char)d1[0], (unsigned char)parity[0]);

    /* 条带 1: 数据在盘 0、2，校验在盘 1（左旋） */
    char d2[SECTOR_SIZE], d3[SECTOR_SIZE], parity2[SECTOR_SIZE];
    generate_test_data(d2, 300);
    generate_test_data(d3, 400);

    const char *blocks_1[] = { d2, d3 };
    xor_parity(blocks_1, 2, parity2);

    disk_write_sector(disks[0], 1, d2);
    disk_write_sector(disks[2], 1, d3);
    disk_write_sector(disks[1], 1, parity2);

    printf("[raid]     条带 1: D2→盘0, D3→盘2, P→盘1 (左旋)\n");
    printf("[raid]       D2=0x%02x..., D3=0x%02x..., P=0x%02x...\n",
           (unsigned char)d2[0], (unsigned char)d3[0], (unsigned char)parity2[0]);

    /* 模拟磁盘 1 故障 → 使用 XOR 恢复数据 */
    printf("\n[raid]   模拟磁盘 1 故障, 重建扇区 0 的数据:\n");
    printf("[raid]     已知: D0(盘0扇区0), P(盘2扇区0)\n");
    printf("[raid]     恢复: D1 = D0 XOR P\n");

    const char *r_d0 = disk_read_sector(disks[0], 0);
    const char *r_p  = disk_read_sector(disks[2], 0);
    char recovered[SECTOR_SIZE];
    const char *rec_blocks[] = { r_d0, r_p };
    xor_parity(rec_blocks, 2, recovered);

    printf("[raid]     恢复 D1: 0x%02x... (期望 0x%02x...) → %s\n",
           (unsigned char)recovered[0], (unsigned char)d1[0],
           memcmp(recovered, d1, SECTOR_SIZE) == 0 ? "正确!" : "失败!");

    printf("\n[raid]   RAID 5 特征:\n");
    printf("[raid]     - 总容量 = (N-1)/N × 总磁盘容量\n");
    printf("[raid]     - 可承受 1 块磁盘故障（任意位置）\n");
    printf("[raid]     - 写性能受校验计算影响（写惩罚）\n");
    printf("[raid]     - 重建时间长（需读取所有盘计算校验）\n");

    for (int i = 0; i < ndisks; i++) disk_free(disks[i]);
}

/* ============================================================
 * Demo 4: RAID 10 — 镜像 + 条带
 * ============================================================ */
static void demo_raid10(void) {
    printf("[raid] RAID 10 (1+0) 镜像+条带演示 (4 盘):\n\n");

    int ndisks = 4;
    disk_t *disks[DISKS_MAX];

    for (int i = 0; i < ndisks; i++) {
        disks[i] = disk_create(i, 4);
    }

    printf("[raid]   布局: 先镜像(R1)再条带(R0)\n");
    printf("[raid]     R1 组 0: 磁盘 0 ↔ 磁盘 1 (镜像)\n");
    printf("[raid]     R1 组 1: 磁盘 2 ↔ 磁盘 3 (镜像)\n");
    printf("[raid]     R0 层: 条带分布在 R1 组 0 和 R1 组 1\n\n");

    /* 写入条带 0 → R1 组 0（盘0 + 盘1 镜像写） */
    /* 写入条带 1 → R1 组 1（盘2 + 盘3 镜像写） */
    char stripe0[SECTOR_SIZE], stripe1[SECTOR_SIZE];
    generate_test_data(stripe0, 500);
    generate_test_data(stripe1, 600);

    printf("[raid]   写入条带 0 → R1 组 0 (盘 0 + 盘 1):\n");
    disk_write_sector(disks[0], 0, stripe0);
    disk_write_sector(disks[1], 0, stripe0);

    printf("[raid]   写入条带 1 → R1 组 1 (盘 2 + 盘 3):\n");
    disk_write_sector(disks[2], 0, stripe1);
    disk_write_sector(disks[3], 0, stripe1);

    /* 验证容错 */
    printf("\n[raid]   容错分析:\n");
    printf("[raid]     - 磁盘 0 故障: R1 组 0 仍可从盘 1 读 → 正常\n");
    printf("[raid]     - 磁盘 0+2 故障: 两组各有一个镜像存活 → 正常\n");
    printf("[raid]     - 磁盘 0+1 故障: R1 组 0 全部丢失 → 数据丢失!\n\n");

    printf("[raid]   RAID 10 特征:\n");
    printf("[raid]     - 总容量 = N/2 × 单盘容量 (50%% 利用率)\n");
    printf("[raid]     - 可承受每组最多 1 盘故障\n");
    printf("[raid]     - 写性能 = N/2 倍单盘（优于 RAID 5）\n");
    printf("[raid]     - 重建只需从镜像盘复制（快于 RAID 5）\n");
    printf("[raid]     - 适用: 数据库、高性能 OLTP\n");

    print_disk_summary(disks[0], 0, 1);
    print_disk_summary(disks[2], 0, 1);

    for (int i = 0; i < ndisks; i++) disk_free(disks[i]);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux RAID 阵列学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: RAID 0 条带化 ---\n");
    demo_raid0();
    printf("\n");

    printf("--- Demo 2: RAID 1 镜像 ---\n");
    demo_raid1();
    printf("\n");

    printf("--- Demo 3: RAID 5 分布式奇偶校验 ---\n");
    demo_raid5();
    printf("\n");

    printf("--- Demo 4: RAID 10 镜像+条带 ---\n");
    demo_raid10();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
