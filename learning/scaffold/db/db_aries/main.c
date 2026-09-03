/*
 * ARIES 算法演示 - main.c
 *
 * ARIES (Algorithms for Recovery and Isolation Exploiting Semantics) 是 IBM 提出的
 * 数据库故障恢复算法，被 PostgreSQL、MySQL InnoDB 等广泛采用。
 *
 * 核心三阶段：
 *   1. Analysis（分析） - 扫描日志构建 Active Transaction Table 和 Dirty Page Table
 *   2. Redo（重做）    - 从最近的检查点开始重放所有 UPDATE，确保所有页写入磁盘
 *   3. Undo（回滚）    - 回滚未提交事务产生的日志，恢复到事务开始前的状态
 *
 * 本程序通过模拟日志记录、脏页跟踪和恢复流程演示 ARIES 核心机制。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================
 * 常量与全局配置
 * ============================================================ */
#define MAX_TRANSACTIONS   16   /* 最大事务数 */
#define MAX_DIRTY_PAGES    32   /* 脏页表最大条目 */
#define MAX_LOG_RECORDS    128  /* 日志缓冲区最大条数 */

/* ============================================================
 * 数据结构定义
 * ============================================================ */

/* 日志记录类型 */
typedef enum {
    LOG_UPDATE   = 0,   /* 更新日志（包含前后镜像） */
    LOG_COMMIT   = 1,   /* 事务提交 */
    LOG_ABORT    = 2,   /* 事务中止（回滚） */
    LOG_COMPENSATE = 3, /* 补偿日志 CLR（用于 Undo 回溯） */
    LOG_CHECKPOINT = 4  /* 检查点日志 */
} LogType;

/* 日志记录结构 */
typedef struct {
    int   lsn;          /* 日志序列号，递增唯一 */
    int   txn_id;       /* 事务 ID */
    LogType type;       /* 日志类型 */
    int   page_id;      /* 受影响的页 ID */
    int   page_lsn;     /* 页面上次修改的 LSN */
    char  old_val[16]; /* 修改前的值（前镜像） */
    char  new_val[16]; /* 修改后的值（后镜像） */
    int   prev_lsn;     /* 同一事务上一条日志的 LSN */
} LogRecord;

/* 活跃事务表条目 */
typedef struct {
    int   txn_id;       /* 事务 ID */
    int   last_lsn;     /* 该事务最后一条日志的 LSN */
    char  status;       /* 'A'=活跃, 'C'=已提交, 'U'=待回滚 */
} TransactionTableEntry;

/* 脏页表条目 */
typedef struct {
    int   page_id;      /* 页 ID */
    int   rec_lsn;      /* 导致该页变脏的最小 LSN（Recovery LSN） */
    bool  dirty;        /* 是否为脏页 */
} DirtyPageTableEntry;

/* 数据库页模拟 */
typedef struct {
    int   page_id;      /* 页 ID */
    int   page_lsn;     /* 页上记录的最新修改 LSN */
    char  data[64];     /* 页数据 */
} Page;

/* ============================================================
 * 全局状态
 * ============================================================ */
static LogRecord          g_log_buf[MAX_LOG_RECORDS];   /* 日志缓冲区 */
static int                g_log_count  = 0;             /* 已记录日志数 */
static int                g_current_lsn = 0;            /* 当前 LSN 计数器 */

static TransactionTableEntry g_txn_tbl[MAX_TRANSACTIONS]; /* 活跃事务表 */
static int                 g_txn_count = 0;               /* 活跃事务数 */

static DirtyPageTableEntry g_dirty_pages[MAX_DIRTY_PAGES]; /* 脏页表 */
static int                 g_dirty_count = 0;              /* 脏页条目数 */

static Page                g_pages[8];                     /* 模拟数据库页 */
static int                 g_page_count = 4;              /* 初始页数 */

/* ============================================================
 * 辅助函数
 * ============================================================ */

/* 打印分隔线 */
static void print_separator(const char *title) {
    printf("\n");
    printf("============================================================\n");
    printf("  %s\n", title);
    printf("============================================================\n");
}

/* 打印日志记录详情 */
static void print_log_record(const LogRecord *rec) {
    const char *type_name[] = {
        "UPDATE", "COMMIT", "ABORT", "COMPENSATE", "CHECKPOINT"
    };
    printf("  [LSN=%03d] Txn=%d Type=%-10s Page=%d old=%s new=%s prev_lsn=%d\n",
           rec->lsn, rec->txn_id, type_name[rec->type], rec->page_id,
           rec->old_val, rec->new_val, rec->prev_lsn);
}

/* 打印脏页表 */
static void print_dirty_page_table(void) {
    printf("  [脏页表] 条目数=%d\n", g_dirty_count);
    for (int i = 0; i < g_dirty_count; i++) {
        printf("    PageID=%d  rec_lsn=%d\n",
               g_dirty_pages[i].page_id, g_dirty_pages[i].rec_lsn);
    }
}

/* 打印活跃事务表 */
static void print_transaction_table(void) {
    printf("  [活跃事务表] 条目数=%d\n", g_txn_count);
    for (int i = 0; i < g_txn_count; i++) {
        printf("    TxnID=%d  last_lsn=%d  status=%c\n",
               g_txn_tbl[i].txn_id, g_txn_tbl[i].last_lsn, g_txn_tbl[i].status);
    }
}

/* ============================================================
 * 日志记录
 * ============================================================ */

/* 写入一条日志记录 */
static LogRecord *write_log(int txn_id, LogType type, int page_id,
                             const char *old_val, const char *new_val) {
    if (g_log_count >= MAX_LOG_RECORDS) {
        printf("  [错误] 日志缓冲区已满！\n");
        return NULL;
    }

    /* 获取事务上一条 LSN */
    int prev_lsn = -1;
    for (int i = 0; i < g_txn_count; i++) {
        if (g_txn_tbl[i].txn_id == txn_id) {
            prev_lsn = g_txn_tbl[i].last_lsn;
            break;
        }
    }

    /* 分配 LSN */
    int lsn = ++g_current_lsn;

    /* 填充日志记录 */
    LogRecord *rec = &g_log_buf[g_log_count++];
    rec->lsn      = lsn;
    rec->txn_id   = txn_id;
    rec->type     = type;
    rec->page_id  = page_id;
    rec->page_lsn = 0;           /* Redo 时填充 */
    rec->prev_lsn = prev_lsn;
    strncpy(rec->old_val, old_val, 15);
    strncpy(rec->new_val, new_val, 15);
    rec->old_val[15] = '\0';
    rec->new_val[15] = '\0';

    /* 更新脏页表：如果页不在表中则添加 */
    if (type == LOG_UPDATE) {
        bool found = false;
        for (int i = 0; i < g_dirty_count; i++) {
            if (g_dirty_pages[i].page_id == page_id) {
                found = true;
                break;
            }
        }
        if (!found && g_dirty_count < MAX_DIRTY_PAGES) {
            g_dirty_pages[g_dirty_count].page_id = page_id;
            g_dirty_pages[g_dirty_count].rec_lsn = lsn;
            g_dirty_pages[g_dirty_count].dirty   = true;
            g_dirty_count++;
        }
    }

    /* 更新事务表 */
    for (int i = 0; i < g_txn_count; i++) {
        if (g_txn_tbl[i].txn_id == txn_id) {
            g_txn_tbl[i].last_lsn = lsn;
            return rec;
        }
    }
    /* 新事务加入表 */
    if (g_txn_count < MAX_TRANSACTIONS) {
        g_txn_tbl[g_txn_count].txn_id  = txn_id;
        g_txn_tbl[g_txn_count].last_lsn = lsn;
        g_txn_tbl[g_txn_count].status   = 'A';
        g_txn_count++;
    }
    return rec;
}

/* ============================================================
 * ARIES 核心算法实现
 * ============================================================ */

/**
 * ARIES 分析阶段（Analysis）
 *
 * 扫描日志文件（这里是从头扫描模拟的日志缓冲区），重建：
 *   - Active Transaction Table（活跃事务表）：哪些事务尚未提交
 *   - Dirty Page Table（脏页表）：哪些页被修改但未刷盘
 *
 * 分析阶段从最近的检查点开始向后扫描，直到日志末尾。
 * 扫描过程中：
 *   - 遇到 UPDATE 记录：将对应事务加入/更新活跃事务表
 *   - 遇到 COMMIT 记录：从活跃事务表中移除该事务
 *   - 遇到 ABORT 记录：标记该事务为待回滚
 *
 * @param checkpoint_lsn 检查点之后的起始 LSN
 * @param out_active_txns 输出：活跃事务数
 */
static void aries_analysis(int checkpoint_lsn, int *out_active_txns) {
    printf("\n[阶段 1] ARIES 分析阶段\n");
    printf("  扫描起点: LSN=%d\n", checkpoint_lsn);

    /* 清空分析阶段重建的表 */
    g_txn_count   = 0;
    g_dirty_count = 0;

    /* 扫描日志：跳过检查点之前的记录 */
    for (int i = 0; i < g_log_count; i++) {
        LogRecord *rec = &g_log_buf[i];
        if (rec->lsn < checkpoint_lsn) continue;

        switch (rec->type) {
            case LOG_UPDATE:
            case LOG_COMPENSATE: {
                /* 更新或补偿日志：将事务标记为活跃（若未在表中） */
                bool found = false;
                for (int j = 0; j < g_txn_count; j++) {
                    if (g_txn_tbl[j].txn_id == rec->txn_id) {
                        g_txn_tbl[j].last_lsn = rec->lsn;
                        found = true;
                        break;
                    }
                }
                if (!found && g_txn_count < MAX_TRANSACTIONS) {
                    g_txn_tbl[g_txn_count].txn_id   = rec->txn_id;
                    g_txn_tbl[g_txn_count].last_lsn = rec->lsn;
                    g_txn_tbl[g_txn_count].status   = 'U'; /* 待回滚 */
                    g_txn_count++;
                }

                /* 更新脏页表（Analysis 阶段也重建脏页表） */
                bool page_found = false;
                for (int j = 0; j < g_dirty_count; j++) {
                    if (g_dirty_pages[j].page_id == rec->page_id) {
                        /* rec_lsn 取最小值 */
                        if (rec->lsn < g_dirty_pages[j].rec_lsn) {
                            g_dirty_pages[j].rec_lsn = rec->lsn;
                        }
                        page_found = true;
                        break;
                    }
                }
                if (!page_found && g_dirty_count < MAX_DIRTY_PAGES) {
                    g_dirty_pages[g_dirty_count].page_id = rec->page_id;
                    g_dirty_pages[g_dirty_count].rec_lsn = rec->lsn;
                    g_dirty_pages[g_dirty_count].dirty   = true;
                    g_dirty_count++;
                }
                break;
            }
            case LOG_COMMIT:
                /* 提交：从事务表移除 */
                for (int j = 0; j < g_txn_count; j++) {
                    if (g_txn_tbl[j].txn_id == rec->txn_id) {
                        g_txn_tbl[j].status = 'C'; /* 已提交 */
                        /* 从表中删除（压缩） */
                        for (int k = j; k < g_txn_count - 1; k++) {
                            g_txn_tbl[k] = g_txn_tbl[k + 1];
                        }
                        g_txn_count--;
                        break;
                    }
                }
                break;
            case LOG_ABORT:
                /* 中止：标记为待回滚 */
                for (int j = 0; j < g_txn_count; j++) {
                    if (g_txn_tbl[j].txn_id == rec->txn_id) {
                        g_txn_tbl[j].status = 'U';
                        break;
                    }
                }
                break;
            default:
                break;
        }
    }

    /* 移除已提交事务（只剩下 U=待回滚） */
    int write_idx = 0;
    for (int i = 0; i < g_txn_count; i++) {
        if (g_txn_tbl[i].status == 'U') {
            g_txn_tbl[write_idx++] = g_txn_tbl[i];
        }
    }
    g_txn_count = write_idx;

    printf("  分析结果:\n");
    print_transaction_table();
    print_dirty_page_table();
    *out_active_txns = g_txn_count;
}

/**
 * ARIES 重做阶段（Redo）
 *
 * 从脏页表中 rec_lsn 最小的位置开始，逐条扫描 UPDATE 日志：
 *   - 如果页的 page_lsn < 日志的 lsn，说明页未包含这次更新，需要重做
 *   - 重做时：修改页数据，更新页的 page_lsn
 *
 * 重做阶段的边界：只需要重做到日志序列的末尾即可。
 *
 * @param last_check_lsn 最近检查点的 LSN
 */
static void aries_redo(int last_check_lsn) {
    (void)last_check_lsn;  /* Redo 从脏页表 rec_lsn 驱动，检查点 LSN 在此不直接使用 */
    printf("\n[阶段 2] ARIES 重做阶段\n");
    printf("  重做起点: 从脏页表中 rec_lsn 最小的位置开始\n");

    int redo_count = 0;

    /* 遍历所有日志记录 */
    for (int i = 0; i < g_log_count; i++) {
        LogRecord *rec = &g_log_buf[i];
        if (rec->type != LOG_UPDATE) continue;

        /* 找到对应页 */
        Page *page = NULL;
        for (int j = 0; j < g_page_count; j++) {
            if (g_pages[j].page_id == rec->page_id) {
                page = &g_pages[j];
                break;
            }
        }
        if (!page) continue;

        /* 判断是否需要重做：页 LSN < 日志 LSN */
        if (page->page_lsn < rec->lsn) {
            printf("  [Redo] LSN=%03d: Txn=%d Page=%d 将 data 设为 '%s'\n",
                   rec->lsn, rec->txn_id, rec->page_id, rec->new_val);
            strncpy(page->data, rec->new_val, 63);
            page->data[63] = '\0';
            page->page_lsn = rec->lsn;
            redo_count++;
        } else {
            printf("  [Skip] LSN=%03d: Page=%d 已有此更新（page_lsn=%d >= %d）\n",
                   rec->lsn, rec->page_id, page->page_lsn, rec->lsn);
        }
    }
    printf("  重做完成: 共重做了 %d 条 UPDATE 日志\n", redo_count);
}

/**
 * ARIES 回滚阶段（Undo）
 *
 * 对于 Analysis 阶段发现的未提交事务（status='U'），沿 prev_lsn 链回溯，
 * 逐条执行反向操作，将数据恢复到 old_val。
 * 每回滚一条 UPDATE 就写入一条 CLR（Compensation Log Record）。
 *
 * @param active_txns 活跃事务数
 */
static void aries_undo(int active_txns) {
    printf("\n[阶段 3] ARIES 回滚阶段\n");

    if (active_txns == 0) {
        printf("  无未提交事务，回滚阶段无需执行\n");
        return;
    }

    int undo_count = 0;

    /* 遍历所有需要回滚的事务 */
    for (int t = 0; t < active_txns; t++) {
        int txn_id = g_txn_tbl[t].txn_id;
        printf("  正在回滚事务 TxnID=%d\n", txn_id);

        /* 从该事务最后一条日志开始，沿 prev_lsn 链回溯 */
        int cur_lsn = g_txn_tbl[t].last_lsn;
        while (cur_lsn > 0) {
            /* 在日志缓冲区中找到对应记录 */
            LogRecord *rec = NULL;
            for (int i = 0; i < g_log_count; i++) {
                if (g_log_buf[i].lsn == cur_lsn && g_log_buf[i].txn_id == txn_id) {
                    rec = &g_log_buf[i];
                    break;
                }
            }
            if (!rec) break;

            /* 只回滚 UPDATE 类型 */
            if (rec->type == LOG_UPDATE) {
                /* 找到页并回滚数据 */
                for (int j = 0; j < g_page_count; j++) {
                    if (g_pages[j].page_id == rec->page_id) {
                        printf("  [Undo] LSN=%03d: Page=%d 恢复 data 为 '%s'\n",
                               rec->lsn, rec->page_id, rec->old_val);
                        strncpy(g_pages[j].data, rec->old_val, 63);
                        g_pages[j].data[63] = '\0';
                        g_pages[j].page_lsn = ++g_current_lsn;
                        break;
                    }
                }

                /* 写入 CLR（补偿日志记录） */
                LogRecord *clr = write_log(txn_id, LOG_COMPENSATE,
                                            rec->page_id, "", "");
                printf("  [CLR]   生成补偿日志 LSN=%03d\n", clr->lsn);
                undo_count++;
            }

            /* 沿 prev_lsn 链上移 */
            cur_lsn = rec->prev_lsn;
        }

        /* 写入 ABORT 日志 */
        write_log(txn_id, LOG_ABORT, -1, "", "");
        printf("  [Abort] TxnID=%d 已回滚完毕\n", txn_id);
    }

    printf("  回滚完成: 共回滚了 %d 条 UPDATE 日志\n", undo_count);
}

/* ============================================================
 * 测试场景：构建日志 + 执行恢复
 * ============================================================ */

/* 模拟页面初始化 */
static void init_pages(void) {
    for (int i = 0; i < g_page_count; i++) {
        g_pages[i].page_id = i;
        g_pages[i].page_lsn = 0;
        snprintf(g_pages[i].data, sizeof(g_pages[i].data), "page_init_%d", i);
    }
    printf("初始化 %d 个页面\n", g_page_count);
}

/* 模拟一个简单的事务操作 */
static void simulate_transaction(int txn_id, int page_id,
                                  const char *old_val, const char *new_val) {
    printf("  Txn=%d UPDATE Page=%d: '%s' -> '%s'\n",
           txn_id, page_id, old_val, new_val);
    write_log(txn_id, LOG_UPDATE, page_id, old_val, new_val);

    /* 更新内存中的页（模拟部分修改） */
    for (int i = 0; i < g_page_count; i++) {
        if (g_pages[i].page_id == page_id) {
            strncpy(g_pages[i].data, new_val, 63);
            g_pages[i].data[63] = '\0';
            break;
        }
    }
}

int main(void) {
    printf("\n");
    printf("############################################################\n");
    printf("#                                                          #\n");
    printf("#         ARIES 算法演示 - 三阶段恢复流程                   #\n");
    printf("#                                                          #\n");
    printf("#  ARIES = Algorithms for Recovery and Isolation           #\n");
    printf("#           Exploiting Semantics                           #\n");
    printf("#                                                          #\n");
    printf("#  IBM Research, 1992.                                     #\n");
    printf("#  被 PostgreSQL / MySQL InnoDB / SQL Server 等采用。        #\n");
    printf("#                                                          #\n");
    printf("############################################################\n");

    /* 0. 初始化模拟存储 */
    print_separator("初始化");
    init_pages();

    /* 1. 模拟正常操作阶段：构造日志 */
    print_separator("阶段 0: 模拟事务操作（构造日志）");

    printf("--- 正常提交的事务 ---\n");
    simulate_transaction(1, 0, "page_init_0", "txn1_val_A");
    simulate_transaction(1, 1, "page_init_1", "txn1_val_B");
    write_log(1, LOG_COMMIT, -1, "", ""); /* Txn1 提交 */

    printf("\n--- 未提交的事务 Txn2（在故障前未提交）---\n");
    simulate_transaction(2, 0, "txn1_val_A", "txn2_val_A");  /* 覆盖 Txn1 的修改 */
    simulate_transaction(2, 2, "page_init_2", "txn2_val_C");

    /* 2. 模拟检查点：写在 Txn2 进行中，尚未提交 */
    print_separator("模拟检查点（故障前）");
    LogRecord *ckpt = write_log(0, LOG_CHECKPOINT, -1, "", "");
    int checkpoint_lsn = ckpt->lsn;
    printf("检查点 LSN=%d 写入日志缓冲区\n", checkpoint_lsn);
    printf("  检查点时活跃事务: Txn2（尚未提交）\n");

    /* 3. 检查点后 Txn2 继续操作（这些操作在故障前未刷盘） */
    printf("\n--- 检查点后 Txn2 继续操作（故障发生在这些操作之后）---\n");
    simulate_transaction(2, 1, "txn1_val_B", "txn2_val_B");  /* Txn2 再次修改 Page1 */
    simulate_transaction(2, 3, "page_init_3", "txn2_val_D"); /* Txn2 修改 Page3 */

    /* 模拟故障发生在此处 */

    /* 打印日志缓冲区 */
    printf("\n--- 当前日志缓冲区（共 %d 条）---\n", g_log_count);
    for (int i = 0; i < g_log_count; i++) {
        print_log_record(&g_log_buf[i]);
    }

    /* 打印活跃事务表（故障前） */
    printf("\n--- 故障前活跃事务表 ---\n");
    print_transaction_table();

    /* 打印脏页表（故障前） */
    printf("\n--- 故障前脏页表 ---\n");
    print_dirty_page_table();

    /* 打印页面状态（故障前） */
    printf("\n--- 故障前页面状态 ---\n");
    for (int i = 0; i < g_page_count; i++) {
        printf("  PageID=%d  page_lsn=%03d  data='%s'\n",
               g_pages[i].page_id, g_pages[i].page_lsn, g_pages[i].data);
    }

    /* 4. 执行 ARIES 三阶段恢复 */
    print_separator("阶段 1-3: ARIES 恢复流程");

    int active_txns = 0;

    /* 阶段 1: 分析 - 从检查点开始扫描，重建 ATT 和 DPT */
    aries_analysis(checkpoint_lsn, &active_txns);

    /* 阶段 2: 重做 */
    aries_redo(checkpoint_lsn);

    /* 阶段 3: 回滚 */
    aries_undo(active_txns);

    /* 4. 恢复后验证 */
    print_separator("恢复后页面状态验证");
    printf("  预期结果:\n");
    printf("    Page0: txn1_val_A (Txn1 提交，最终值)\n");
    printf("    Page1: txn1_val_B (Txn1 提交)\n");
    printf("    Page2: page_init_2 (Txn2 未提交，应被回滚)\n");
    printf("    Page3: txn3_val_D (Txn3 提交)\n");
    printf("\n  实际结果:\n");
    for (int i = 0; i < g_page_count; i++) {
        printf("    PageID=%d  data='%s'\n",
               g_pages[i].page_id, g_pages[i].data);
    }

    /* 5. 打印恢复后的完整日志 */
    print_separator("恢复后完整日志（含 CLR）");
    printf("  日志总数: %d 条\n", g_log_count);
    for (int i = 0; i < g_log_count; i++) {
        print_log_record(&g_log_buf[i]);
    }

    printf("\n");
    printf("############################################################\n");
    printf("#  ARIES 恢复流程演示完毕                                   #\n");
    printf("#  关键点回顾:                                              #\n");
    printf("#    - Analysis: 扫描日志重建事务表和脏页表                 #\n");
    printf("#    - Redo:     从 rec_lsn 起重放所有未刷盘的更新           #\n");
    printf("#    - Undo:     沿 prev_lsn 链回滚未提交事务               #\n");
    printf("#    - CLR:      每次 Undo 写入补偿日志，保证可恢复性        #\n");
    printf("############################################################\n");
    return 0;
}
