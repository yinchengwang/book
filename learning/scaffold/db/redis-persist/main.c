/**
 * @file main.c
 * @brief Redis 持久化演示
 *
 * 演示 Redis RDB、AOF、混合持久化三种模式。
 * 包含 save/bgsave 配置、fork 机制、AOF 重写。
 *
 * @see engineering/src/redis/persist.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ============================================================
 * 持久化类型
 * ============================================================ */

/** 持久化模式 */
typedef enum {
    PERSIST_NONE     = 0,
    PERSIST_RDB      = 1,
    PERSIST_AOF      = 2,
    PERSIST_RDB_AOF  = 3   /* 混合持久化 */
} PersistMode;

/** RDB 信息 */
typedef struct {
    time_t last_save;      /* 上次 save 时间 */
    int dirty;             /* 自上次 save 后的修改数 */
    int bgsave_in_progress;
    time_t last_bgsave_time;
} RdbInfo;

/** AOF 状态 */
typedef struct {
    int enabled;
    uint64_t appendfsync;  /* always/everysec/no */
    uint64_t size;         /* AOF 文件大小 */
    int rewrite_in_progress;
} AofInfo;

/* ============================================================
 * [rdb] RDB 持久化
 * ============================================================ */

static void demo_rdb_save(void)
{
    printf("[rdb] ===== RDB 持久化 =====\n\n");

    printf("[rdb] RDB 格式:\n");
    printf("[rdb]   magic: \"REDIS\" + version\n");
    printf("[rdb]   data: database #0 ~ #15\n");
    printf("[rdb]   each db: SELECTOR + key-value pairs\n");
    printf("[rdb]   EOF + checksum (CRC64)\n\n");

    printf("[rdb] save vs bgsave:\n");
    printf("[rdb]   save: 同步阻塞，阻塞主线程\n");
    printf("[rdb]   bgsave: fork 子进程，不阻塞\n\n");

    printf("[rdb] bgsave 流程:\n");
    printf("[rdb]   1. Redis 主进程 fork()\n");
    printf("[rdb]   2. 子进程创建 RDB 文件\n");
    printf("[rdb]   3. 完成后通知主进程\n");
    printf("[rdb]   4. 原子替换旧文件\n\n");

    printf("[rdb] fork 机制:\n");
    printf("[rdb]   - Copy-On-Write (COW) 机制\n");
    printf("[rdb]   - 父子进程共享页表\n");
    printf("[rdb]   - 写入时复制真实内存\n");
    printf("[rdb]   - 节省内存开销\n\n");

    printf("[rdb] 触发条件:\n");
    printf("[rdb]   - 定时触发: save m n (m 秒内 n 次修改)\n");
    printf("[rdb]   - shutdown 时\n");
    printf("[rdb]   - redis-cli bgsave\n");
    printf("[rdb]   - REPLICAOF none (主从切换)\n\n");
}

/* ============================================================
 * [aof] AOF 持久化
 * ============================================================ */

static void demo_aof_write(void)
{
    printf("[aof] ===== AOF 持久化 =====\n\n");

    printf("[aof] AOF 格式:\n");
    printf("[aof]   *3\\r\\n$3\\r\\nSET\\r\\n$3\\r\\nkey\\r\\n$5\\r\\nvalue\\r\\n\n");

    printf("[aof] 追加模式:\n");
    printf("[aof]   appendfsync everysec (默认)\n");
    printf("[aof]     - 每秒强制写入磁盘\n");
    printf("[aof]     - 最多丢失 1 秒数据\n");
    printf("[aof]   appendfsync always\n");
    printf("[aof]     - 每条命令同步写入\n");
    printf("[aof]     - 最多丢失 0 数据，IO 开销大\n");
    printf("[aof]   appendfsync no\n");
    printf("[aof]     - 由操作系统决定写入时机\n");
    printf("[aof]     - 可能丢失较多数据\n\n");
}

static void demo_aof_rewrite(void)
{
    printf("[aof] AOF 重写:\n\n");

    printf("[aof] 问题:\n");
    printf("[aof]   - INCR key 执行 1000 次 -> 1000 条日志\n");
    printf("[aof]   - AOF 文件膨胀\n\n");

    printf("[aof] 重写机制:\n");
    printf("[aof]   - bgrewriteaof 命令\n");
    printf("[aof]   - fork 子进程，读取内存重建 AOF\n");
    printf("[aof]   - 不需要读取旧 AOF\n");
    printf("[aof]   - 重写期间，新命令追加到重写缓冲区\n\n");

    printf("[aof] 重写缓冲区:\n");
    printf("[aof]   - 子进程开始重写后，主进程\n");
    printf("[aof]   - 将新命令写入 aof_rewrite_buf\n");
    printf("[aof]   - 重写完成后，追加到新 AOF\n\n");

    printf("[aof] 自动触发条件:\n");
    printf("[aof]   - AOF 文件 > auto-aof-rewrite-min-size\n");
    printf("[aof]   - 且 大小增长 > auto-aof-rewrite-percentage%%\n\n");
}

/* ============================================================
 * [mix] 混合持久化（Redis 4.0+）
 * ============================================================ */

static void demo_mixed_persist(void)
{
    printf("[mix] ===== 混合持久化 =====\n\n");

    printf("[mix] 原理:\n");
    printf("[mix]   - RDB格式前缀 + AOF格式增量\n");
    printf("[mex]   - 兼顾恢复速度和增量备份\n\n");

    printf("[mix] 文件结构:\n");
    printf("[mix]   +---- RDB ----+----- AOF ----+\n");
    printf("[mix]   | REDISxxxx   | *3\r\nSET...   |\n");
    printf("[mix]   | (二进制)    | (文本格式)    |\n\n");

    printf("[mix] 启用方式:\n");
    printf("[mix]   aof-use-rdb-preamble yes\n\n");

    printf("[mix] 恢复流程:\n");
    printf("[mix]   1. 检测到 RDB 前缀\n");
    printf("[mix]   2. 加载 RDB 部分\n");
    printf("[mix]   3. 重放后续 AOF 命令\n\n");

    printf("[mix] 对比:\n");
    printf("[mix]   纯 RDB: 恢复快，但丢失较多\n");
    printf("[mix]   纯 AOF: 恢复慢，但丢失少\n");
    printf("[mix]   混合:   恢复快 + 增量备份\n\n");
}

/* ============================================================
 * [recovery] 恢复流程
 * ============================================================ */

static void demo_recovery(void)
{
    printf("[recovery] ===== 恢复流程 =====\n\n");

    printf("[recovery] 启动时加载:\n");
    printf("[recovery]   if (AOF enabled && exist) {\n");
    printf("[recovery]       loadAppendOnlyFile()\n");
    printf("[recovery]   } else if (RDB exist) {\n");
    printf("[recovery]       rdbLoad()\n");
    printf("[recovery]   }\n\n");

    printf("[recovery] AOF 加载:\n");
    printf("[recovery]   - 逐条解析 RESP 命令\n");
    printf("[recovery]   - 伪客户端执行（fake client）\n");
    printf("[recovery]   - 比 RDB 慢但完整\n\n");

    printf("[recovery] RDB 加载:\n");
    printf("[recovery]   - 二进制解析，快速\n");
    printf("[recovery]   - 无命令细节\n\n");

    printf("[recovery] 故障恢复场景:\n");
    printf("[recovery]   - 突然断电: AOF everysec 最多丢 1s\n");
    printf("[recovery]   - 计划停机: shutdown 自动 bgsave\n");
    printf("[recovery]   - 数据损坏: AOF rewrite 重建\n\n");
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("   Redis 持久化\n");
    printf("   RDB / AOF / 混合持久化\n");
    printf("========================================\n\n");

    demo_rdb_save();
    demo_aof_write();
    demo_aof_rewrite();
    demo_mixed_persist();
    demo_recovery();

    printf("========================================\n");
    printf("   演示结束\n");
    printf("========================================\n");
    return 0;
}
