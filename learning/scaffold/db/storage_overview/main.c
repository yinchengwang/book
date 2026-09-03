/**
 * @file main.c
 * @brief 存储引擎核心组件演示
 *
 * 演示 Catalog、Buffer Pool、Heap、BTree 四大核心组件的协作关系。
 * 所有输出使用 [tag] 格式标记来源模块。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * 模拟数据类型（真实实现参见 engineering/src/db/storage/）
 * ============================================================ */

/** 对象标识符 */
typedef uint32_t Oid;
#define InvalidOid ((Oid)0)

/** 页面大小 */
#define PAGE_SIZE 8192

/** Buffer 描述符（模拟） */
typedef struct BufferDesc_s {
    uint32_t buf_id;
    Oid      relfilenode;
    uint32_t blocknum;
    int      usage_count;
    int      refcount;
    int      is_dirty;
    char     data[PAGE_SIZE];
} BufferDesc;

/** 页面头部（Heap） */
typedef struct PageHeaderData {
    uint32_t pd_lsn;
    uint16_t pd_flags;
    uint16_t pd_lower;
    uint16_t pd_upper;
    uint16_t pd_special;
} PageHeader;

/** LinePointer（指向元组） */
typedef struct {
    uint32_t t_off;
    uint8_t  t_flags;
} LinePointer;

/** BTree 页面头部 */
typedef struct {
    uint16_t btpo_flags;
    uint16_t btpo_level;
    uint32_t btpo_prev;
    uint32_t btpo_next;
    uint16_t btpo_count;
} BTPageHeader;

/** 表信息 */
typedef struct {
    Oid  oid;
    char name[64];
    Oid  filenode;
    int  npages;
} TableInfo;

/** 索引信息 */
typedef struct {
    Oid  oid;
    Oid  table_oid;
    char name[64];
    int  nkeys;
} IndexInfo;

/* ============================================================
 * 静态模拟数据
 * ============================================================ */

/** 模拟系统表 */
static TableInfo system_tables[] = {
    {100, "pg_class",     1000, 1},
    {101, "pg_attribute", 1001, 2},
    {102, "pg_index",     1002, 1},
    {103, "pg_proc",      1003, 1},
    {104, "users",        2000, 3},
    {105, "orders",       2001, 5},
};

static const int num_tables = sizeof(system_tables) / sizeof(system_tables[0]);

/** 模拟索引 */
static IndexInfo system_indexes[] = {
    {200, 104, "users_pkey",   1},
    {201, 105, "orders_pkey",  1},
    {202, 104, "users_email",  1},
};

static const int num_indexes = sizeof(system_indexes) / sizeof(system_indexes[0]);

/** 模拟 Buffer Pool */
static BufferDesc buffer_pool[4];
static int buf_clock = 0;

/* ============================================================
 * [catalog] Catalog 系统表管理
 * ============================================================ */

static void demo_catalog(void)
{
    printf("[catalog] ===== Catalog 系统表管理 =====\n");

    /* 1. 模拟 OID 分配 */
    printf("[catalog] OID 分配:\n");
    printf("[catalog]   pg_class     -> OID %u (filenode %u)\n",
           system_tables[0].oid, system_tables[0].filenode);
    printf("[catalog]   users        -> OID %u (filenode %u)\n",
           system_tables[4].oid, system_tables[4].filenode);
    printf("[catalog]   orders       -> OID %u (filenode %u)\n",
           system_tables[5].oid, system_tables[5].filenode);

    /* 2. 表查找 */
    printf("[catalog] 表查找 (name='users'):\n");
    for (int i = 0; i < num_tables; i++) {
        if (strcmp(system_tables[i].name, "users") == 0) {
            printf("[catalog]   found: OID=%u, filenode=%u, npages=%d\n",
                   system_tables[i].oid, system_tables[i].filenode,
                   system_tables[i].npages);
        }
    }

    /* 3. 索引列表 */
    printf("[catalog] 表 'users' 的索引:\n");
    for (int i = 0; i < num_indexes; i++) {
        if (system_indexes[i].table_oid == system_tables[4].oid) {
            printf("[catalog]   - %s (OID=%u, nkeys=%d)\n",
                   system_indexes[i].name,
                   system_indexes[i].oid,
                   system_indexes[i].nkeys);
        }
    }

    /* 4. 元数据缓存 */
    printf("[catalog] 元数据缓存: %d 表, %d 索引 已注册\n",
           num_tables, num_indexes);
    printf("\n");
}

/* ============================================================
 * [buffer] Buffer Pool 缓存管理
 * ============================================================ */

static BufferDesc *buf_read(Oid relfilenode, uint32_t blocknum)
{
    /* 查找现有 Buffer */
    for (int i = 0; i < 4; i++) {
        if (buffer_pool[i].relfilenode == relfilenode &&
            buffer_pool[i].blocknum == blocknum) {
            buffer_pool[i].usage_count++;
            buffer_pool[i].refcount++;
            return &buffer_pool[i];
        }
    }

    /* Clock-Sweep 置换策略 */
    int victim = -1;
    for (int tries = 0; tries < 8; tries++) {
        int idx = buf_clock % 4;
        if (buffer_pool[idx].refcount == 0) {
            if (buffer_pool[idx].usage_count == 0) {
                victim = idx;
                break;
            }
            buffer_pool[idx].usage_count--;
        }
        buf_clock++;
    }

    if (victim < 0) victim = buf_clock % 4;
    buf_clock++;

    /* 换出脏页 */
    if (buffer_pool[victim].is_dirty) {
        printf("[buffer]   [flush] 写回脏页 block=%u\n",
               buffer_pool[victim].blocknum);
    }

    /* 换入新页 */
    buffer_pool[victim].buf_id = victim;
    buffer_pool[victim].relfilenode = relfilenode;
    buffer_pool[victim].blocknum = blocknum;
    buffer_pool[victim].usage_count = 1;
    buffer_pool[victim].refcount = 1;
    buffer_pool[victim].is_dirty = 0;
    return &buffer_pool[victim];
}

static void buf_unpin(BufferDesc *buf)
{
    if (buf) buf->refcount--;
}

static void buf_dirty(BufferDesc *buf)
{
    if (buf) buf->is_dirty = 1;
}

static void demo_buffer(void)
{
    printf("[buffer] ===== Buffer Pool 缓存管理 =====\n");

    /* 初始化模拟 Buffer Pool */
    for (int i = 0; i < 4; i++) {
        buffer_pool[i].refcount = 0;
        buffer_pool[i].usage_count = 0;
        buffer_pool[i].is_dirty = 0;
    }

    /* 1. 读取页面（触发未命中 -> 置换） */
    printf("[buffer] 读取页面 (relfilenode=2000, block=0):\n");
    BufferDesc *buf1 = buf_read(2000, 0);
    printf("[buffer]   -> 分配 Buffer #%u, refcount=%d\n",
           buf1->buf_id, buf1->refcount);

    /* 2. 再次读取同一页面（命中） */
    printf("[buffer] 再次读取 (relfilenode=2000, block=0):\n");
    BufferDesc *buf2 = buf_read(2000, 0);
    printf("[buffer]   -> 命中 Buffer #%u, usage_count=%d\n",
           buf2->buf_id, buf2->usage_count);

    /* 3. 标记脏页 */
    printf("[buffer] 修改页面数据，标记脏:\n");
    buf_dirty(buf1);
    printf("[buffer]   -> Buffer #%u 已标记为脏\n", buf1->buf_id);

    /* 4. 释放引用 */
    buf_unpin(buf1);
    buf_unpin(buf2);
    printf("[buffer]   -> refcount 回到 %d\n", buf1->refcount);

    /* 5. Clock-Sweep 置换演示 */
    printf("[buffer] Clock-Sweep 置换:\n");
    for (int i = 0; i < 6; i++) {
        BufferDesc *b = buf_read(2001, i);
        printf("[buffer]   置换 -> Buffer #%u (block=%u)\n",
               b->buf_id, b->blocknum);
    }

    printf("[buffer] 命中率: %.0f%% (8次请求, 2次命中)\n", 25.0);
    printf("\n");
}

/* ============================================================
 * [heap] Heap 堆表存储
 * ============================================================ */

static void demo_heap(void)
{
    printf("[heap] ===== Heap 堆表存储 =====\n");

    /* 1. 页面布局 */
    printf("[heap] 页面布局 (PAGE_SIZE=%d):\n", PAGE_SIZE);
    PageHeader hdr;
    hdr.pd_lsn    = 0x00010001;
    hdr.pd_flags  = 0x0000;
    hdr.pd_lower  = 24 + 3 * 6;  /* 头部 + 3个LinePointer */
    hdr.pd_upper  = 8192 - 256; /* 预留空间 */
    hdr.pd_special = 8192;      /* 无特殊空间 */
    printf("[heap]   pd_lsn=0x%08X, pd_lower=%u, pd_upper=%u\n",
           hdr.pd_lsn, hdr.pd_lower, hdr.pd_upper);
    printf("[heap]   空闲空间 = %d 字节\n", hdr.pd_upper - hdr.pd_lower);

    /* 2. LinePointer 数组 */
    printf("[heap] LinePointer 数组:\n");
    LinePointer lp[3] = {
        {24,  0x01},   /* 有效元组 */
        {256, 0x01},   /* 有效元组 */
        {1024,0x00}    /* 已删除 */
    };
    for (int i = 0; i < 3; i++) {
        printf("[heap]   lp[%d]: offset=%u, flags=0x%02X %s\n",
               i, lp[i].t_off, lp[i].t_flags,
               (lp[i].t_flags & 0x01) ? "(LIVE)" : "(DEAD)");
    }

    /* 3. 元组插入 */
    printf("[heap] 元组插入流程:\n");
    int tuple_size = 128;
    int free_before = hdr.pd_upper - hdr.pd_lower;
    hdr.pd_upper -= tuple_size;
    hdr.pd_lower += 6;
    int free_after = hdr.pd_upper - hdr.pd_lower;
    printf("[heap]   插入 %d 字节元组\n", tuple_size);
    printf("[heap]   空闲空间: %d -> %d 字节\n", free_before, free_after);
    printf("[heap]   分配 LinePointer -> pd_lower += 6\n");

    /* 4. 页面修剪 */
    printf("[heap] 页面修剪 (prune):\n");
    printf("[heap]   扫描死亡元组: 1 个已删除\n");
    printf("[heap]   压缩存活元组到页面底部\n");
    printf("[heap]   回收空间: ~200 字节\n");

    /* 5. 堆页面类型 */
    printf("[heap] 页面类型:\n");
    printf("[heap]   HEAP_PAGE_TYPE = 0x00 (普通堆页面)\n");
    printf("[heap]   BTP_LEAF      = 0x0001 (BTree叶子，索引专用)\n");
    printf("[heap]   BTP_INTERNAL  = 0x0004 (BTree内部节点)\n");
    printf("\n");
}

/* ============================================================
 * [btree] BTree 索引
 * ============================================================ */

static void demo_btree(void)
{
    printf("[btree] ===== BTree 索引 =====\n");

    /* 1. BTree 页面结构 */
    printf("[btree] BTree 页面结构:\n");
    BTPageHeader bthdr;
    bthdr.btpo_flags = 0x0001;  /* 叶子页面 */
    bthdr.btpo_level = 0;
    bthdr.btpo_prev   = 0;
    bthdr.btpo_next   = 2;
    bthdr.btpo_count  = 5;
    printf("[btree]   btpo_flags=0x%04X (LEAF)\n", bthdr.btpo_flags);
    printf("[btree]   btpo_level=%u (0=叶子, >0=内部节点)\n", bthdr.btpo_level);
    printf("[btree]   btpo_prev=%u, btpo_next=%u (双向链表)\n",
           bthdr.btpo_prev, bthdr.btpo_next);
    printf("[btree]   btpo_count=%u 条目\n", bthdr.btpo_count);

    /* 2. 树层级 */
    printf("[btree] BTree 树结构:\n");
    printf("[btree]   Level 2: [根页面] (btpo_level=2)\n");
    printf("[btree]            -> 3 个内部页面 (btpo_level=1)\n");
    printf("[btree]                -> 12 个叶子页面 (btpo_level=0)\n");
    printf("[btree]   键值分布: [10][20][30]...[120]\n");

    /* 3. 键值定位 */
    printf("[btree] 键值定位流程 (search key=55):\n");
    int levels[] = {2, 1, 0};
    int keys[]   = {50, 60, 55};
    for (int i = 0; i < 3; i++) {
        printf("[btree]   Level %d: 比较键值 %d -> %s\n",
               levels[i], keys[i],
               keys[i] < 60 ? "向右" : "命中");
    }
    printf("[btree]   -> 定位到叶子页面 #5\n");
    printf("[btree]   -> 扫描叶子页面查找精确键\n");
    printf("[btree]   -> 返回 heap_ptr (page=%u, offset=%u)\n", 10, 256);

    /* 4. 插入操作 */
    printf("[btree] 键值插入 (key=55):\n");
    printf("[btree]   定位插入位置: 叶子页面 #5\n");
    printf("[btree]   检查唯一性: 无重复 -> 允许\n");
    printf("[btree]   插入键值，更新 LinePointer\n");
    printf("[btree]   检查页面分裂: 已满 -> 分裂\n");
    printf("[btree]   上层内部节点同步更新下链\n");

    /* 5. 内部节点 vs 叶子节点 */
    printf("[btree] 内部节点 vs 叶子节点:\n");
    printf("[btree]   内部: [ downlink | key | downlink | key | ... ]\n");
    printf("[btree]         downlink 指向子页面\n");
    printf("[btree]   叶子: [ key | heap_ptr | key | heap_ptr | ... ]\n");
    printf("[btree]         heap_ptr 指向堆表元组\n");
    printf("[btree]         叶子页面间双向链表连接\n");
    printf("\n");
}

/* ============================================================
 * [page] 页面布局总览
 * ============================================================ */

static void demo_page_layout(void)
{
    printf("[page] ===== 页面布局结构 =====\n");

    /* Heap 页面 */
    printf("[page] Heap 堆表页面布局:\n");
    printf("[page] +--------------------------+\n");
    printf("[page] | PageHeader (24B)        |\n");
    printf("[page] |   pd_lsn, pd_flags,      |\n");
    printf("[page] |   pd_lower, pd_upper     |\n");
    printf("[page] +--------------------------+\n");
    printf("[page] | LinePointer[0] (6B)     |\n");
    printf("[page] | LinePointer[1] (6B)     |\n");
    printf("[page] | ...                      |\n");
    printf("[page] +--------------------------+\n");
    printf("[page] | Tuple[0] (var-size)     |\n");
    printf("[page] | Tuple[1] (var-size)     |\n");
    printf("[page] | ...         ^            |\n");
    printf("[page] +---------- | ------------+\n");
    printf("[page] | Free Space | (pd_upper) |\n");
    printf("[page] +------------|-------------+\n");
    printf("[page] | pd_special (通常=8192) |\n");
    printf("[page] +--------------------------+\n");

    printf("\n");

    /* BTree 页面 */
    printf("[page] BTree 索引页面布局:\n");
    printf("[page] +--------------------------+\n");
    printf("[page] | BTPageHeader (20B)      |\n");
    printf("[page] |   btpo_flags, level,     |\n");
    printf("[page] |   prev, next, count      |\n");
    printf("[page] +--------------------------+\n");
    printf("[page] | Items[] (按偏移数组)    |\n");
    printf("[page] |   - 内部: [blkno|key]    |\n");
    printf("[page] |   - 叶子: [key|heap_ptr] |\n");
    printf("[page] +----------^--------------+\n");
    printf("[page] | Free Space |            |\n");
    printf("[page] +------------|-------------+\n");
    printf("[page] | Special Space           |\n");
    printf("[page] |   (索引类型特定数据)    |\n");
    printf("[page] +--------------------------+\n");
    printf("\n");
}

/* ============================================================
 * 组件协作流程
 * ============================================================ */

static void demo_workflow(void)
{
    printf("[workflow] ===== 组件协作流程 =====\n");

    printf("[workflow] 场景: 插入元组 'id=100' 到表 'users'\n\n");

    printf("[workflow] Step 1: Catalog 查找表信息\n");
    printf("[workflow]   catalog_lookup_table('users') -> OID=104\n");
    printf("[workflow]   catalog_get_table(104) -> filenode=2000\n");
    printf("[workflow]   catalog_get_indexes(104) -> users_pkey\n\n");

    printf("[workflow] Step 2: Buffer Pool 获取/分配页面\n");
    printf("[workflow]   buf_new_page(2000) -> Buffer #0\n");
    printf("[workflow]   如果已满，分配新页面 (blocknum=3)\n");
    printf("[workflow]   buf_pin(buffer)\n\n");

    printf("[workflow] Step 3: Heap 插入元组\n");
    printf("[workflow]   heap_page_add_tuple(page, tuple, 128)\n");
    printf("[workflow]   分配 LinePointer: pd_lower += 6\n");
    printf("[workflow]   写入元组数据: pd_upper -= 128\n");
    printf("[workflow]   buf_dirty(buffer)\n\n");

    printf("[workflow] Step 4: BTree 更新索引\n");
    printf("[workflow]   btinsert(users_pkey, [100], 1, heap_ptr)\n");
    printf("[workflow]   定位叶子页面，插入键值\n");
    printf("[workflow]   如需要，触发页面分裂和父节点更新\n\n");

    printf("[workflow] Step 5: 提交事务\n");
    printf("[workflow]   buf_unpin(buffer)\n");
    printf("[workflow]   WAL 写日志（确保持久性）\n");
    printf("[workflow]   Catalog 缓存可能失效并重新加载\n\n");

    printf("[workflow] ===== 数据流总结 =====\n");
    printf("[workflow] SQL -> Catalog(元数据) -> Buffer Pool(缓存)\n");
    printf("[workflow]       -> Heap/BTree(存储) -> WAL(日志) -> 磁盘\n");
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("   存储引擎核心组件演示\n");
    printf("   PostgreSQL 风格架构\n");
    printf("========================================\n\n");

    demo_catalog();
    demo_buffer();
    demo_heap();
    demo_btree();
    demo_page_layout();
    demo_workflow();

    printf("========================================\n");
    printf("   演示结束\n");
    printf("========================================\n");
    return 0;
}
