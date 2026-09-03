/**
 * Redo Log 详解演示
 *
 * 演示 Redo Log 的核心概念：
 * - Redo Log 格式（LSN/PageID/操作类型/数据）
 * - 检查点（Checkpoint）机制
 * - 崩溃恢复流程（从检查点重放）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ========== Redo Log 记录结构 ========== */
typedef struct {
    int lsn;                /* Log Sequence Number */
    int page_id;            /* 页面 ID */
    char op_type;           /* 操作类型：I=Insert, U=Update, D=Delete */
    void *page_data;        /* 页面数据 */
    int data_len;           /* 数据长度 */
    int txn_id;             /* 事务 ID */
} RedoLogRecord;

/* ========== 脏页表条目 ========== */
typedef struct {
    int page_id;
    int rec_lsn;            /* 最近修改的 LSN */
    bool dirty;
} DirtyPageEntry;

/* ========== 检查点信息 ========== */
typedef struct {
    int checkpoint_lsn;     /* 检查点 LSN */
    int active_txn_count;   /* 活跃事务数 */
    int *active_txns;       /* 活跃事务 ID 列表 */
    int dirty_page_count;   /* 脏页数 */
    int *dirty_pages;       /* 脏页 ID 列表 */
} CheckpointInfo;

/* ========== 全局状态 ========== */
RedoLogRecord redo_log[100];
int redo_log_count = 0;
int current_lsn = 1;
DirtyPageEntry dirty_pages[50];
int dirty_page_count = 0;
CheckpointInfo last_checkpoint = {0, 0, NULL, 0, NULL};

/* ========== 模拟数据页 ========== */
typedef struct {
    int page_id;
    char data[256];
} DataPage;

DataPage pages[10];

/* ========== 写入 Redo Log ========== */
int write_redo_log(int page_id, char op_type, void *data, int len, int txn_id) {
    redo_log[redo_log_count].lsn = current_lsn++;
    redo_log[redo_log_count].page_id = page_id;
    redo_log[redo_log_count].op_type = op_type;
    redo_log[redo_log_count].page_data = malloc(len);
    memcpy(redo_log[redo_log_count].page_data, data, len);
    redo_log[redo_log_count].data_len = len;
    redo_log[redo_log_count].txn_id = txn_id;

    /* 更新脏页表 */
    bool found = false;
    for (int i = 0; i < dirty_page_count; i++) {
        if (dirty_pages[i].page_id == page_id) {
            dirty_pages[i].rec_lsn = redo_log[redo_log_count].lsn;
            dirty_pages[i].dirty = true;
            found = true;
            break;
        }
    }
    if (!found && dirty_page_count < 50) {
        dirty_pages[dirty_page_count].page_id = page_id;
        dirty_pages[dirty_page_count].rec_lsn = redo_log[redo_log_count].lsn;
        dirty_pages[dirty_page_count].dirty = true;
        dirty_page_count++;
    }

    printf("[RedoLog] LSN=%d, Page=%d, Op=%c, Txn=%d\n",
           redo_log[redo_log_count].lsn, page_id, op_type, txn_id);

    redo_log_count++;
    return redo_log[redo_log_count - 1].lsn;
}

/* ========== 创建检查点 ========== */
void create_checkpoint(void) {
    printf("\n[Checkpoint] 创建检查点 at LSN=%d\n", current_lsn - 1);

    last_checkpoint.checkpoint_lsn = current_lsn - 1;
    last_checkpoint.dirty_page_count = dirty_page_count;
    last_checkpoint.dirty_pages = malloc(sizeof(int) * dirty_page_count);

    for (int i = 0; i < dirty_page_count; i++) {
        last_checkpoint.dirty_pages[i] = dirty_pages[i].page_id;
        printf("  脏页: PageID=%d, REC_LSN=%d\n",
               dirty_pages[i].page_id, dirty_pages[i].rec_lsn);
    }

    /* 写入检查点日志 */
    redo_log[redo_log_count].lsn = current_lsn++;
    redo_log[redo_log_count].page_id = -1;  /* 检查点记录 */
    redo_log[redo_log_count].op_type = 'C';
    redo_log[redo_log_count].txn_id = 0;
    redo_log_count++;

    printf("[Checkpoint] 检查点创建完成\n\n");
}

/* ========== 崩溃恢复 ========== */
void crash_recovery(void) {
    printf("\n========== 崩溃恢复 ==========\n");
    printf("系统崩溃！从检查点 LSN=%d 恢复...\n\n", last_checkpoint.checkpoint_lsn);

    /* 分析阶段：确定需要重做的页面 */
    printf("[分析] 确定需要重做的脏页:\n");
    for (int i = 0; i < dirty_page_count; i++) {
        printf("  脏页 PageID=%d, REC_LSN=%d\n",
               dirty_pages[i].page_id, dirty_pages[i].rec_lsn);
    }

    /* 重做阶段：从检查点重放 Redo Log */
    printf("\n[重做] 重放 Redo Log:\n");
    int redo_count = 0;
    for (int i = 0; i < redo_log_count; i++) {
        if (redo_log[i].lsn > last_checkpoint.checkpoint_lsn &&
            redo_log[i].op_type != 'C') {
            printf("  重做 LSN=%d: PageID=%d, Op=%c\n",
                   redo_log[i].lsn, redo_log[i].page_id, redo_log[i].op_type);
            redo_count++;
        }
    }
    printf("  共重做 %d 条记录\n", redo_count);

    /* 清理脏页标记 */
    for (int i = 0; i < dirty_page_count; i++) {
        dirty_pages[i].dirty = false;
    }
    dirty_page_count = 0;

    printf("\n[恢复] 恢复完成！\n");
}

/* ========== 测试场景 ========== */
void test_redo_log(void) {
    printf("========== 测试1: 正常操作 + Redo Log ==========\n");
    printf("初始化页面...\n");
    for (int i = 0; i < 3; i++) {
        pages[i].page_id = i + 1;
        sprintf(pages[i].data, "Page %d initial data", i + 1);
    }

    printf("\n执行事务 T1（更新页面 1）:\n");
    write_redo_log(1, 'U', "Updated page 1", 18, 1);

    printf("\n执行事务 T2（更新页面 2）:\n");
    write_redo_log(2, 'U', "Updated page 2", 18, 2);

    printf("\n执行事务 T3（插入页面 3）:\n");
    write_redo_log(3, 'I', "New page 3 data", 18, 3);

    printf("\n当前 Redo Log: %d 条记录\n", redo_log_count);
}

void test_checkpoint_and_recovery(void) {
    printf("\n========== 测试2: 检查点 + 崩溃恢复 ==========\n");

    /* 继续添加日志 */
    printf("\n继续操作...\n");
    write_redo_log(1, 'U', "Another update", 15, 4);
    write_redo_log(2, 'U', "Another update", 15, 4);

    /* 创建检查点 */
    create_checkpoint();

    /* 继续操作（检查点之后的日志） */
    printf("检查点之后继续操作...\n");
    write_redo_log(1, 'U', "Post-checkpoint update", 23, 5);

    /* 模拟崩溃 */
    printf("\n[模拟] 系统崩溃！\n");

    /* 恢复 */
    crash_recovery();
}

/* ========== 主函数 ========== */
int main(void) {
    printf("========================================\n");
    printf("     Redo Log 详解演示\n");
    printf("========================================\n\n");

    test_redo_log();
    test_checkpoint_and_recovery();

    printf("\n========================================\n");
    printf("Redo Log 要点总结:\n");
    printf("  1. LSN 唯一标识每条日志记录\n");
    printf("  2. 每个脏页记录 REC_LSN（最近修改的 LSN）\n");
    printf("  3. 检查点记录当前状态，加速恢复\n");
    printf("  4. 恢复时从检查点重放 Redo Log\n");
    printf("  5. WAL 原则：数据修改先写日志\n");
    printf("========================================\n");

    return 0;
}
