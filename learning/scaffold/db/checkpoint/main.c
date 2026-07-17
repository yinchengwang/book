/**
 * @file main.c
 * @brief Checkpoint 机制演示
 *
 * 演示内容：
 * 1. Sharp Checkpoint vs Fuzzy Checkpoint
 * 2. 脏页刷盘策略
 * 3. LSN 水位管理
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 模拟数据结构
 * ============================================================ */

/** 脏页项 */
typedef struct dirty_page_s {
    uint32_t page_id;      /**< 页面ID */
    uint64_t rec_lsn;      /**< 该页面的最近修改 LSN */
    bool     dirty;        /**< 是否脏页 */
} dirty_page_t;

/** 检查点类型 */
typedef enum {
    CHECKPOINT_SHARP,  /**< Sharp Checkpoint：刷所有脏页 */
    CHECKPOINT_FUZZY   /**< Fuzzy Checkpoint：只刷检查点开始前的脏页 */
} checkpoint_type_t;

/** 检查点信息 */
typedef struct {
    uint64_t    checkpoint_lsn;  /**< 检查点 LSN */
    uint64_t    prev_lsn;         /**< 上一个检查点 LSN */
    uint32_t    dirty_page_count; /**< 脏页数量 */
    uint64_t    end_lsn;          /**< 检查点结束 LSN（Fuzzy） */
} checkpoint_info_t;

/* ============================================================
 * 模拟 WAL 状态
 * ============================================================ */

static uint64_t g_current_lsn = 0;
static uint64_t g_checkpoint_lsn = 0;
static dirty_page_t g_dirty_pages[16];
static uint32_t g_dirty_count = 0;

/** 模拟分配 LSN */
static uint64_t alloc_lsn(void) {
    return ++g_current_lsn;
}

/* ============================================================
 * Sharp Checkpoint 实现
 * ============================================================ */

/**
 * Sharp Checkpoint：强制刷所有脏页
 *
 * 特点：
 * - 阻塞所有写操作
 * - 必须等待所有脏页刷盘完成
 * - 恢复时从检查点开始，无需扫描日志
 * - 耗时但保证一致性
 */
static checkpoint_info_t do_sharp_checkpoint(void) {
    printf("[checkpoint] === Sharp Checkpoint 开始 ===\n");
    printf("[checkpoint] 当前 LSN: %llu\n", g_current_lsn);
    printf("[checkpoint] 脏页数量: %u\n", g_dirty_count);

    checkpoint_info_t ckpt = {
        .checkpoint_lsn = g_current_lsn + 1,
        .prev_lsn = g_checkpoint_lsn,
        .dirty_page_count = g_dirty_count,
        .end_lsn = 0
    };

    /* 刷所有脏页 */
    for (uint32_t i = 0; i < g_dirty_count; i++) {
        printf("[checkpoint] 刷脏页 #%u (rec_lsn=%llu)\n",
               g_dirty_pages[i].page_id, g_dirty_pages[i].rec_lsn);
        g_dirty_pages[i].dirty = false;
    }

    /* 写入检查点记录 */
    ckpt.checkpoint_lsn = alloc_lsn();
    printf("[checkpoint] 写入检查点记录，LSN=%llu\n", ckpt.checkpoint_lsn);

    g_checkpoint_lsn = ckpt.checkpoint_lsn;
    g_dirty_count = 0;

    printf("[checkpoint] === Sharp Checkpoint 完成 ===\n\n");
    return ckpt;
}

/* ============================================================
 * Fuzzy Checkpoint 实现
 * ============================================================ */

/**
 * Fuzzy Checkpoint：只刷检查点开始前的脏页
 *
 * 特点：
 * - 不阻塞新事务
 * - 分为 BEGIN、DO、END 三个阶段
 * - 可能遗漏检查点期间产生的脏页
 * - 恢复时需要扫描检查点之后的日志
 */
static checkpoint_info_t do_fuzzy_checkpoint(void) {
    printf("[checkpoint] === Fuzzy Checkpoint 开始 ===\n");
    printf("[checkpoint] 当前 LSN: %llu, 上次检查点: %llu\n",
           g_current_lsn, g_checkpoint_lsn);

    checkpoint_info_t ckpt = {
        .checkpoint_lsn = 0,
        .prev_lsn = g_checkpoint_lsn,
        .dirty_page_count = g_dirty_count,
        .end_lsn = 0
    };

    /* 阶段1: BEGIN - 记录当前脏页状态 */
    uint32_t pre_checkpoint_dirty = 0;
    for (uint32_t i = 0; i < g_dirty_count; i++) {
        if (g_dirty_pages[i].rec_lsn <= g_current_lsn) {
            pre_checkpoint_dirty++;
        }
    }
    printf("[checkpoint] [BEGIN] 检查点前脏页: %u\n", pre_checkpoint_dirty);

    /* 写入检查点 BEGIN 记录 */
    ckpt.checkpoint_lsn = alloc_lsn();
    printf("[checkpoint] [BEGIN] 检查点记录 LSN=%llu\n", ckpt.checkpoint_lsn);

    /* 阶段2: DO - 只刷检查点开始前的脏页 */
    printf("[checkpoint] [DO] 开始刷脏页...\n");
    uint32_t flushed = 0;
    for (uint32_t i = 0; i < g_dirty_count; i++) {
        if (g_dirty_pages[i].dirty && g_dirty_pages[i].rec_lsn <= ckpt.checkpoint_lsn) {
            printf("[checkpoint] [DO] 刷脏页 #%u (rec_lsn=%llu)\n",
                   g_dirty_pages[i].page_id, g_dirty_pages[i].rec_lsn);
            g_dirty_pages[i].dirty = false;
            flushed++;
        }
    }
    printf("[checkpoint] [DO] 已刷脏页: %u/%u\n", flushed, g_dirty_count);

    /* 阶段3: END - 记录检查点结束状态 */
    ckpt.end_lsn = g_current_lsn;
    printf("[checkpoint] [END] 检查点结束 LSN=%llu\n", ckpt.end_lsn);

    g_checkpoint_lsn = ckpt.checkpoint_lsn;
    printf("[checkpoint] === Fuzzy Checkpoint 完成 ===\n\n");
    return ckpt;
}

/* ============================================================
 * LSN 水位管理
 * ============================================================ */

/** LSN 水位阈值 */
#define LSN_HIGH_WATER 1000
#define LSN_LOW_WATER  500

/**
 * 检查是否需要触发检查点
 */
static bool need_checkpoint_by_lsn(void) {
    uint64_t lsn_distance = g_current_lsn - g_checkpoint_lsn;
    printf("[checkpoint] LSN 水位: 当前=%llu, 检查点=%llu, 距离=%llu\n",
           g_current_lsn, g_checkpoint_lsn, lsn_distance);

    return lsn_distance >= LSN_HIGH_WATER;
}

/**
 * 脏页刷盘策略演示
 */
static void demonstrate_flush_strategy(void) {
    printf("\n[checkpoint] === 脏页刷盘策略演示 ===\n");

    /* 策略1: 全量刷盘（Sharp） */
    printf("\n策略1: 全量刷盘\n");
    printf("  - 适用场景: 停机维护、备份\n");
    printf("  - 优点: 简单一致\n");
    printf("  - 缺点: 阻塞、耗时长\n");

    /* 策略2: 按需刷盘（Fuzzy） */
    printf("\n策略2: 按需刷盘\n");
    printf("  - 适用场景: 正常运行时的定期检查点\n");
    printf("  - 优点: 不阻塞、高可用\n");
    printf("  - 缺点: 需要额外处理检查点期间的脏页\n");

    /* 策略3: LSN 驱动刷盘 */
    printf("\n策略3: LSN 驱动刷盘\n");
    printf("  - 阈值: 高水位=%d, 低水位=%d\n", LSN_HIGH_WATER, LSN_LOW_WATER);
    printf("  - 当 LSN 距离 >= 高水位时触发检查点\n");
    printf("  - 检查点完成后等待降至低水位\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void) {
    printf("========================================\n");
    printf("     Checkpoint 机制演示\n");
    printf("========================================\n\n");

    /* 初始化脏页 */
    g_dirty_count = 4;
    g_dirty_pages[0] = (dirty_page_t){.page_id = 1, .rec_lsn = 100, .dirty = true};
    g_dirty_pages[1] = (dirty_page_t){.page_id = 2, .rec_lsn = 150, .dirty = true};
    g_dirty_pages[2] = (dirty_page_t){.page_id = 3, .rec_lsn = 200, .dirty = true};
    g_dirty_pages[3] = (dirty_page_t){.page_id = 5, .rec_lsn = 180, .dirty = true};

    g_current_lsn = 200;
    g_checkpoint_lsn = 0;

    /* 演示 Sharp Checkpoint */
    printf("\n----------------------------------------\n");
    printf("1. Sharp Checkpoint 演示\n");
    printf("----------------------------------------\n");
    checkpoint_info_t sharp = do_sharp_checkpoint();
    printf("[checkpoint] 结果: LSN=%llu, 刷页数=%u\n\n",
           sharp.checkpoint_lsn, sharp.dirty_page_count);

    /* 重置脏页并模拟新事务 */
    g_dirty_count = 3;
    g_dirty_pages[0] = (dirty_page_t){.page_id = 2, .rec_lsn = 210, .dirty = true};
    g_dirty_pages[1] = (dirty_page_t){.page_id = 4, .rec_lsn = 215, .dirty = true};
    g_dirty_pages[2] = (dirty_page_t){.page_id = 6, .rec_lsn = 220, .dirty = true};
    g_current_lsn = 220;

    /* 演示 Fuzzy Checkpoint */
    printf("----------------------------------------\n");
    printf("2. Fuzzy Checkpoint 演示\n");
    printf("----------------------------------------\n");
    checkpoint_info_t fuzzy = do_fuzzy_checkpoint();
    printf("[checkpoint] 结果: LSN=%llu, END_LSN=%llu\n\n",
           fuzzy.checkpoint_lsn, fuzzy.end_lsn);

    /* 演示 LSN 水位管理 */
    printf("----------------------------------------\n");
    printf("3. LSN 水位管理演示\n");
    printf("----------------------------------------\n");
    g_current_lsn = 1200;
    g_checkpoint_lsn = 100;
    if (need_checkpoint_by_lsn()) {
        printf("[checkpoint] 需要触发检查点！\n");
    } else {
        printf("[checkpoint] 暂不需要检查点\n");
    }

    /* 演示脏页刷盘策略 */
    demonstrate_flush_strategy();

    printf("\n========================================\n");
    printf("     演示结束\n");
    printf("========================================\n");

    return 0;
}
