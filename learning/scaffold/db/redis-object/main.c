/**
 * @file main.c
 * @brief Redis 对象系统演示
 *
 * 演示 Redis 对象类型（String/Hash/List/Set/ZSet）、编码转换、
 * 内存优化（int/embstr/raw）、过期删除策略。
 *
 * @see engineering/src/redis/object.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ============================================================
 * Redis 对象类型与编码
 * ============================================================ */

/** Redis 对象类型 */
typedef enum {
    OBJ_STRING = 0,
    OBJ_LIST   = 1,
    OBJ_SET    = 2,
    OBJ_ZSET   = 3,
    OBJ_HASH   = 4
} ObjectType;

/** Redis 编码类型 */
typedef enum {
    /* String 编码 */
    OBJ_ENCODING_INT      = 0,   /* 整数编码 */
    OBJ_ENCODING_EMBSTR   = 1,   /* 短字符串（<=44B）*/
    OBJ_ENCODING_RAW      = 2,   /* 长字符串 */

    /* List 编码 */
    OBJ_ENCODING_LINKEDLIST = 3,  /* 双向链表 */
    OBJ_ENCODING_ZIPLIST   = 4,  /* 压缩列表 */

    /* Hash 编码 */
    OBJ_ENCODING_HT       = 5,   /* Hash 表 */
    OBJ_ENCODING_ZIPMAP   = 6,   /* (废弃) */

    /* Set 编码 */
    OBJ_ENCODING_INTSET   = 7,   /* 整数集合 */

    /* ZSet 编码 */
    OBJ_ENCODING_SKIPLIST = 8,  /* 跳表 + Hash */
    OBJ_ENCODING_ZIPLIST_ZSET = 9 /* Ziplist 编码 ZSet */
} EncodingType;

/** Redis 对象结构（简化版）*/
typedef struct redisObject {
    uint32_t    type;        /* 对象类型 */
    uint32_t    encoding;    /* 编码类型 */
    void       *ptr;         /* 指向实际数据结构 */
    int         lru;         /* LRU 时钟 */
    int         refcount;    /* 引用计数 */
} robj;

/** SDS 简单动态字符串 */
typedef struct {
    int         len;         /* 已使用字节 */
    int         alloc;       /* 分配字节 */
    char        flags;        /* 类型标志 */
    char        buf[];        /* 柔性数组 */
} sds;

/* ============================================================
 * [type] 对象类型展示
 * ============================================================ */

static const char *obj_type_name(int type)
{
    switch (type) {
    case OBJ_STRING: return "String";
    case OBJ_LIST:   return "List";
    case OBJ_SET:    return "Set";
    case OBJ_ZSET:   return "ZSet";
    case OBJ_HASH:    return "Hash";
    default:          return "Unknown";
    }
}

static const char *encoding_name(int enc)
{
    switch (enc) {
    case OBJ_ENCODING_INT:        return "int";
    case OBJ_ENCODING_EMBSTR:     return "embstr";
    case OBJ_ENCODING_RAW:        return "raw";
    case OBJ_ENCODING_LINKEDLIST: return "linkedlist";
    case OBJ_ENCODING_ZIPLIST:    return "ziplist";
    case OBJ_ENCODING_HT:          return "hashtable";
    case OBJ_ENCODING_INTSET:     return "intset";
    case OBJ_ENCODING_SKIPLIST:   return "skiplist";
    case OBJ_ENCODING_ZIPLIST_ZSET: return "ziplist";
    default:                      return "unknown";
    }
}

static void demo_object_types(void)
{
    printf("[type] ===== Redis 对象类型 =====\n\n");

    printf("[type] 5 种对象类型:\n");
    printf("[type]   String  -> 字符串/计数器/缓存\n");
    printf("[type]   List    -> 消息队列/时间线\n");
    printf("[type]   Set     -> 标签/好友/粉丝\n");
    printf("[type]   ZSet    -> 排行榜/带权重集合\n");
    printf("[type]   Hash    -> 对象/关系数据\n\n");

    printf("[type] 对象结构:\n");
    printf("[type]   typedef struct redisObject {\n");
    printf("[type]       uint32_t type;      /* OBJ_* */\n");
    printf("[type]       uint32_t encoding;  /* 内部编码 */\n");
    printf("[type]       void *ptr;           /* 实际数据 */\n");
    printf("[type]       int lru;            /* LRU 时钟 */\n");
    printf("[type]       int refcount;       /* 引用计数 */\n");
    printf("[type]   } robj;\n\n");
}

/* ============================================================
 * [string] String 编码转换
 * ============================================================ */

static void demo_string_encoding(void)
{
    printf("[string] ===== String 编码转换 =====\n\n");

    /* 编码转换规则 */
    printf("[string] 编码类型:\n");
    printf("[string]   int:    整数（long 类型）\n");
    printf("[string]   embstr: 短字符串 <= 44 字节\n");
    printf("[string]   raw:    长字符串 > 44 字节\n\n");

    /* int -> embstr -> raw 转换 */
    printf("[string] 编码转换链:\n");
    printf("[string]   SET n 100        -> int\n");
    printf("[string]   APPEND n abc     -> embstr (or raw)\n");
    printf("[string]   APPEND n xxx...  -> raw\n\n");

    /* 内存布局 */
    printf("[string] embstr 内存布局:\n");
    printf("[string]   robj (16B) + sdshdr (3B) + \"hi\" (2B) + \\0 = 21B\n");
    printf("[string]   一次 malloc，缓存友好\n\n");

    printf("[string] raw 内存布局:\n");
    printf("[string]   robj (16B)  --malloc--> sdshdr + data\n");
    printf("[string]   两次 malloc，对象和数据分离\n\n");

    /* 转换阈值（Redis 6.x）*/
    printf("[string] 编码阈值（Redis 6.x）:\n");
    printf("[string]   embstr 上限: 44 字节 (64 - sizeof(robj) - sizeof(sdshdr))\n");
    printf("[string]   超过 44B -> raw 编码\n");
    printf("[string]   整数追加字符串 -> int -> embstr -> raw\n\n");
}

/* ============================================================
 * [list] List 编码：ziplist vs linkedlist
 * ============================================================ */

static void demo_list_encoding(void)
{
    printf("[list] ===== List 编码对比 =====\n\n");

    printf("[list] ziplist (压缩列表):\n");
    printf("[list]   - 连续内存，节省空间\n");
    printf("[list]   - 适合 < 128 元素，元素 < 64B\n");
    printf("[list]   - 内存布局: zlbytes + zltail + zllen + entries + zlend\n\n");

    printf("[list] linkedlist (双向链表):\n");
    printf("[list]   - 节点独立，插入 O(1)\n");
    printf("[list]   - 适合大列表或长字符串\n");
    printf("[list]   - 每个节点独立 malloc\n\n");

    printf("[list] 转换条件:\n");
    printf("[list]   ziplist -> linkedlist:\n");
    printf("[list]     元素数量 > list-max-ziplist-entries (默认 512)\n");
    printf("[list]     或 任意元素 > list-max-ziplist-value (默认 64B)\n\n");

    printf("[list] 编码选择:\n");
    printf("[list]   小列表 -> ziplist（内存优化）\n");
    printf("[list]   大列表 -> linkedlist（插入效率）\n\n");
}

/* ============================================================
 * [hash] Hash 编码：ziplist vs hashtable
 * ============================================================ */

static void demo_hash_encoding(void)
{
    printf("[hash] ===== Hash 编码对比 =====\n\n");

    printf("[hash] ziplist 编码:\n");
    printf("[hash]   field1, value1, field2, value2, ... (连续存储)\n");
    printf("[hash]   适合 < 512 字段，每字段 < 64B\n\n");

    printf("[hash] hashtable 编码:\n");
    printf("[hash]   dict 结构，O(1) 查找\n");
    printf("[hash]   适合大 Hash 或长字段值\n\n");

    printf("[hash] 转换条件:\n");
    printf("[hash]   ziplist -> hashtable:\n");
    printf("[hash]     字段数 > hash-max-ziplist-entries (默认 512)\n");
    printf("[hash]     或 任意值 > hash-max-ziplist-value (默认 64B)\n\n");

    printf("[hash] 内部结构:\n");
    printf("[hash]   HSET user name jack age 25\n");
    printf("[hash]   ziplist: [\"name\", \"jack\", \"age\", \"25\"]\n");
    printf("[hash]   hashtable: {name: jack, age: 25}\n\n");
}

/* ============================================================
 * [zset] ZSet 编码：ziplist vs skiplist
 * ============================================================ */

static void demo_zset_encoding(void)
{
    printf("[zset] ===== ZSet 编码对比 =====\n\n");

    printf("[zset] ziplist 编码:\n");
    printf("[zset]   member1, score1, member2, score2, ...\n");
    printf("[zset]   适合 < 128 元素，member < 64B\n\n");

    printf("[zset] skiplist 编码:\n");
    printf("[zset]   zset 结构: skiplist + dict\n");
    printf("[zset]   skiplist: 有序，O(log n) 范围查询\n");
    printf("[zset]   dict:     O(1) 成员查找\n\n");

    printf("[zset] 转换条件:\n");
    printf("[zset]   ziplist -> skiplist:\n");
    printf("[zset]     元素数 > zset-max-ziplist-entries (默认 128)\n");
    printf("[zset]     或 member > zset-max-ziplist-value (默认 64B)\n\n");

    printf("[zset] 典型应用:\n");
    printf("[zset]   排行榜: ZADD leaderboard 100 \"user1\"\n");
    printf("[zset]   权重队列: ZRANGEBYSCORE salary 5000 10000\n\n");
}

/* ============================================================
 * [expire] 过期删除策略
 * ============================================================ */

static void demo_expire_strategy(void)
{
    printf("[expire] ===== 过期删除策略 =====\n\n");

    printf("[expire] 惰性删除 (Lazy Expunging):\n");
    printf("[expire]   - 访问时检查，过期则删除\n");
    printf("[expire]   - 优点: 节省 CPU\n");
    printf("[expire]   - 缺点: 过期键可能堆积\n\n");

    printf("[expire] 定时删除 (Cron):\n");
    printf("[expire]   - 定期扫描，删除过期键\n");
    printf("[expire]   - 每次随机抽 20 个 key 检查\n");
    printf("[expire]   - 平衡 CPU 和内存\n\n");

    printf("[expire] 定期删除:\n");
    printf("[expire]   - serverCron 每 100ms 执行\n");
    printf("[expire]   - 遍历所有 db，抽样检查\n");
    printf("[expire]   - 防止内存持续膨胀\n\n");

    printf("[expire] 生存时间结构:\n");
    printf("[expire]   Redis 会维护一个 expires dict\n");
    printf("[expire]   key -> expire_timestamp\n");
    printf("[expire]   VOLATILE_* 命令只操作 expires 字典\n\n");

    printf("[expire] 过期判断:\n");
    printf("[expire]   current_ms > key_expire(key)\n");
    printf("[expire]   精确到毫秒级\n\n");
}

/* ============================================================
 * [refcount] 引用计数与对象共享
 * ============================================================ */

static void demo_refcount(void)
{
    printf("[refcount] ===== 引用计数 =====\n\n");

    printf("[refcount] 引用计数机制:\n");
    printf("[refcount]   - 每个对象有 refcount 字段\n");
    printf("[refcount]   - INCR: refcount++\n");
    printf("[refcount]   - DECR: refcount--\n");
    printf("[refcount]   - refcount=0 时释放内存\n\n");

    printf("[refcount] 对象共享:\n");
    printf("[refcount]   - 共享对象池（0~9999 整数）\n");
    printf("[refcount]   - SET a 100, SET b 100 -> 共享同一对象\n");
    printf("[refcount]   - 节省内存\n\n");

    printf("[refcount] 禁止共享的情况:\n");
    printf("[refcount]   - 命令参数对象\n");
    printf("[refcount]   - 正在被修改的对象\n");
    printf("[refcount]   - AOF/RDB 中持久化的对象\n\n");
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("   Redis 对象系统\n");
    printf("   String/Hash/List/Set/ZSet 编码\n");
    printf("========================================\n\n");

    demo_object_types();
    demo_string_encoding();
    demo_list_encoding();
    demo_hash_encoding();
    demo_zset_encoding();
    demo_expire_strategy();
    demo_refcount();

    printf("========================================\n");
    printf("   演示结束\n");
    printf("========================================\n");
    return 0;
}
