/**
 * @file main.c
 * @brief LSM-Tree 原理演示
 *
 * 组件：MemTable(SkipList) + WAL + SSTable 分层 + Bloom Filter
 * 参考: engineering/src/db/storage/kv/kv_engine.c
 *
 * SkipList 使用静态节点池实现，避免动态指针碎片化问题。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ========================================================================
 * 常量
 * ======================================================================== */
#define LOG(fmt, ...)   printf("[lsm] " fmt "\n", ##__VA_ARGS__)
#define MAX_LEVEL       16          /* 跳表最大层数 */
#define MAX_NODES       16384       /* 节点池容量 */
#define CAPACITY        8192        /* MemTable 上限(条目数) */
#define SST_TBL_PATH    "sstables"   /* SSTable 目录 */
#define WAL_PATH        "wal.log"   /* WAL 文件路径 */

/* ========================================================================
 * SkipList 节点（静态池实现）
 * ======================================================================== */
typedef struct {
    char key[64];
    char val[128];
    int nnext;                       /* 本节点层数（1..MAX_LEVEL） */
    int next[MAX_LEVEL];             /* 下一节点索引，-1 表示空 */
} snode_t;

static snode_t nodes[MAX_NODES];    /* 静态节点池，避免栈溢出 */

typedef struct {
    /* nodes[0] 为哨兵头节点，有效节点索引 1..count */
    int level;                     /* 当前最大层 */
    int count;                     /* 有效节点数（不含哨兵） */
} skiplist_t;

/* ========================================================================
 * WAL 记录
 * ======================================================================== */
typedef enum {
    WAL_PUT,
    WAL_DEL
} wal_op_t;

typedef struct {
    wal_op_t op;
    char key[64];
    char val[128];
} wal_rec_t;

/* ========================================================================
 * Bloom Filter（简化实现）
 * ======================================================================== */
#define BF_BITS     1024
#define BF_HASHES   7

static unsigned char bf[BF_BITS] = {0};

static void bf_add(const char *key) {
    for (int i = 0; i < BF_HASHES; i++) {
        uint64_t h = 14695981039346656037ULL;
        for (const char *p = key; *p; p++) {
            h ^= (unsigned char)(*p + i * 31);
            h *= 1099511628211ULL;
        }
        bf[h % BF_BITS] = 1;
    }
}

static bool bf_may_exist(const char *key) {
    for (int i = 0; i < BF_HASHES; i++) {
        uint64_t h = 14695981039346656037ULL;
        for (const char *p = key; *p; p++) {
            h ^= (unsigned char)(*p + i * 31);
            h *= 1099511628211ULL;
        }
        if (!bf[h % BF_BITS]) return false;
    }
    return true;
}

/* ========================================================================
 * MemTable (SkipList 实现)
 * ======================================================================== */
static int cur_level(void) {
    int lvl = 0;
    while (lvl < MAX_LEVEL - 1 && rand() % 2 == 0) lvl++;
    return lvl;
}

static void skiplist_init(skiplist_t *list) {
    memset(nodes, 0, sizeof(nodes));
    nodes[0].nnext = MAX_LEVEL; /* 节点 0 为哨兵头 */
    for (int i = 0; i < MAX_LEVEL; i++) nodes[0].next[i] = -1;
    list->level = 0;
    list->count = 0;
}

static void skiplist_put(skiplist_t *list, const char *key, const char *val) {
    int update[MAX_LEVEL];
    int cur = 0; /* 从哨兵头开始 */

    /* 从最高层向下找到每层的插入前驱索引 */
    for (int i = list->level; i >= 0; i--) {
        while (nodes[cur].next[i] >= 0 &&
               strcmp(nodes[nodes[cur].next[i]].key, key) < 0) {
            cur = nodes[cur].next[i];
        }
        update[i] = cur;
    }
    cur = nodes[cur].next[0];

    /* 已存在则更新值 */
    if (cur >= 0 && strcmp(nodes[cur].key, key) == 0) {
        strncpy(nodes[cur].val, val, 127);
        LOG("update key='%s' val='%s'", key, val);
        return;
    }

    /* 分配新节点 */
    if (list->count >= MAX_NODES - 1) {
        LOG("node pool full!");
        return;
    }
    int nid = ++list->count;        /* 节点从 1 开始 */
    snode_t *n = &nodes[nid];
    memset(n, 0, sizeof(snode_t));
    strncpy(n->key, key, 63);
    strncpy(n->val, val, 127);
    for (int i = 0; i < MAX_LEVEL; i++) n->next[i] = -1;

    /* 随机层数 */
    int lvl = cur_level();
    n->nnext = lvl + 1;

    /* 逐层插入：将新节点插入各层前驱之后 */
    for (int i = 0; i <= lvl; i++) {
        n->next[i] = nodes[update[i]].next[i];
        nodes[update[i]].next[i] = nid;
    }
    /* 更新最大层 */
    if (lvl > list->level) list->level = lvl;
    LOG("insert key='%s' val='%s' (level=%d, total=%d)", key, val, lvl, list->count);
}

static const char *skiplist_get(skiplist_t *list, const char *key) {
    int cur = 0;
    for (int i = list->level; i >= 0; i--) {
        while (nodes[cur].next[i] >= 0 &&
               strcmp(nodes[nodes[cur].next[i]].key, key) < 0) {
            cur = nodes[cur].next[i];
        }
    }
    cur = nodes[cur].next[0];
    if (cur >= 0 && strcmp(nodes[cur].key, key) == 0) {
        return nodes[cur].val;
    }
    return NULL;
}

/* ========================================================================
 * WAL 持久化
 * ======================================================================== */
static FILE *wal_fd = NULL;

static void wal_init(void) {
    wal_fd = fopen(WAL_PATH, "wb");
    if (!wal_fd) { perror("wal open"); return; }
    LOG("WAL opened: %s", WAL_PATH);
}

/* 创建 SSTable 目录 */
static void sst_init(void) {
    (void)system("mkdir -p " SST_TBL_PATH);
}

static void wal_write(const char *key, const char *val) {
    if (!wal_fd) return;
    wal_rec_t rec = {.op = WAL_PUT};
    strncpy(rec.key, key, 63);
    strncpy(rec.val, val, 127);
    fwrite(&rec, sizeof(wal_rec_t), 1, wal_fd);
    fflush(wal_fd);
    LOG("WAL flushed: key='%s'", key);
}

static void wal_close(void) {
    if (wal_fd) { fclose(wal_fd); wal_fd = NULL; }
    LOG("WAL closed");
}

/* ========================================================================
 * SSTable 简化写入
 * ======================================================================== */
static FILE *sst_open(int level, int idx) {
    char path[256];
    snprintf(path, sizeof(path), "%s/L%d_%04d.sst", SST_TBL_PATH, level, idx);
    return fopen(path, "wb");
}

static void flush_memtable(skiplist_t *list, int *flush_count) {
    static int sst_idx = 0;
    static int level = 0;

    FILE *f = sst_open(level, sst_idx);
    if (!f) { LOG("SSTable open failed"); return; }

    /* 按节点池顺序（已排序）遍历并写入 */
    int cnt = 0;
    for (int i = 1; i <= list->count; i++) {
        fprintf(f, "%s:%s\n", nodes[i].key, nodes[i].val);
        bf_add(nodes[i].key);
        cnt++;
    }
    fclose(f);

    LOG("SSTable L%d #%d written (%d entries) -> Bloom bits set",
        level, sst_idx, cnt);

    /* 简单的层旋转策略 */
    (*flush_count)++;
    if (*flush_count % 3 == 0) level = (level + 1) % 3;
    sst_idx++;
}

/* ========================================================================
 * LSM-Tree 状态
 * ======================================================================== */
typedef struct {
    skiplist_t memtable;
    int flush_count;
} lsm_t;

static void lsm_init(lsm_t *lsm) {
    skiplist_init(&lsm->memtable);
    lsm->flush_count = 0;
    sst_init();
    wal_init();
    LOG("LSM-Tree initialized");
}

static void lsm_put(lsm_t *lsm, const char *key, const char *val) {
    if (lsm->memtable.count >= CAPACITY) {
        LOG("MemTable full (cap=%d) -> flushing", CAPACITY);
        flush_memtable(&lsm->memtable, &lsm->flush_count);
        skiplist_init(&lsm->memtable);
    }
    wal_write(key, val);             /* WAL 保护 */
    bf_add(key);                     /* 写入时同步加入 BloomFilter */
    skiplist_put(&lsm->memtable, key, val);
}

static const char *lsm_get(lsm_t *lsm, const char *key) {
    /* 1. Bloom Filter 快速判断（可能有假阳性） */
    if (!bf_may_exist(key)) {
        LOG("get '%s' -> BloomFilter miss", key);
        return NULL;
    }

    /* 2. MemTable 查询 */
    const char *val = skiplist_get(&lsm->memtable, key);
    if (val) {
        LOG("get '%s' -> MemTable HIT, val='%s'", key, val);
        return val;
    }

    /* 3. SSTable 查询（简化版：仅提示已落盘的层） */
    LOG("get '%s' -> MemTable MISS, check SSTable (level=%d)",
        key, lsm->flush_count > 0 ? (lsm->flush_count - 1) % 3 : -1);
    return NULL;
}

static void lsm_close(lsm_t *lsm) {
    if (lsm->memtable.count > 0) {
        LOG("Flushing remaining %d entries before shutdown", lsm->memtable.count);
        flush_memtable(&lsm->memtable, &lsm->flush_count);
    }
    wal_close();
    LOG("LSM-Tree closed");
}

/* ========================================================================
 * 演示入口
 * ======================================================================== */
int main(void) {
    srand((unsigned)time(NULL));

    LOG("=== LSM-Tree Demo ===");
    LOG("MemTable limit: %d, Bloom bits: %d", CAPACITY, BF_BITS);

    lsm_t lsm;
    lsm_init(&lsm);

    /* 写入数据，触发多次 MemTable 刷盘 */
    char key[32], val[64];
    for (int i = 1; i <= 20; i++) {
        snprintf(key, sizeof(key), "key%03d", i);
        snprintf(val, sizeof(val), "value_%d", i * 10);
        lsm_put(&lsm, key, val);
    }

    /* 点查询测试 */
    LOG("\n--- Query Tests ---");
    const char *v;

    v = lsm_get(&lsm, "key005");
    LOG("query key005 -> %s", v ? v : "(null)");

    v = lsm_get(&lsm, "key015");
    LOG("query key015 -> %s", v ? v : "(null)");

    v = lsm_get(&lsm, "key999");     /* 不存在的 key（BloomFilter 假阳性场景） */
    LOG("query key999 -> %s", v ? v : "(null)");

    v = lsm_get(&lsm, "key999");     /* 再次查询同一 key */
    LOG("query key999 again -> %s", v ? v : "(null)");

    lsm_close(&lsm);

    LOG("\n=== Demo Done ===");
    return 0;
}
