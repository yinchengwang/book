/**
 * @file main.c
 * @brief Buffer Pool 缓存管理演示程序
 *
 * 演示内容：
 * - Hash 表查找页面
 * - Clock-Sweep 置换算法
 * - 脏页标记与刷盘
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

#define PAGE_SIZE       4096        /* 页面大小 4KB */
#define MAX_BUFFERS     8           /* 最大 Buffer 数量 */
#define MAX_PAGES       32          /* 最大页面数 */

/* Buffer 状态标志 */
#define BM_VALID        0x01        /* Buffer 有效 */
#define BM_DIRTY        0x02        /* Buffer 脏（已修改） */
#define BM_PINNED       0x04        /* Buffer 被 pin 住 */

/* ============================================================
 * 数据结构
 * ============================================================ */

/** Buffer 描述符 */
typedef struct BufferDesc_s {
    uint32_t    buf_id;             /* Buffer ID */
    uint32_t    page_id;            /* 页面 ID */
    uint32_t    state;              /* 状态标志 */
    uint32_t    usage_count;        /* 使用计数（Clock-Sweep） */
    uint32_t    refcount;           /* 引用计数 */
    char        *data;              /* 页面数据 */
} BufferDesc;

/** Buffer Pool */
typedef struct {
    BufferDesc  *descriptors;       /* 描述符数组 */
    char        **pages;            /* 页面数据数组 */
    int         nbuffers;           /* Buffer 数量 */
    int         next_victim;        /* Clock 指针 */
    int         clock_hand;         /* 扫描指针 */
    int         hash_size;          /* Hash 桶数量 */
    BufferDesc  **hash_table;       /* Hash 表（简化：page_id % hash_size） */
} BufferPool;

/* ============================================================
 * 全局变量
 * ============================================================ */

static BufferPool pool;

/* ============================================================
 * 脏页刷盘
 * ============================================================ */

void buf_flush_all(void);

/* ============================================================
 * 初始化
 * ============================================================ */

int buf_init(int nbuffers) {
    if (nbuffers <= 0) nbuffers = MAX_BUFFERS;

    pool.nbuffers = nbuffers;
    pool.next_victim = 0;
    pool.clock_hand = 0;

    /* 分配描述符和页面数组 */
    pool.descriptors = (BufferDesc *)calloc(nbuffers, sizeof(BufferDesc));
    pool.pages = (char **)calloc(nbuffers, sizeof(char *));

    if (!pool.descriptors || !pool.pages) {
        return -1;
    }

    /* 初始化每个 Buffer */
    for (int i = 0; i < nbuffers; i++) {
        pool.pages[i] = (char *)calloc(1, PAGE_SIZE);
        if (!pool.pages[i]) {
            for (int j = 0; j < i; j++) free(pool.pages[j]);
            return -1;
        }

        pool.descriptors[i].buf_id = i;
        pool.descriptors[i].page_id = UINT32_MAX;  /* 无效页面 */
        pool.descriptors[i].state = 0;
        pool.descriptors[i].usage_count = 0;
        pool.descriptors[i].refcount = 0;
        pool.descriptors[i].data = pool.pages[i];
    }

    /* 初始化 Hash 表 */
    pool.hash_size = nbuffers * 2;
    pool.hash_table = (BufferDesc **)calloc(pool.hash_size, sizeof(BufferDesc *));
    if (!pool.hash_table) {
        return -1;
    }

    printf("[buffer] Buffer Pool 初始化完成: %d 个 Buffer\n", nbuffers);
    return 0;
}

void buf_shutdown(void) {
    /* 刷写所有脏页 */
    buf_flush_all();

    /* 释放资源 */
    for (int i = 0; i < pool.nbuffers; i++) {
        free(pool.pages[i]);
    }
    free(pool.pages);
    free(pool.descriptors);
    free(pool.hash_table);

    printf("[buffer] Buffer Pool 已关闭\n");
}

/* ============================================================
 * Hash 表操作
 * ============================================================ */

static uint32_t hash_page(uint32_t page_id) {
    return page_id % pool.hash_size;
}

static BufferDesc *hash_lookup(uint32_t page_id) {
    uint32_t bucket = hash_page(page_id);
    return pool.hash_table[bucket];
}

static void hash_insert(BufferDesc *buf, uint32_t page_id) {
    uint32_t bucket = hash_page(page_id);
    pool.hash_table[bucket] = buf;
}

static void hash_remove(uint32_t page_id) {
    uint32_t bucket = hash_page(page_id);
    if (pool.hash_table[bucket] &&
        pool.hash_table[bucket]->page_id == page_id) {
        pool.hash_table[bucket] = NULL;
    }
}

/* ============================================================
 * Clock-Sweep 置换算法
 * ============================================================ */

/**
 * Clock-Sweep 算法：
 * 1. 从当前指针位置开始扫描
 * 2. 如果 Buffer 被 pin（refcount > 0），跳过
 * 3. 如果 usage_count > 0，减少计数并继续
 * 4. 否则，该 Buffer 可被置换
 */
static int clock_sweep(void) {
    int n = pool.nbuffers;

    for (int tries = 0; tries < n * 2; tries++) {
        BufferDesc *buf = &pool.descriptors[pool.clock_hand];

        /* 跳过正在使用的 Buffer */
        if (buf->refcount > 0) {
            pool.clock_hand = (pool.clock_hand + 1) % n;
            printf("[buffer] Clock: 跳过 Buffer %d (refcount=%d)\n",
                   buf->buf_id, buf->refcount);
            continue;
        }

        /* 如果 usage_count > 0，减少并继续 */
        if (buf->usage_count > 0) {
            buf->usage_count--;
            pool.clock_hand = (pool.clock_hand + 1) % n;
            printf("[buffer] Clock: 减少 Buffer %d usage_count -> %d\n",
                   buf->buf_id, buf->usage_count);
            continue;
        }

        /* 找到可置换的 Buffer */
        int buf_id = pool.clock_hand;
        pool.clock_hand = (pool.clock_hand + 1) % n;

        /* 如果 Buffer 有脏数据，需要刷盘 */
        if (buf->state & BM_VALID && buf->state & BM_DIRTY) {
            printf("[buffer] Clock: 置换前刷盘 Buffer %d (page_id=%u)\n",
                   buf_id, buf->page_id);
            buf->state &= ~BM_DIRTY;
        }

        /* 从 Hash 表移除旧条目 */
        if (buf->state & BM_VALID) {
            hash_remove(buf->page_id);
        }

        printf("[buffer] Clock: 选中 Buffer %d 进行置换\n", buf_id);
        return buf_id;
    }

    /* 无法找到可用 Buffer（理论上不会发生） */
    printf("[buffer] Clock: 警告 - 强制使用 Buffer 0\n");
    return 0;
}

/* ============================================================
 * 页面操作
 * ============================================================ */

BufferDesc *buf_get_page(uint32_t page_id) {
    /* 1. 先在 Hash 表中查找 */
    BufferDesc *buf = hash_lookup(page_id);
    if (buf) {
        printf("[buffer] Hash 命中: page_id=%u -> Buffer %d\n", page_id, buf->buf_id);
        buf->usage_count++;  /* 增加使用计数 */
        buf->refcount++;     /* Pin 住 */
        return buf;
    }

    printf("[buffer] Hash 未命中: page_id=%u\n", page_id);

    /* 2. 未命中，分配新 Buffer */
    int buf_id = clock_sweep();
    buf = &pool.descriptors[buf_id];

    /* 3. 清除旧内容 */
    if (buf->state & BM_VALID) {
        /* 已在 clock_sweep 中处理脏页刷盘 */
    }

    /* 4. 加载新页面（模拟从磁盘读取） */
    buf->page_id = page_id;
    buf->state = BM_VALID;
    buf->usage_count = 1;
    buf->refcount = 1;
    memset(buf->data, 0, PAGE_SIZE);

    /* 5. 插入 Hash 表 */
    hash_insert(buf, page_id);

    printf("[buffer] 加载页面: page_id=%u -> Buffer %d\n", page_id, buf_id);
    return buf;
}

void buf_unpin(BufferDesc *buf) {
    if (buf->refcount > 0) {
        buf->refcount--;
        printf("[buffer] Unpin Buffer %d (refcount=%d)\n", buf->buf_id, buf->refcount);
    }
}

void buf_dirty(BufferDesc *buf) {
    if (!(buf->state & BM_DIRTY)) {
        buf->state |= BM_DIRTY;
        printf("[buffer] 标记脏页: Buffer %d (page_id=%u)\n", buf->buf_id, buf->page_id);
    }
}

void buf_flush_all(void) {
    printf("[buffer] 刷写所有脏页:\n");
    for (int i = 0; i < pool.nbuffers; i++) {
        if (pool.descriptors[i].state & BM_DIRTY) {
            printf("[buffer]   刷盘 Buffer %d (page_id=%u)\n",
                   i, pool.descriptors[i].page_id);
            pool.descriptors[i].state &= ~BM_DIRTY;
        }
    }
}

/* ============================================================
 * 演示程序
 * ============================================================ */

int main(void) {
    printf("=== Buffer Pool 演示程序 ===\n\n");

    /* 初始化 Buffer Pool */
    if (buf_init(4) != 0) {
        fprintf(stderr, "Buffer Pool 初始化失败\n");
        return 1;
    }

    printf("\n--- 演示 1: Hash 表查找 ---\n");
    BufferDesc *buf1 = buf_get_page(100);  /* 未命中，加载 */
    buf_unpin(buf1);

    BufferDesc *buf2 = buf_get_page(100);  /* 命中 */
    buf_unpin(buf2);

    printf("\n--- 演示 2: 脏页标记 ---\n");
    BufferDesc *buf3 = buf_get_page(200);
    buf_dirty(buf3);                        /* 标记为脏 */
    memcpy(buf3->data, "Hello Buffer Pool!", 19);
    buf_unpin(buf3);

    printf("\n--- 演示 3: Clock-Sweep 置换 ---\n");
    /* 加载更多页面触发置换 */
    for (uint32_t page_id = 300; page_id <= 350; page_id += 10) {
        BufferDesc *buf = buf_get_page(page_id);
        buf_dirty(buf);
        buf_unpin(buf);
    }

    printf("\n--- 演示 4: 脏页刷盘 ---\n");
    buf_flush_all();

    /* 关闭 */
    buf_shutdown();

    printf("\n=== 演示完成 ===\n");
    return 0;
}
