/**
 * db_undo_log/main.c
 * Undo Log 学习演示：模拟 Undo Log 结构、回滚操作与页面恢复
 *
 * 编译：gcc -std=c11 -Wall -o undo_log main.c
 * 运行：./undo_log
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────
 * Undo Log 记录结构（简化版，参考 engineering/src/db/concurrency/mvcc_version.c）
 * ───────────────────────────────────────────────────────────────── */

/* 操作类型枚举 */
typedef enum {
    UNDO_INSERT = 1,   /* 插入操作的回滚：删除该行 */
    UNDO_UPDATE = 2,   /* 更新操作的回滚：恢复旧值 */
    UNDO_DELETE = 3    /* 删除操作的回滚：恢复该行 */
} undo_type_t;

/* Undo Log 记录结构 */
typedef struct {
    uint32_t    txn_id;         /* 事务 ID */
    undo_type_t type;           /* 操作类型 */
    uint64_t    page_id;        /* 所属页面号 */
    uint32_t    offset;         /* 在页面中的偏移量 */
    void       *old_data;       /* 旧数据副本（UPDATE/DELETE 时有效） */
    size_t      old_data_size;  /* 旧数据大小 */
} undo_record_t;

/* 模拟的数据页面（简化版，4KB） */
#define PAGE_SIZE 4096
#define MAX_RECORDS 64

typedef struct {
    uint8_t data[PAGE_SIZE];    /* 页面数据 */
    int is_dirty;               /* 是否脏页 */
} page_t;

/* Undo Log 链表头 */
typedef struct {
    undo_record_t *records[MAX_RECORDS];
    int count;
} undo_log_t;

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

/* 安全打印字符串（处理无 null 终止的情况） */
void print_string_at(const char *label, uint8_t *data, uint32_t offset, size_t max_len)
{
    char buf[64] = {0};
    size_t i = 0;
    while (i < max_len && data[offset + i] != 0 && i < sizeof(buf) - 1) {
        buf[i] = data[offset + i];
        i++;
    }
    printf("%s: \"%s\"\n", label, buf);
}

/* ─────────────────────────────────────────────────────────────────
 * Undo Log 操作函数
 * ───────────────────────────────────────────────────────────────── */

/* 创建 Undo Log 记录 */
undo_record_t *undo_create(uint32_t txn_id, undo_type_t type,
                           uint64_t page_id, uint32_t offset,
                           const void *old_data, size_t old_data_size)
{
    undo_record_t *rec = (undo_record_t *)malloc(sizeof(undo_record_t));
    if (rec == NULL) return NULL;

    rec->txn_id = txn_id;
    rec->type = type;
    rec->page_id = page_id;
    rec->offset = offset;

    if (old_data != NULL && old_data_size > 0) {
        rec->old_data = malloc(old_data_size);
        if (rec->old_data == NULL) {
            free(rec);
            return NULL;
        }
        memcpy(rec->old_data, old_data, old_data_size);
        rec->old_data_size = old_data_size;
    } else {
        rec->old_data = NULL;
        rec->old_data_size = 0;
    }

    return rec;
}

/* 销毁 Undo Log 记录 */
void undo_destroy(undo_record_t *rec)
{
    if (rec == NULL) return;
    free(rec->old_data);
    free(rec);
}

/* 模拟在页面中写入数据（返回写入位置的偏移量） */
uint32_t page_write(page_t *page, const void *data, size_t size)
{
    static uint32_t next_offset = sizeof(uint32_t);  /* 跳过头部空间 */
    uint32_t offset = next_offset;

    if (offset + size > PAGE_SIZE - sizeof(uint32_t)) {
        return 0;  /* 页面空间不足 */
    }

    memcpy(page->data + offset, data, size);
    page->is_dirty = 1;

    /* 更新记录数 */
    *(uint32_t *)page->data += 1;
    next_offset += size;

    return offset;
}

/* ─────────────────────────────────────────────────────────────────
 * 页面恢复演示
 * ───────────────────────────────────────────────────────────────── */

/* 模拟页面损坏（将某位置数据清零） */
void simulate_page_corruption(page_t *page, uint32_t offset, size_t size)
{
    printf("  [模拟故障] 页面损坏，偏移 %u 处 %zu 字节数据丢失\n", offset, size);
    memset(page->data + offset, 0, size);
}

/* 从 Undo Log 恢复页面数据（参考 engineering/src/db/concurrency/mvcc_version.c） */
int page_recover_from_undo(page_t *page, undo_record_t *rec)
{
    printf("  [恢复] 事务 %u, 类型: ", rec->txn_id);

    switch (rec->type) {
        case UNDO_INSERT:
            printf("INSERT 回滚（删除行）\n");
            /* INSERT 回滚：物理删除该行（这里用清零模拟） */
            memset(page->data + rec->offset, 0, 8);
            break;

        case UNDO_UPDATE:
            printf("UPDATE 回滚（恢复旧值）\n");
            /* UPDATE 回滚：用 old_data 覆盖当前行 */
            if (rec->old_data != NULL && rec->old_data_size > 0) {
                memcpy(page->data + rec->offset, rec->old_data, rec->old_data_size);
                printf("    恢复数据: %.*s\n", (int)rec->old_data_size, (char *)rec->old_data);
            }
            break;

        case UNDO_DELETE:
            printf("DELETE 回滚（恢复行）\n");
            /* DELETE 回滚：用 old_data 重建该行 */
            if (rec->old_data != NULL && rec->old_data_size > 0) {
                memcpy(page->data + rec->offset, rec->old_data, rec->old_data_size);
                printf("    恢复数据: %.*s\n", (int)rec->old_data_size, (char *)rec->old_data);
            }
            break;

        default:
            printf("未知类型\n");
            return -1;
    }

    page->is_dirty = 1;
    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * 测试场景
 * ───────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("========================================\n");
    printf("Undo Log 学习演示：回滚操作与页面恢复\n");
    printf("========================================\n\n");

    /* 初始化页面和 Undo Log */
    page_t page = {0};
    undo_log_t undo_log = {0};

    /* ── 场景 1: 插入并记录 Undo ── */
    printf("[场景 1] 插入操作与 Undo 记录\n");
    printf("----------------------------------------\n");

    const char *name1 = "Alice";
    uint32_t offset1 = page_write(&page, name1, strlen(name1));
    printf("  插入数据: \"%s\"，页面偏移: %u\n", name1, offset1);

    /* 记录 INSERT 的 Undo（用于回滚） */
    undo_log.records[undo_log.count++] = undo_create(
        100, UNDO_INSERT, 1, offset1, NULL, 0);
    printf("  记录 Undo: 事务 100, INSERT 回滚\n\n");

    /* ── 场景 2: 更新并记录 Undo ── */
    printf("[场景 2] 更新操作与 Undo 记录\n");
    printf("----------------------------------------\n");

    const char *name2 = "Bob";
    uint32_t offset2 = page_write(&page, name2, strlen(name2));
    printf("  插入数据: \"%s\"，页面偏移: %u\n", name2, offset2);

    /* 模拟更新：保存旧值到 Undo */
    char old_name[16] = "Charlie";
    undo_log.records[undo_log.count++] = undo_create(
        101, UNDO_UPDATE, 1, offset2, old_name, strlen(old_name));
    printf("  记录 Undo: 事务 101, UPDATE 回滚（旧值: \"%s\"）\n\n", old_name);

    /* 执行更新操作（覆盖为新值） */
    const char *new_name = "David";
    memcpy(page.data + offset2, new_name, strlen(new_name));
    page.is_dirty = 1;
    printf("  执行更新: \"%s\" -> \"%s\"\n\n", old_name, new_name);

    /* ── 场景 3: 模拟故障回滚 ── */
    printf("[场景 3] 模拟故障与回滚恢复\n");
    printf("----------------------------------------\n");

    /* 模拟页面损坏 */
    simulate_page_corruption(&page, offset2, strlen(new_name));

    /* 查找对应事务的 Undo 并恢复 */
    for (int i = 0; i < undo_log.count; i++) {
        undo_record_t *rec = undo_log.records[i];
        if (rec->txn_id == 101 && rec->page_id == 1) {
            page_recover_from_undo(&page, rec);
            break;
        }
    }

    /* 验证恢复结果 */
    printf("  验证恢复:");
    print_string_at("页面偏移", page.data, offset2, 8);

    /* ── 场景 4: 删除并回滚 ── */
    printf("\n[场景 4] 删除操作与 Undo 回滚\n");
    printf("----------------------------------------\n");

    const char *name3 = "Eve";
    /* 使用固定偏移，避免覆盖问题 */
    uint32_t offset3 = 256;
    memcpy(page.data + offset3, name3, strlen(name3));
    page.is_dirty = 1;
    printf("  插入数据: \"%s\"，页面偏移: %u\n", name3, offset3);

    /* 记录 DELETE 的 Undo */
    char backup_eve[16] = "Eve";
    size_t name3_len = strlen(name3);
    undo_log.records[undo_log.count++] = undo_create(
        102, UNDO_DELETE, 1, offset3, backup_eve, name3_len);
    printf("  记录 Undo: 事务 102, DELETE 回滚\n");

    /* 执行删除（清零指定长度） */
    memset(page.data + offset3, 0, name3_len);
    printf("  执行删除: 数据已清零（%zu 字节）\n", name3_len);

    /* 回滚恢复 */
    printf("  回滚删除:\n");
    for (int i = 0; i < undo_log.count; i++) {
        undo_record_t *rec = undo_log.records[i];
        if (rec->txn_id == 102 && rec->page_id == 1) {
            page_recover_from_undo(&page, rec);
            break;
        }
    }

    printf("  验证恢复:");
    print_string_at("页面偏移", page.data, offset3, 8);

    /* ── 清理资源 ── */
    printf("========================================\n");
    printf("清理 Undo Log 资源...\n");
    for (int i = 0; i < undo_log.count; i++) {
        undo_destroy(undo_log.records[i]);
    }
    printf("完成，共处理 %d 条 Undo 记录\n", undo_log.count);

    return 0;
}
