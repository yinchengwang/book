/**
 * compaction.c — LSM-Tree Compaction 与空间回收演示
 *
 * 演示两种主流 Compaction 策略：
 *   1. Leveled Compaction（层级压缩）
 *   2. Tiered Compaction（分层压缩）
 *
 * 计算写放大(Write Amplification)和空间放大(Space Amplification)，
 * 帮助理解 RocksDB 等 KV 存储的 Compaction 权衡。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/* ============================================================================
 * 常量与配置
 * ============================================================================ */

/* 层级数（不含 L0） */
#define MAX_LEVELS 6

/* 每层大小增长倍数（T） */
#define LEVEL_SIZE_RATIO 10

/* 初始层大小（MB） */
#define BASE_SIZE_MB 1

/* 目标空间放大系数 */
#define TARGET_SPACE_AMPLIFICATION 1.2

/* ============================================================================
 * 数据结构
 * ============================================================================ */

/**
 * Compaction 统计信息
 */
typedef struct {
    double write_amplification;   /* 写放大 = 实际写入量 / 逻辑写入量 */
    double space_amplification;   /* 空间放大 = 磁盘占用 / 有效数据量 */
    int64_t total_bytes_written;  /* 总写入字节数 */
    int64_t total_bytes_read;     /* 总读取字节数 */
    int compaction_count;         /* Compaction 次数 */
} compaction_stats_t;

/**
 * 层信息
 */
typedef struct {
    int level;            /* 层编号（0-based，L0 为最上层） */
    int64_t size_mb;      /* 当前大小 */
    int64_t max_size_mb;  /* 最大允许大小 */
    int file_count;       /* 文件数 */
} level_info_t;

/* ============================================================================
 * Leveled Compaction 实现
 * ============================================================================ */

/**
 * 模拟 Leveled Compaction 过程
 *
 * Leveled 特点：
 * - L0 层接受所有新写入（可能有重叠）
 * - 每层只有一个 SSTable 文件（无重叠）
 * - 向下合并时读取一层的全部数据 + 下层对应范围
 *
 * 写放大分析：
 * - 最坏情况：1 * log_T(N)  ~ O(log_T(N))
 * - RocksDB 典型值：~20-30x
 */
compaction_stats_t simulate_leveled_compaction(int64_t data_size_mb, int num_writes) {
    compaction_stats_t stats = {0};
    level_info_t levels[MAX_LEVELS + 1] = {0};
    int64_t write_size = data_size_mb / num_writes;

    /* 初始化各层最大容量（指数增长） */
    int64_t max_size = BASE_SIZE_MB;
    for (int i = 0; i <= MAX_LEVELS; i++) {
        levels[i].level = i;
        levels[i].max_size_mb = max_size;
        levels[i].size_mb = (i == 0) ? write_size : 0;
        max_size *= LEVEL_SIZE_RATIO;
    }

    printf("[compaction] ===== Leveled Compaction 模拟 =====\n");
    printf("[compaction] 数据总量: %lld MB, 分 %d 次写入\n", (long long)data_size_mb, num_writes);
    printf("[compaction] Level Size Ratio: %d\n\n", LEVEL_SIZE_RATIO);

    /* 模拟多次写入和 Compaction */
    for (int w = 1; w <= num_writes; w++) {
        /* 写入 L0 */
        levels[0].size_mb += write_size;
        levels[0].file_count++;
        stats.total_bytes_written += write_size;

        /* 检查是否需要 Compaction */
        for (int l = 0; l < MAX_LEVELS; l++) {
            if (levels[l].size_mb > levels[l].max_size_mb) {
                /* 执行 Compaction：合并 L 到 L+1 */
                int64_t read_bytes = levels[l].size_mb;
                int64_t write_bytes = levels[l].size_mb;

                /* Leveled: 读取一层的全部数据 */
                stats.total_bytes_read += read_bytes;
                stats.total_bytes_written += write_bytes;
                stats.compaction_count++;

                levels[l + 1].size_mb += levels[l].size_mb;
                levels[l + 1].file_count++;
                levels[l].size_mb = 0;
                levels[l].file_count = 0;

                printf("[compaction] Write #%d: Compaction L%d -> L%d "
                       "(读 %lld MB, 写 %lld MB)\n",
                       w, l, l + 1, (long long)read_bytes, (long long)write_bytes);
            }
        }
    }

    /* 计算放大系数 */
    stats.write_amplification = (double)stats.total_bytes_written / data_size_mb;

    /* Leveled 空间放大 = 各层容量之和 / 数据量 */
    int64_t total_capacity = 0;
    for (int i = 0; i <= MAX_LEVELS; i++) {
        total_capacity += levels[i].max_size_mb;
    }
    stats.space_amplification = (double)total_capacity / data_size_mb;

    return stats;
}

/* ============================================================================
 * Tiered Compaction 实现
 * ============================================================================ */

/**
 * 模拟 Tiered Compaction 过程
 *
 * Tiered 特点：
 * - 每层可以有多个 SSTable 文件（无排序要求）
 * - 只有当层满时才与下一层合并
 * - 合并时读取整个层的所有文件
 *
 * 写放大分析：
 * - 最坏情况：(T-1)/T * N  ~ O(N)
 * - 空间放大：~2x（T 很大时）
 */
compaction_stats_t simulate_tiered_compaction(int64_t data_size_mb, int num_writes) {
    compaction_stats_t stats = {0};
    level_info_t levels[MAX_LEVELS + 1] = {0};
    int64_t write_size = data_size_mb / num_writes;

    /* 初始化各层最大容量 */
    int64_t max_size = BASE_SIZE_MB;
    for (int i = 0; i <= MAX_LEVELS; i++) {
        levels[i].level = i;
        levels[i].max_size_mb = max_size;
        levels[i].size_mb = 0;
        max_size *= LEVEL_SIZE_RATIO;
    }

    printf("[compaction] ===== Tiered Compaction 模拟 =====\n");
    printf("[compaction] 数据总量: %lld MB, 分 %d 次写入\n", (long long)data_size_mb, num_writes);
    printf("[compaction] Level Size Ratio: %d\n\n", LEVEL_SIZE_RATIO);

    /* 模拟多次写入 */
    for (int w = 1; w <= num_writes; w++) {
        levels[0].size_mb += write_size;
        levels[0].file_count++;
        stats.total_bytes_written += write_size;

        /* 检查是否需要 Compaction */
        for (int l = 0; l < MAX_LEVELS; l++) {
            if (levels[l].size_mb >= levels[l].max_size_mb) {
                /* Tiered: 读取整个层的全部数据 */
                int64_t read_bytes = levels[l].size_mb;
                int64_t write_bytes = levels[l].size_mb;

                stats.total_bytes_read += read_bytes;
                stats.total_bytes_written += write_bytes;
                stats.compaction_count++;

                levels[l + 1].size_mb += levels[l].size_mb;
                levels[l + 1].file_count++;
                levels[l].size_mb = 0;
                levels[l].file_count = 0;

                printf("[compaction] Write #%d: Compaction L%d -> L%d "
                       "(读 %lld MB, 写 %lld MB)\n",
                       w, l, l + 1, (long long)read_bytes, (long long)write_bytes);
            }
        }
    }

    /* 计算放大系数 */
    stats.write_amplification = (double)stats.total_bytes_written / data_size_mb;
    stats.space_amplification = (data_size_mb > 0)
        ? (double)stats.total_bytes_written / data_size_mb
        : 1.0;

    return stats;
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void) {
    printf("========================================\n");
    printf("  LSM-Tree Compaction 策略对比演示\n");
    printf("========================================\n\n");

    /* 测试参数：100MB 数据，分 20 次写入 */
    int64_t data_size = 100;  /* MB */
    int num_writes = 20;

    /* Leveled Compaction */
    compaction_stats_t leveled = simulate_leveled_compaction(data_size, num_writes);

    printf("\n[compaction] Leveled 统计结果:\n");
    printf("[compaction]   写放大: %.2fx\n", leveled.write_amplification);
    printf("[compaction]   空间放大: %.2fx\n", leveled.space_amplification);
    printf("[compaction]   总读取: %lld MB\n", (long long)leveled.total_bytes_read);
    printf("[compaction]   总写入: %lld MB\n", (long long)leveled.total_bytes_written);
    printf("[compaction]   Compaction 次数: %d\n\n", leveled.compaction_count);

    /* Tiered Compaction */
    compaction_stats_t tiered = simulate_tiered_compaction(data_size, num_writes);

    printf("\n[compaction] Tiered 统计结果:\n");
    printf("[compaction]   写放大: %.2fx\n", tiered.write_amplification);
    printf("[compaction]   空间放大: %.2fx\n", tiered.space_amplification);
    printf("[compaction]   总读取: %lld MB\n", (long long)tiered.total_bytes_read);
    printf("[compaction]   总写入: %lld MB\n", (long long)tiered.total_bytes_written);
    printf("[compaction]   Compaction 次数: %d\n\n", tiered.compaction_count);

    /* 对比总结 */
    printf("========================================\n");
    printf("  写放大/空间放大权衡总结\n");
    printf("========================================\n");
    printf("[compaction] 策略        写放大    空间放大    适用场景\n");
    printf("[compaction] ----------  --------  ---------  ------------------------\n");
    printf("[compaction] Leveled     较高(~10x) 低(~1.1x)  读密集型，写QPS不高\n");
    printf("[compaction] Tiered      低(~2x)   较高(~2x)   写密集型，空间充足\n");
    printf("[compaction] ----------  --------  ---------  ------------------------\n");

    return 0;
}
