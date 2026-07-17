/**
 * @file main.c
 * @brief Heap 堆表存储演示程序
 *
 * 演示内容：
 * 1. 堆表页面结构（PageHeader + tuples）
 * 2. 元组插入（找空闲空间）
 * 3. 元组删除（打墓碑）
 * 4. TID（Tuple ID）定位
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 页面常量（与 engineering/include/db/heapam.h 保持一致）
 * ============================================================ */

#define HEAP_PAGE_SIZE       8192    /* 页面大小 8KB */
#define SizeOfPageHeaderData 24      /* 页面头部大小 */
#define SizeOfHeapLinePointer 6      /* LinePointer 大小 */

/* ============================================================
 * 页面头部结构
 * ============================================================ */

/** @brief 页面头部 */
typedef struct PageHeaderData {
    uint32_t    pd_lsn;           /* 最后修改的 LSN */
    uint16_t    pd_checksum;      /* 页面校验和 */
    uint16_t    pd_flags;         /* 页面标志 */
    uint16_t    pd_lower;         /* 空闲空间起始位置 */
    uint16_t    pd_upper;         /* 空闲空间结束位置 */
    uint16_t    pd_special;       /* 特殊空间起始位置 */
    uint16_t    pd_pagesize_version; /* 页面大小和版本 */
    uint32_t    pd_prune_xid;     /* 可清理的最早事务 ID */
    uint16_t    pd_xid_base;      /* 事务 ID 基准 */
    uint16_t    pd_multi_base;    /* 多事务 ID 基准 */
} PageHeaderData;

/** 页面头部指针 */
typedef PageHeaderData *PageHeader;

/** @brief LinePointer（指向元组） */
typedef struct HeapLinePointerData {
    uint32_t    t_off;            /* 元组在页面中的偏移 */
    uint8_t     t_flags;          /* 标志 */
    uint8_t     t_xmax;           /* 删除事务 ID */
} HeapLinePointerData;

typedef HeapLinePointerData *HeapLinePointer;

/** @brief TID（元组标识符） */
typedef struct ItemPointerData {
    uint32_t    ip_blkno;         /* 块号 */
    uint16_t    ip_offset;        /* LinePointer 偏移 */
} ItemPointerData;

typedef ItemPointerData *ItemPointer;

/* TID 存储数组（最多 1000 个） */
static ItemPointerData tid_array[1000];
static int tid_count = 0;

/* ============================================================
 * 页面操作函数
 * ============================================================ */

/**
 * @brief 初始化页面
 */
void page_init(void *page, size_t size) {
    PageHeader ph = (PageHeader)page;
    ph->pd_lsn = 0;
    ph->pd_checksum = 0;
    ph->pd_flags = 0;
    ph->pd_lower = SizeOfPageHeaderData;  /* LinePointer 数组起始 */
    ph->pd_upper = size;                   /* 空闲空间结束 */
    ph->pd_special = size;
    ph->pd_pagesize_version = 0x0001;
    ph->pd_prune_xid = 0;
    ph->pd_xid_base = 0;
    ph->pd_multi_base = 0;
}

/**
 * @brief 获取页面空闲空间
 */
int page_get_free_space(void *page) {
    PageHeader ph = (PageHeader)page;
    return (int)(ph->pd_upper - ph->pd_lower);
}

/**
 * @brief 获取元组数量
 */
int page_get_tuple_count(void *page) {
    PageHeader ph = (PageHeader)page;
    return (ph->pd_lower - SizeOfPageHeaderData) / SizeOfHeapLinePointer;
}

/**
 * @brief 在页面中插入元组
 * @return TID 或 NULL
 */
ItemPointer page_add_tuple(void *page, const void *tuple, size_t len) {
    PageHeader ph = (PageHeader)page;

    /* 检查空间 */
    int free_space = ph->pd_upper - ph->pd_lower;
    if (free_space < (int)(len + SizeOfHeapLinePointer)) {
        return NULL;
    }

    /* 在 pd_upper 处向前放置元组 */
    uint16_t off = ph->pd_upper - len;
    memcpy((char *)page + off, tuple, len);
    ph->pd_upper = off;

    /* 在 pd_lower 处向后放置 LinePointer */
    HeapLinePointer lp = (HeapLinePointer)((char *)page + ph->pd_lower);
    lp->t_off = off;
    lp->t_flags = 0;
    lp->t_xmax = 0;
    ph->pd_lower += SizeOfHeapLinePointer;

    /* 构造 TID 并存储到全局数组 */
    ItemPointerData *tid = &tid_array[tid_count++];
    tid->ip_blkno = 0;
    tid->ip_offset = (uint16_t)((char *)lp - (char *)page);
    return tid;
}

/**
 * @brief 标记元组为删除（打墓碑）
 */
bool page_delete_tuple(void *page, int index) {
    int count = page_get_tuple_count(page);
    if (index < 0 || index >= count) {
        return false;
    }

    /* 获取 LinePointer */
    uint16_t lp_off = SizeOfPageHeaderData + index * SizeOfHeapLinePointer;
    HeapLinePointer lp = (HeapLinePointer)((char *)page + lp_off);

    /* 打墓碑：设置 t_xmax 非0 */
    lp->t_xmax = 1;
    return true;
}

/**
 * @brief 通过 TID 定位元组
 */
void *page_get_tuple_by_tid(void *page, ItemPointer tid) {
    PageHeader ph = (PageHeader)page;
    if (tid->ip_offset < SizeOfPageHeaderData ||
        tid->ip_offset >= ph->pd_upper) {
        return NULL;
    }

    HeapLinePointer lp = (HeapLinePointer)((char *)page + tid->ip_offset);
    if (lp->t_off == 0 || lp->t_off >= HEAP_PAGE_SIZE) {
        return NULL;
    }

    /* 检查是否被删除 */
    if (lp->t_xmax != 0) {
        return NULL;
    }

    return (char *)page + lp->t_off;
}

/* ============================================================
 * 辅助函数
 * ============================================================ */

/** 打印页面布局 */
void dump_page_layout(void *page) {
    PageHeader ph = (PageHeader)page;
    int count = page_get_tuple_count(page);

    printf("\n=== 页面布局 [heap] ===\n");
    printf("Header: lsn=%u, lower=%u, upper=%u, free=%d bytes\n",
           ph->pd_lsn, ph->pd_lower, ph->pd_upper,
           page_get_free_space(page));
    printf("Tuples: %d 个\n", count);

    for (int i = 0; i < count; i++) {
        uint16_t lp_off = SizeOfPageHeaderData + i * SizeOfHeapLinePointer;
        HeapLinePointer lp = (HeapLinePointer)((char *)page + lp_off);
        char *data = (char *)page + lp->t_off;

        printf("  [%d] offset=%u, deleted=%s, data=\"%s\"\n",
               i, lp->t_off, lp->t_xmax ? "YES" : "NO", data);
    }
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void) {
    printf("Heap 堆表存储演示\n");
    printf("=================\n");

    /* 分配页面内存 */
    char *page = calloc(1, HEAP_PAGE_SIZE);
    if (!page) {
        perror("分配页面失败");
        return 1;
    }

    /* 初始化页面 */
    page_init(page, HEAP_PAGE_SIZE);

    /* 插入 3 个元组 */
    const char *tuples[] = {"Alice", "Bob", "Charlie"};
    ItemPointer tids[3];

    printf("\n[1] 插入元组\n");
    for (int i = 0; i < 3; i++) {
        tids[i] = page_add_tuple(page, tuples[i], strlen(tuples[i]) + 1);
        printf("  插入 \"%s\", TID=(block=%u, offset=%u)\n",
               tuples[i], tids[i]->ip_blkno, tids[i]->ip_offset);
    }
    dump_page_layout(page);

    /* 通过 TID 定位 */
    printf("\n[2] 通过 TID 定位元组\n");
    for (int i = 0; i < 3; i++) {
        void *data = page_get_tuple_by_tid(page, tids[i]);
        if (data) {
            printf("  TID[%d] -> \"%s\"\n", i, (char *)data);
        }
    }

    /* 删除第 2 个元组（Bob） */
    printf("\n[3] 删除元组 Bob（打墓碑）\n");
    page_delete_tuple(page, 1);
    dump_page_layout(page);

    /* 再次定位已删除的元组 */
    printf("\n[4] 尝试定位已删除的元组\n");
    void *data = page_get_tuple_by_tid(page, tids[1]);
    if (data) {
        printf("  TID[1] -> \"%s\" (仍可见)\n", (char *)data);
    } else {
        printf("  TID[1] -> NULL（已删除）\n");
    }

    /* 插入新元组 */
    printf("\n[5] 插入新元组 \"David\"\n");
    ItemPointer new_tid = page_add_tuple(page, "David", 6);
    printf("  插入 \"David\", TID=(block=%u, offset=%u)\n",
           new_tid->ip_blkno, new_tid->ip_offset);
    dump_page_layout(page);

    free(page);
    printf("\n演示完成！\n");
    return 0;
}
