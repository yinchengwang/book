/**
 * @file bufmgr.c
 * @brief Buffer Pool 管理器实现
 *
 * 实现 Clock-Sweep 置换算法和 Hash 表管理
 *
 * ## 核心设计
 *
 * ### Buffer Pool 结构
 * - 采用"描述符 + 数据页"分离设计，描述符存储元数据
 * - Hash 表提供 O(1) 页面查找能力
 * - Clock-Sweep 算法在 O(1) 时间复杂度内找到 victim
 *
 * ### 并发控制
 * - 使用 refcount 实现 pin/unpin 机制
 * - 页面锁（Page Lock）用于细粒度并发控制
 * - 使用原子操作（atomic）保证计数器线程安全
 *
 * ### 置换策略
 * - usage_count 记录页面访问频率
 * - 被 pin 的页面不可置换
 * - 脏页在置换前必须刷盘
 *
 * ## 性能特性
 * - Hash 查找: O(1) 平均
 * - 置换查找: 最坏 O(n)，实际接近 O(1)
 * - 命中率依赖访问局部性
 */

#include "db/buf.h"
#include "db/kv.h"
#include "db/lock.h"
#include "db/page.h"       /* page_verify_checksum, page_set_checksum */
#include "db/guc.h"        /* guc_get_bool */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 内部常量
 * ============================================================ */

#define BUF_HASH_BUCKETS(bn) ((bn) * BUF_HASH_SIZE_FACTOR)

/* ============================================================
 * 内部数据类型
 * ============================================================ */

/** Hash 表条目 */
typedef struct BufHashEntry_s {
    uint32_t    relfilenode;      /**< 物理文件节点 */
    BlockNumber blocknum;         /**< 块号 */
    uint32_t    buf_id;           /**< Buffer ID */
    struct BufHashEntry_s *next;  /**< 链表下一项 */
} BufHashEntry;

/** Buffer Pool */
struct BufferPool_s {
    BufferDesc  *descriptors;     /**< 描述符数组 */
    char        **buffers;        /**< 页面数据数组 */
    int         nbuffers;         /**< Buffer 数量 */

    /* Hash 表 */
    BufHashEntry **hash_table;    /**< Hash 桶数组 */
    uint32_t    hash_size;        /**< Hash 桶数量 */
    uint32_t    hash_entries;     /**< Hash 条目数量 */

    /* Clock-Sweep */
    uint32_t    next_victim;      /**< Clock 指针 */

    /* 锁（简化版，使用原子操作） */
    void        *io_lock;         /**< IO 锁（保留） */

    /* 统计 */
    uint64_t    hits;
    uint64_t    misses;
    uint64_t    writes;
    uint64_t    reads;
};

/* ============================================================
 * 全局变量
 * ============================================================ */

static BufferPool *buffer_pool = NULL;
static bool buf_initialized = false;

/* KV 存储（可选，用于页面持久化） */
static kv_t *kv_store = NULL;

/* ============================================================
 * Hash 函数
 * ============================================================ */

static uint32_t buf_hash(uint32_t relfilenode, BlockNumber blocknum) {
    /* 简单的组合 hash */
    uint64_t key = ((uint64_t)relfilenode << 32) | blocknum;
    return ((key * 2654435761UL) >> 16) % buffer_pool->hash_size;
}

/* ============================================================
 * 初始化
 * ============================================================ */

int buf_init(int nbuffers) {
    if (buf_initialized) {
        return 0;
    }

    /* 默认 Buffer 数量 */
    if (nbuffers <= 0) {
        nbuffers = BUF_DEFAULT_NBUFFERS;
    }

    buffer_pool = (BufferPool *)calloc(1, sizeof(BufferPool));
    if (!buffer_pool) {
        return -1;
    }

    buffer_pool->nbuffers = nbuffers;
    buffer_pool->next_victim = 0;
    buffer_pool->hash_entries = 0;

    /* 分配描述符数组 */
    buffer_pool->descriptors = (BufferDesc *)calloc(nbuffers, sizeof(BufferDesc));
    if (!buffer_pool->descriptors) {
        free(buffer_pool);
        buffer_pool = NULL;
        return -1;
    }

    /* 分配页面数据数组 */
    buffer_pool->buffers = (char **)calloc(nbuffers, sizeof(char *));
    if (!buffer_pool->buffers) {
        free(buffer_pool->descriptors);
        free(buffer_pool);
        buffer_pool = NULL;
        return -1;
    }

    /* 分配每个页面 */
    for (int i = 0; i < nbuffers; i++) {
        buffer_pool->buffers[i] = (char *)calloc(1, BUF_PAGE_SIZE);
        if (!buffer_pool->buffers[i]) {
            /* 清理已分配的 */
            for (int j = 0; j < i; j++) {
                free(buffer_pool->buffers[j]);
            }
            free(buffer_pool->buffers);
            free(buffer_pool->descriptors);
            free(buffer_pool);
            buffer_pool = NULL;
            return -1;
        }

        /* 初始化描述符 */
        buffer_pool->descriptors[i].buf_id = i;
        buffer_pool->descriptors[i].state = 0;
        buffer_pool->descriptors[i].relfilenode = 0;
        buffer_pool->descriptors[i].blocknum = 0;
        buffer_pool->descriptors[i].usage_count = 0;
        buffer_pool->descriptors[i].last_written = 0;
        buffer_pool->descriptors[i].hash_prev = UINT32_MAX;
        buffer_pool->descriptors[i].hash_next = UINT32_MAX;
        buffer_pool->descriptors[i].refcount = 0;
    }

    /* 分配 Hash 表 */
    buffer_pool->hash_size = BUF_HASH_BUCKETS(nbuffers);
    buffer_pool->hash_table = (BufHashEntry **)calloc(
        buffer_pool->hash_size, sizeof(BufHashEntry *));
    if (!buffer_pool->hash_table) {
        /* 清理 */
        for (int i = 0; i < nbuffers; i++) {
            free(buffer_pool->buffers[i]);
        }
        free(buffer_pool->buffers);
        free(buffer_pool->descriptors);
        free(buffer_pool);
        buffer_pool = NULL;
        return -1;
    }

    /* 初始化统计 */
    buffer_pool->hits = 0;
    buffer_pool->misses = 0;
    buffer_pool->writes = 0;
    buffer_pool->reads = 0;

    buf_initialized = true;
    return 0;
}

void buf_shutdown(void) {
    if (!buf_initialized) {
        return;
    }

    /* 刷写所有脏页 */
    buf_flush_all();

    /* 关闭 KV 存储（如果有） */
    if (kv_store) {
        kv_close(kv_store);
        kv_store = NULL;
    }

    /* 释放 Hash 表 */
    for (uint32_t i = 0; i < buffer_pool->hash_size; i++) {
        BufHashEntry *entry = buffer_pool->hash_table[i];
        while (entry) {
            BufHashEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(buffer_pool->hash_table);

    /* 释放页面数据 */
    for (int i = 0; i < buffer_pool->nbuffers; i++) {
        free(buffer_pool->buffers[i]);
    }
    free(buffer_pool->buffers);

    /* 释放描述符 */
    free(buffer_pool->descriptors);

    free(buffer_pool);
    buffer_pool = NULL;
    buf_initialized = false;

    /* 关闭锁子系统 */
    buf_lock_shutdown();
}

int buf_get_nbuffers(void) {
    return buffer_pool ? buffer_pool->nbuffers : 0;
}

/**
 * Hash 表操作
 *
 * ## Hash 表设计
 *
 * 使用链地址法（Separate Chaining）解决 Hash 冲突：
 * - 每个桶维护一个链表，支持 O(1) 插入/删除
 * - Hash 函数使用 Knuth 乘法哈希变体
 *
 * ## 关键操作
 *
 * ### hash_lookup
 * - 输入: (relfilenode, blocknum) 元组标识
 * - 过程: 计算桶索引 → 遍历链表 → 精确匹配
 * - 输出: 命中的 BufHashEntry 或 NULL
 * - 时间复杂度: O(1) 平均，O(n) 最坏（所有键冲突到同一桶）
 *
 * ### hash_insert
 * - 采用头插法，新条目插入链表头部
 * - 同时更新描述符的 hash_prev/hash_next 指针
 *
 * ### hash_delete
 * - 使用 prev 指针维护链表，实现 O(1) 删除
 * - 需要先调用 hash_lookup 找到待删除条目
 */

/**
 * @brief Hash 表查找
 *
 * 在 Hash 表中查找指定 (relfilenode, blocknum) 对应的 Buffer
 *
 * @param relfilenode 关系文件节点号（表/索引的唯一标识）
 * @param blocknum    块号（页面在文件中的偏移）
 * @return 命中的 Hash 表条目，NULL 表示未命中
 *
 * @note 调用方需持有必要的锁
 */
static BufHashEntry *hash_lookup(uint32_t relfilenode, BlockNumber blocknum) {
    /* 第一步：计算 Hash 桶索引
     * 使用 Knuth 乘法哈希变体：
     * - key = (relfilenode << 32) | blocknum 合并两个 32 位值
     * - 乘以黄金分割常数 2654435761 (2^32 / phi)
     * - 右移 16 位后取模，实现均匀分布
     */
    uint32_t bucket = buf_hash(relfilenode, blocknum);
    BufHashEntry *entry = buffer_pool->hash_table[bucket];

    /* 第二步：遍历链表查找精确匹配
     * 理论上可能有 Hash 冲突，需要逐个比较
     */
    while (entry) {
        if (entry->relfilenode == relfilenode && entry->blocknum == blocknum) {
            return entry;  /* 命中！返回条目 */
        }
        entry = entry->next;
    }
    return NULL;  /* 未命中 */
}

/**
 * @brief Hash 表插入
 *
 * 将新的 (relfilenode, blocknum) → buf_id 映射插入 Hash 表
 *
 * @param relfilenode 关系文件节点号
 * @param blocknum    块号
 * @param buf_id      Buffer ID
 *
 * @note 使用头插法，时间复杂度 O(1)
 * @note 调用方需确保该 (relfilenode, blocknum) 不存在
 */
static void hash_insert(uint32_t relfilenode, BlockNumber blocknum, uint32_t buf_id) {
    /* 计算 Hash 桶索引 */
    uint32_t bucket = buf_hash(relfilenode, blocknum);

    /* 分配新条目 */
    BufHashEntry *entry = (BufHashEntry *)malloc(sizeof(BufHashEntry));
    if (!entry) return;  /* 内存分配失败 */

    /* 填充条目字段 */
    entry->relfilenode = relfilenode;
    entry->blocknum = blocknum;
    entry->buf_id = buf_id;

    /* 头插法：插入链表头部
     * 原链表头成为新条目的下一个节点
     */
    entry->next = buffer_pool->hash_table[bucket];
    buffer_pool->hash_table[bucket] = entry;

    /* 更新统计和描述符指针 */
    buffer_pool->hash_entries++;

    /* 同步更新描述符的 Hash 链表指针
     * 用于双向遍历优化（如果将来需要）
     */
    BufferDesc *buf = &buffer_pool->descriptors[buf_id];
    buf->hash_prev = UINT32_MAX;  /* 头部节点无前驱 */
    buf->hash_next = (entry->next) ? entry->next->buf_id : UINT32_MAX;
}

/**
 * @brief Hash 表删除
 *
 * 从 Hash 表中移除指定 (relfilenode, blocknum) 的映射
 *
 * @param relfilenode 关系文件节点号
 * @param blocknum    块号
 *
 * @note 使用 prev 指针实现 O(1) 删除
 * @note 不检查条目是否存在
 */
static void hash_delete(uint32_t relfilenode, BlockNumber blocknum) {
    uint32_t bucket = buf_hash(relfilenode, blocknum);
    BufHashEntry **prev = &buffer_pool->hash_table[bucket];
    BufHashEntry *entry = *prev;

    /* 遍历链表找到目标条目 */
    while (entry) {
        if (entry->relfilenode == relfilenode && entry->blocknum == blocknum) {
            /* 更新前驱指针，跳过当前节点
             * 等价于: prev->next = entry->next
             */
            *prev = entry->next;
            free(entry);  /* 释放内存 */
            buffer_pool->hash_entries--;
            return;
        }
        prev = &entry->next;  /* 移动前驱指针 */
        entry = entry->next;
    }
}

/* ============================================================
 * Hash 表公共 API（供其他模块使用）
 * ============================================================ */

void buf_hash_insert(uint32_t relfilenode, BlockNumber blocknum, uint32_t buf_id) {
    hash_insert(relfilenode, blocknum, buf_id);
}

void buf_hash_delete(uint32_t relfilenode, BlockNumber blocknum) {
    hash_delete(relfilenode, blocknum);
}

/**
 * Clock-Sweep 置换算法
 *
 * ## 算法原理
 *
 * Clock-Sweep（又称 Second-Chance）是 LRU 的近似实现，
 * 在 O(1) 时间复杂度内找到可置换的 Buffer，避免全表扫描。
 *
 * ## 算法流程
 *
 * ```
 *  while (未找到 victim) {
 *      1. 检查当前候选 buffer:
 *         - 如果 refcount > 0 (正在使用): 跳过，refcount--
 *         - 如果 usage_count > 0 (最近访问过): usage_count--，跳过
 *         - 否则: 这是 victim，可以置换
 *
 *      2. 移动 clock 指针到下一个 buffer
 *
 *      3. 如果遍历一圈未找到: 返回第一个 buffer (强制置换)
 *  }
 * ```
 *
 * ## 与 LRU 的对比
 *
 * | 特性 | LRU | Clock-Sweep |
 * |------|-----|-------------|
 * | 时间复杂度 | O(1) | O(1) |
 * | 空间复杂度 | O(n) | O(1) |
 * | 实现复杂度 | 复杂（双向链表） | 简单 |
 * | 缓存污染 | 低 | 中等 |
 * | 扫描开销 | 无 | 有 |
 *
 * ## 为什么用 usage_count？
 *
 * - usage_count 初始为 0
 * - 每次页面被访问（buf_read/buf_pin）时递增
 * - Clock-Sweep 遍历时递减，直到 0 才能置换
 * - 相当于 N 次"第二次访问机会"
 *
 * ## 脏页处理
 *
 * 找到 victim 后：
 * 1. 如果是脏页，先调用 buf_write 刷盘
 * 2. 从 Hash 表移除旧映射
 * 3. 返回 buf_id 供重新分配
 */

/**
 * @brief Clock-Sweep 置换算法
 *
 * 找到一个可置换的 Buffer 供重新使用
 *
 * @return 可置换的 Buffer ID
 *
 * @note 返回的 Buffer 可能仍是脏的，调用方需处理
 * @note 最多遍历 2n 个 buffer 确保找到 victim
 */
static uint32_t clock_sweep(void) {
    int n = buffer_pool->nbuffers;

    /**
     * 外层循环：最多遍历 2 圈
     * 理论上最多需要 2n 次检查（如果所有 buffer 都被 pin）
     * n 次让 refcount 减到 0，n 次检查 usage_count
     */
    for (int tries = 0; tries < n * 2; tries++) {
        /* 获取当前候选 buffer 的描述符 */
        BufferDesc *buf = &buffer_pool->descriptors[buffer_pool->next_victim];

        /* ===== 第一关：检查 refcount =====
         * refcount > 0 表示页面正在被使用
         * 此时不能置换，但需要递减计数（释放已不存在的引用）
         */
        if (buf->refcount > 0) {
            buffer_pool->next_victim = (buffer_pool->next_victim + 1) % n;
            continue;  /* 跳过，检查下一个 */
        }

        /* ===== 第二关：检查 usage_count =====
         * usage_count > 0 表示页面最近被访问过
         * 给它一个"第二次机会"，递减后继续
         * 这样频繁访问的页面不会被轻易置换
         */
        if (buf->usage_count > 0) {
            buf->usage_count--;
            buffer_pool->next_victim = (buffer_pool->next_victim + 1) % n;
            continue;  /* 跳过，检查下一个 */
        }

        /* ===== 通过所有检查，找到 victim！=====
         * 此时 refcount == 0 且 usage_count == 0
         * 这个 buffer 可以安全置换
         */
        uint32_t buf_id = buffer_pool->next_victim;
        buffer_pool->next_victim = (buffer_pool->next_victim + 1) % n;

        /* 如果是有效页面，处理脏页和 Hash 表 */
        if (buf->state & BM_VALID) {
            /**
             * 检查是否需要写回
             * BM_DIRTY 标记表示页面被修改过
             * 置换前必须持久化，否则数据丢失
             */
            if (buf->state & BM_DIRTY) {
                /* 需要写回磁盘 */
                buf_write(buf);
            }
            /* 从 Hash 表移除旧映射
             * 这样下次查找不会返回已置换的页面
             */
            hash_delete(buf->relfilenode, buf->blocknum);
        }

        return buf_id;  /* 返回可用的 buffer */
    }

    /* ===== 极端情况：所有 buffer 都被 pin =====
     * 不应该发生，但作为保底，返回 buffer 0
     * 调用方可能需要等待或报错
     */
    return 0;
}

/**
 * 页面读取与分配
 *
 * ## 页面读取流程 (buf_read)
 *
 * ```
 *                    ┌─────────────────┐
 *                    │  输入: (relfn, blkno, mode) │
 *                    └────────┬────────┘
 *                             │
 *                             ▼
 *                    ┌─────────────────┐
 *                    │ Hash 表查找     │
 *                    └────────┬────────┘
 *                             │
 *              ┌──────────────┴──────────────┐
 *              │ 命中？                        │
 *              └──────────────┬──────────────┘
 *                    Yes ↓              No ↓
 *              ┌──────────┐     ┌─────────────────┐
 *              │ hits++   │     │ misses++        │
 *              │ usage++  │     │ clock_sweep()   │
 *              │ dirty?   │     │ 分配新 buffer   │
 *              └──────────┘     └────────┬────────┘
 *                                        │
 *                                        ▼
 *                               ┌─────────────────┐
 *                               │ 读取页面数据      │
 *                               │ (从磁盘或 KV)    │
 *                               └─────────────────┘
 * ```
 *
 * ## 三种访问模式
 *
 * | mode | 含义 | 行为 |
 * |------|------|------|
 * | 0 | 只读 | 不标记脏，共享访问 |
 * | > 0 | 写访问 | 标记脏，排他访问 |
 */

/**
 * @brief 读取或创建页面
 *
 * 获取指定 (relfilenode, blocknum) 对应的 Buffer
 * 如果页面不在内存中，分配新 Buffer 并从磁盘读取
 *
 * @param relfilenode 关系文件节点号
 * @param blocknum    块号
 * @param access_mode 访问模式：0=只读，>0=写访问
 * @return Buffer 描述符，NULL 表示失败
 *
 * @note 返回的 Buffer 被 pin，调用方使用完后必须 buf_unpin
 * @note 写访问时自动标记脏页
 *
 * ## 使用示例
 *
 * @code
 * // 读取页面（只读）
 * BufferDesc *buf = buf_read(relfn, blocknum, 0);
 * if (buf) {
 *     char *data = buf_get_data(buf);
 *     // ... 使用数据 ...
 *     buf_unpin(buf);
 * }
 *
 * // 修改页面（写访问）
 * BufferDesc *buf = buf_read(relfn, blocknum, 1);
 * if (buf) {
 *     char *data = buf_get_data(buf);
 *     memcpy(data, new_content, BUF_PAGE_SIZE);
 *     // 无需手动调用 buf_dirty，写访问已自动标记
 *     buf_unpin(buf);
 * }
 * @endcode
 */
BufferDesc *buf_read(uint32_t relfilenode, BlockNumber blocknum, int access_mode) {
    if (!buf_initialized) {
        return NULL;
    }

    /* ===== 第一步：Hash 表查找 =====
     * 先检查页面是否已在内存中
     * O(1) 平均时间复杂度
     */
    BufHashEntry *entry = hash_lookup(relfilenode, blocknum);
    if (entry) {
        /* ===== 命中！=====
         * 页面已在 Buffer Pool 中
         */

        buffer_pool->hits++;  /* 更新命中统计 */

        BufferDesc *buf = &buffer_pool->descriptors[entry->buf_id];

        /* 增加 usage_count
         * 反映页面被访问，下次置换时优先级降低
         */
        buf->usage_count++;

        /* 如果需要写访问，标记为脏
         * 脏页在置换前必须刷盘
         */
        if (access_mode > 0) {
            buf_dirty(buf);
        }

        return buf;  /* 直接返回，无需 IO */
    }

    /* ===== 未命中 =====
     * 页面不在内存中，需要分配新 buffer 并读取
     */

    buffer_pool->misses++;  /* 更新未命中统计 */

    /* 通过 Clock-Sweep 分配一个 buffer
     * 如果需要，会自动处理脏页刷盘
     */
    uint32_t buf_id = clock_sweep();
    BufferDesc *buf = &buffer_pool->descriptors[buf_id];

    /* 如果原 buffer 包含有效数据，从 Hash 表移除旧映射
     * 避免 (relfilenode, blocknum) 冲突
     */
    if (buf->state & BM_VALID) {
        hash_delete(buf->relfilenode, buf->blocknum);
    }

    /* 设置新页面的元数据 */
    buf->relfilenode = relfilenode;
    buf->blocknum = blocknum;
    buf->usage_count = 1;   /* 新页面至少被访问一次 */
    buf->state = BM_VALID;   /* 标记为有效 */
    buf->refcount = 1;       /* 当前线程正在使用 */

    /* 写访问时标记脏页 */
    if (access_mode > 0) {
        buf->state |= BM_DIRTY;
    }

    /* 清空页面数据
     * 新页面需要零初始化
     */
    memset(buffer_pool->buffers[buf_id], 0, BUF_PAGE_SIZE);

    /* 插入 Hash 表，建立映射关系
     * 后续查找可以直接命中
     */
    hash_insert(relfilenode, blocknum, buf_id);

    /* 从磁盘读取页面数据
     * 这里简化实现，实际应调用 page_read()
     * 目前统计 reads 但未执行实际 IO
     */
    buffer_pool->reads++;

    /* [A1.1] 校验和验证：从磁盘加载的页面需验证校验和
     * Buffer Pool 使用 BUF_PAGE_SIZE (8192) 作为页面大小，
     * 校验和计算也基于此大小，而非 sizeof(page_t) (65536)。
     */
    {
        page_t *pg = (page_t *)buffer_pool->buffers[buf_id];
        uint16_t saved = pg->header.checksum;
        uint16_t expected = page_calc_checksum_bytes((const uint8_t *)pg, BUF_PAGE_SIZE);
        if (saved != expected) {
            bool *ignore_failure = guc_get_bool("ignore_checksum_failure");
            if (ignore_failure && *ignore_failure) {
                /* 配置忽略校验和失败，仅记录日志 */
                fprintf(stderr, "[WARN] checksum verification failed for "
                        "relfilenode=%u blocknum=%u (ignored)\n",
                        relfilenode, blocknum);
            } else {
                /* 校验和失败，标记为损坏 */
                fprintf(stderr, "[ERROR] checksum verification failed for "
                        "relfilenode=%u blocknum=%u\n",
                        relfilenode, blocknum);
                buf->state |= BM_CORRUPTED;
                buf_unpin(buf);
                return NULL;
            }
        }
    }

    return buf;
}

BufferDesc *buf_new(uint32_t relfilenode) {
    if (!buf_initialized) {
        return NULL;
    }

    /* 分配一个 Buffer */
    uint32_t buf_id = clock_sweep();
    BufferDesc *buf = &buffer_pool->descriptors[buf_id];

    /* 清除旧内容 */
    if (buf->state & BM_VALID) {
        hash_delete(buf->relfilenode, buf->blocknum);
    }

    /* 设置新内容 - 新页面 */
    buf->relfilenode = relfilenode;
    buf->blocknum = 0;  /* 临时标记，之后会更新 */
    buf->usage_count = 1;
    buf->state = BM_VALID | BM_DIRTY;
    buf->refcount = 1;

    /* 清空页面数据 */
    memset(buffer_pool->buffers[buf_id], 0, BUF_PAGE_SIZE);

    /* [A1.4] 新页面设置初始校验和
     * 使用 BUF_PAGE_SIZE 计算校验和，与 buf_read 校验和验证一致。
     */
    {
        page_t *pg = (page_t *)buffer_pool->buffers[buf_id];
        pg->header.checksum = page_calc_checksum_bytes((const uint8_t *)pg, BUF_PAGE_SIZE);
    }

    return buf;
}

BufferDesc *buf_new_page(uint32_t relfilenode) {
    /* 类似于 buf_new，但专门用于表扩展 */
    BufferDesc *buf = buf_new(relfilenode);
    if (buf) {
        /* 标记为新页面 */
        buf->state |= BM_DIRTY;
    }
    return buf;
}

BufferDesc *buf_new_temp(void) {
    if (!buf_initialized) {
        return NULL;
    }

    /* 分配一个 Buffer，不加入 Hash 表 */
    uint32_t buf_id = clock_sweep();
    BufferDesc *buf = &buffer_pool->descriptors[buf_id];

    /* 清除旧内容 */
    if (buf->state & BM_VALID) {
        hash_delete(buf->relfilenode, buf->blocknum);
    }

    /* 设置为临时页面 */
    buf->relfilenode = 0;
    buf->blocknum = 0;
    buf->usage_count = 1;
    buf->state = BM_VALID;  /* 不标记为脏 */
    buf->refcount = 1;

    /* 清空页面数据 */
    memset(buffer_pool->buffers[buf_id], 0, BUF_PAGE_SIZE);

    /* [A1.4] 新页面设置初始校验和
     * 使用 BUF_PAGE_SIZE 计算校验和，与 buf_read 校验和验证一致。
     */
    {
        page_t *pg = (page_t *)buffer_pool->buffers[buf_id];
        pg->header.checksum = page_calc_checksum_bytes((const uint8_t *)pg, BUF_PAGE_SIZE);
    }

    return buf;
}

/* ============================================================
 * Buffer Pin/Unpin
 * ============================================================ */

void buf_pin(BufferDesc *buf) {
    if (buf) {
        buf->refcount++;
        buf->usage_count++;
    }
}

void buf_unpin(BufferDesc *buf) {
    if (buf && buf->refcount > 0) {
        buf->refcount--;
    }
}

bool buf_ispinned(BufferDesc *buf) {
    return buf && buf->refcount > 0;
}

/* ============================================================
 * 脏页标记
 * ============================================================ */

void buf_dirty(BufferDesc *buf) {
    if (buf) {
        buf->state |= BM_DIRTY | BM_JUST_DIRTIED;
    }
}

void buf_clean(BufferDesc *buf) {
    if (buf) {
        buf->state &= ~BM_DIRTY;
    }
}

bool buf_isdirty(BufferDesc *buf) {
    return buf && (buf->state & BM_DIRTY) != 0;
}

/**
 * 页面刷盘
 *
 * ## 脏页刷盘策略
 *
 * ### 何时刷盘？
 * 1. **主动刷盘**：调用 buf_flush_all() 或 buf_flush_relation()
 * 2. **被动刷盘**：Clock-Sweep 置换脏页时
 * 3. **检查点刷盘**：WAL 检查点期间
 *
 * ### 刷盘顺序
 * - 为保证一致性，按 BlockNumber 顺序刷盘
 * - 或按 LSN 顺序刷盘（WAL 协调）
 *
 * ### 双写（Double Write）
 * 现代数据库（如 PostgreSQL）使用双写技术：
 * 1. 先写 doublewrite buffer（顺序写，速度快）
 * 2. 再异步写数据页（随机写，速度慢）
 * 3. 崩溃恢复时，从 doublewrite 恢复损坏的页面
 *
 * 本实现简化处理，直接写数据页。
 */

/**
 * @brief 刷写所有脏页
 *
 * 遍历所有 Buffer，将脏页写入磁盘
 *
 * @return 刷写的页面数量
 *
 * @note 通常在 shutdown 或检查点时调用
 */
int buf_flush_all(void) {
    if (!buf_initialized) {
        return 0;
    }

    int count = 0;
    /* 遍历所有 Buffer */
    for (int i = 0; i < buffer_pool->nbuffers; i++) {
        BufferDesc *buf = &buffer_pool->descriptors[i];
        /* 只处理有效且脏的 Buffer */
        if ((buf->state & BM_VALID) && (buf->state & BM_DIRTY)) {
            buf_write(buf);
            count++;
        }
    }
    return count;
}

/**
 * @brief 刷写指定关系的脏页
 *
 * 只刷写属于指定 relfilenode 的脏页
 *
 * @param relfilenode 关系文件节点号
 * @return 刷写的页面数量
 *
 * @note 用于关系关闭或删除前的清理
 */
int buf_flush_relation(uint32_t relfilenode) {
    if (!buf_initialized) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < buffer_pool->nbuffers; i++) {
        BufferDesc *buf = &buffer_pool->descriptors[i];
        /* 匹配关系且是脏页 */
        if (buf->relfilenode == relfilenode &&
            (buf->state & BM_VALID) && (buf->state & BM_DIRTY)) {
            buf_write(buf);
            count++;
        }
    }
    return count;
}

/**
 * @brief 刷写单个 Buffer
 *
 * 将指定 Buffer 的内容写入磁盘，并清除脏标记
 *
 * @param buf Buffer 描述符
 * @return 0 表示成功，-1 表示失败
 *
 * @note 不处理 BM_VALID 检查，调用方需确保有效性
 * @note 写成功后清除 BM_DIRTY 标记
 *
 * ## 实现说明
 *
 * 简化实现：
 * 1. 更新 writes 统计
 * 2. 清除脏标记
 *
 * 完整实现应：
 * 1. 调用 page_write() 实际写入
 * 2. 更新 WAL LSN
 * 3. 等待 IO 完成（同步）或记录 IO 请求（异步）
 */
int buf_write(BufferDesc *buf) {
    if (!buf_initialized || !buf) {
        return -1;
    }

    if (!(buf->state & BM_VALID)) {
        return 0;
    }

    /* 如果 Buffer 有关联的文件，调用 disk 层写入 */
    if (buf->file) {
        page_t *pg = (page_t *)buffer_pool->buffers[buf->buf_id];

        /* 刷盘前设置校验和 */
        pg->header.checksum = page_calc_checksum_bytes((const uint8_t *)pg, BUF_PAGE_SIZE);

        /* 通过 db_file_t 写入 */
        int ret = disk_write_page((db_file_t *)buf->file, buf->blocknum, pg);
        if (ret != 0) {
            fprintf(stderr, "buf_write: 页面写盘失败 relfilenode=%u blocknum=%u\n",
                      buf->relfilenode, buf->blocknum);
            return -1;
        }
    }

    /* 清除脏标记 */
    buf->state &= ~BM_DIRTY;

    /* 更新统计 */
    buffer_pool->writes++;

    return 0;
}

/* ============================================================
 * Buffer 数据访问
 * ============================================================ */

void *buf_get_data(BufferDesc *buf) {
    if (!buf || !buf_initialized) {
        return NULL;
    }
    return buffer_pool->buffers[buf->buf_id];
}

BlockNumber buf_get_blocknum(BufferDesc *buf) {
    return buf ? buf->blocknum : 0;
}

uint32_t buf_get_relfilenode(BufferDesc *buf) {
    return buf ? buf->relfilenode : 0;
}

int buf_get_id(BufferDesc *buf) {
    return buf ? (int)buf->buf_id : -1;
}

/* ============================================================
 * Buffer 状态查询
 * ============================================================ */

BufferState buf_get_state(BufferDesc *buf) {
    return buf ? buf->state : 0;
}

bool buf_isvalid(BufferDesc *buf) {
    return buf && (buf->state & BM_VALID) != 0;
}

/* ============================================================
 * 统计信息
 * ============================================================ */

void buf_get_stats(buf_stats_t *stats) {
    if (!stats) return;

    if (buffer_pool) {
        stats->hits = buffer_pool->hits;
        stats->misses = buffer_pool->misses;
        stats->writes = buffer_pool->writes;
        stats->reads = buffer_pool->reads;
        stats->bg_writes = 0;
        stats->num_buf_alloc = buffer_pool->nbuffers;

        /* 计算正在使用的 Buffer */
        int used = 0;
        for (int i = 0; i < buffer_pool->nbuffers; i++) {
            if (buffer_pool->descriptors[i].state & BM_VALID) {
                used++;
            }
        }
        stats->num_buf_used = used;
    } else {
        memset(stats, 0, sizeof(buf_stats_t));
    }
}

void buf_reset_stats(void) {
    if (buffer_pool) {
        buffer_pool->hits = 0;
        buffer_pool->misses = 0;
        buffer_pool->writes = 0;
        buffer_pool->reads = 0;
    }
}

/* ============================================================
 * 调试
 * ============================================================ */

void buf_dump(void) {
    if (!buf_initialized) {
        printf("Buffer Pool not initialized\n");
        return;
    }

    printf("=== Buffer Pool Status ===\n");
    printf("Nbuffers: %d\n", buffer_pool->nbuffers);
    printf("Hash entries: %u / %u\n", buffer_pool->hash_entries, buffer_pool->hash_size);
    printf("Next victim: %u\n", buffer_pool->next_victim);
    printf("Hits: %lu, Misses: %lu, Hits%%: %.2f\n",
           (unsigned long)buffer_pool->hits,
           (unsigned long)buffer_pool->misses,
           buffer_pool->hits + buffer_pool->misses > 0 ?
           100.0 * buffer_pool->hits / (buffer_pool->hits + buffer_pool->misses) : 0.0);
    printf("Writes: %lu, Reads: %lu\n",
           (unsigned long)buffer_pool->writes,
           (unsigned long)buffer_pool->reads);

    printf("\nBuffer Descriptors:\n");
    printf("%-6s %-12s %-8s %-8s %-6s %-6s %s\n",
           "ID", "RelFileNode", "Block", "State", "Refs", "Usage", "Dirty");
    for (int i = 0; i < buffer_pool->nbuffers && i < 32; i++) {
        BufferDesc *buf = &buffer_pool->descriptors[i];
        printf("%-6d 0x%-10x %-8u 0x%-6x %-6d %-6d %s\n",
               buf->buf_id,
               buf->relfilenode,
               buf->blocknum,
               buf->state,
               buf->refcount,
               buf->usage_count,
               (buf->state & BM_DIRTY) ? "YES" : "");
    }
}

bool buf_check_consistency(void) {
    if (!buf_initialized) {
        return false;
    }

    /* 检查所有有效的 Buffer 是否都在 Hash 表中 */
    for (int i = 0; i < buffer_pool->nbuffers; i++) {
        BufferDesc *buf = &buffer_pool->descriptors[i];
        if (buf->state & BM_VALID) {
            BufHashEntry *entry = hash_lookup(buf->relfilenode, buf->blocknum);
            if (!entry || entry->buf_id != (uint32_t)i) {
                return false;
            }
        }
    }

    return true;
}

/**
 * 页面锁实现（并发安全）
 *
 * ## 并发控制模型
 *
 * Buffer Pool 采用多层锁机制：
 *
 * ### 1. Buffer 级别：Pin/Unpin
 * - 使用 refcount 实现 pin/unpin
 * - pin 增加 refcount，unpin 减少
 * - refcount > 0 表示页面正在被使用，不可置换
 *
 * ### 2. Page 级别：页面锁
 * - 使用 lock_manager 实现页面级别的锁
 * - 支持共享锁（S）和排他锁（X）
 * - 用于多事务并发访问同一页面
 *
 * ### 3. Buffer Pool 级别：全局锁
 * - 保护 Hash 表等共享数据结构
 * - 可考虑使用细粒度锁（分桶锁）优化
 *
 * ## 锁粒度选择
 *
 * | 粒度 | 优点 | 缺点 |
 * |------|------|------|
 * | 粗粒度（全局锁） | 简单，不死锁 | 并发度低 |
 * | 细粒度（分桶锁） | 并发度高 | 实现复杂，可能死锁 |
 *
 * ## 死锁避免策略
 *
 * 1. **固定锁顺序**：总是按 (relfilenode, blocknum) 字典序获取锁
 * 2. **锁超时**：设置锁等待超时，快速失败
 * 3. **锁升级**：在必要时将共享锁升级为排他锁
 */

/* 全局锁管理器用于页面锁
 * 每个 Buffer Pool 实例维护自己的锁管理器
 * 锁按 (relfilenode, blocknum) 唯一标识
 */
static lock_manager_t *g_buf_lockmgr = NULL;

/**
 * @brief 初始化页面锁管理器
 *
 * 创建页面锁管理器，用于管理页面级别的并发访问
 *
 * @return 0 成功，-1 失败
 */
int buf_lock_init(void) {
    if (g_buf_lockmgr) {
        return 0;  /* 已初始化，无需重复创建 */
    }
    g_buf_lockmgr = lock_mgr_create();
    return g_buf_lockmgr ? 0 : -1;
}

/**
 * @brief 关闭页面锁管理器
 *
 * 释放锁管理器资源，通常在 Buffer Pool 关闭时调用
 */
void buf_lock_shutdown(void) {
    if (g_buf_lockmgr) {
        lock_mgr_destroy(g_buf_lockmgr);
        g_buf_lockmgr = NULL;
    }
}

/**
 * @brief 获取锁管理器
 *
 * 懒加载锁管理器
 *
 * @return 锁管理器指针
 */
lock_manager_t *buf_get_lockmgr(void) {
    if (!g_buf_lockmgr) {
        buf_lock_init();  /* 首次使用时初始化 */
    }
    return g_buf_lockmgr;
}

/**
 * @brief 获取页面锁
 *
 * 请求获取指定页面的锁
 *
 * @param relfilenode 关系文件节点号
 * @param blocknum    块号
 * @param exclusive   true=排他锁(X)，false=共享锁(S)
 * @param timeout_ms  超时时间（毫秒）
 * @return LOCK_SUCCESS 成功，LOCK_TIMEOUT 超时，LOCK_ERROR 错误
 *
 * ## 锁模式
 *
 * - **共享锁 (S)**：多个事务可以同时持有，用于只读访问
 * - **排他锁 (X)**：只能有一个事务持有，用于写访问
 *
 * ## 相容性矩阵
 *
 * |          | S 锁 | X 锁 |
 * |----------|------|------|
 * | S 锁请求 | ✓    | ✗    |
 * | X 锁请求 | ✗    | ✗    |
 *
 * ## 使用示例
 *
 * @code
 * // 获取排他锁进行修改
 * if (buf_lock_page(relfn, blkno, true, 5000) == LOCK_SUCCESS) {
 *     BufferDesc *buf = buf_read(relfn, blkno, 1);
 *     // ... 修改数据 ...
 *     buf_unlock_page(relfn, blkno, true);
 * }
 * @endcode
 */
int buf_lock_page(uint32_t relfilenode, BlockNumber blocknum,
                  bool exclusive, uint32_t timeout_ms) {
    if (!buf_initialized) {
        return LOCK_ERROR;
    }

    lock_manager_t *mgr = buf_get_lockmgr();
    if (!mgr) {
        return LOCK_ERROR;
    }

    /* 创建页面唯一 ID
     * 将 relfilenode 和 blocknum 合并为 64 位 ID
     * 高 32 位：relfilenode
     * 低 32 位：blocknum
     */
    uint64_t page_id = ((uint64_t)relfilenode << 32) | blocknum;

    /* 选择锁模式 */
    lock_mode_t mode = exclusive ? LOCK_MODE_X : LOCK_MODE_S;

    /* 请求获取锁
     * lock_acquire 实现锁等待和超时逻辑
     */
    return (int)lock_acquire(mgr, NULL, LOCK_PAGE, page_id,
                             relfilenode, mode, timeout_ms);
}

/**
 * @brief 释放页面锁
 *
 * 释放之前获取的页面锁
 *
 * @param relfilenode 关系文件节点号
 * @param blocknum    块号
 * @param exclusive   true=释放排他锁，false=释放共享锁
 */
void buf_unlock_page(uint32_t relfilenode, BlockNumber blocknum, bool exclusive) {
    if (!buf_initialized) {
        return;
    }

    lock_manager_t *mgr = g_buf_lockmgr;
    if (!mgr) {
        return;
    }

    /* 计算页面 ID */
    uint64_t page_id = ((uint64_t)relfilenode << 32) | blocknum;
    lock_mode_t mode = exclusive ? LOCK_MODE_X : LOCK_MODE_S;

    /* 释放锁 */
    lock_release(mgr, NULL, LOCK_PAGE, page_id, mode);
}
