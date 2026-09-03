/**
 * @file main.c
 * @brief Redis 主从复制演示
 *
 * 演示 PSYNC 同步机制、Replication Buffer、故障切换。
 *
 * @see engineering/src/redis/replica.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * 复制相关类型
 * ============================================================ */

/** 复制角色 */
typedef enum {
    ROLE_MASTER = 0,
    ROLE_SLAVE  = 1
} ReplicationRole;

/** 复制状态 */
typedef enum {
    REPL_STATE_NONE         = 0,
    REPL_STATE_CONNECTING   = 1,
    REPL_STATE_HANDSHAKE    = 2,
    REPL_STATE_CONNECTED    = 3,
    REPL_STATE_TRANSFER     = 4,  /* 传输 RDB */
    REPL_STATE_ONLINE       = 5   /* 在线同步 */
} ReplicationState;

/** 复制偏移量信息 */
typedef struct {
    uint64_t master_repl_offset;  /* 主节点偏移量 */
    uint64_t second_repl_offset;   /* 复制积压缓冲区起始 */
    int      replid_len;          /* replid 长度 */
    char     replid[64];          /* 主节点复制ID */
} ReplicationInfo;

/** 主从连接信息 */
typedef struct {
    ReplicationRole role;
    ReplicationState state;
    uint64_t master_offset;       /* 已同步偏移量 */
    uint64_t master_psync_offset;  /* PSYNC 起始偏移 */
    int      master_link_status;   /* 0=断,1=正常 */
} ReplicationLink;

/* ============================================================
 * [psync] PSYNC 部分同步
 * ============================================================ */

static void demo_psync_full_sync(void)
{
    printf("[psync] ===== PSYNC 全量同步 =====\n\n");

    printf("[psync] 首次连接流程:\n");
    printf("[psync]   1. SLAVE -> MASTER: PING\n");
    printf("[psync]   2. MASTER -> SLAVE: +PONG\n");
    printf("[psync]   3. SLAVE -> MASTER: REPLCONF listening-port <port>\n");
    printf("[psync]   4. MASTER -> SLAVE: +OK\n");
    printf("[psync]   5. SLAVE -> MASTER: REPLCONF capa eof capa psync2\n");
    printf("[psync]   6. MASTER -> SLAVE: +OK\n");
    printf("[psync]   7. SLAVE -> MASTER: PSYNC ? -1\n");
    printf("[psync]   8. MASTER -> SLAVE: +FULLRESYNC <replid> 0\n");
    printf("[psync]   9. MASTER -> SLAVE: <RDB dump>\n");
    printf("[psync]   10. MASTER -> SLAVE: <commands...>\n\n");

    printf("[psync] FULLRESYNC:\n");
    printf("[psync]   - ? 表示新主节点\n");
    printf("[psync]   - -1 表示全量同步\n");
    printf("[psync]   - 返回 replid + 偏移量\n\n");
}

static void demo_psync_partial_sync(void)
{
    printf("[psync] ===== PSYNC 增量同步 =====\n\n");

    printf("[psync] 断线重连流程:\n");
    printf("[psync]   SLAVE -> MASTER: PSYNC <replid> <offset>\n\n");

    printf("[psync] 情况 1: 增量同步成功\n");
    printf("[psync]   MASTER -> SLAVE: +CONTINUE <new_replid>\n");
    printf("[psync]   MASTER -> SLAVE: <commands from offset...>\n\n");

    printf("[psync] 情况 2: 需要全量同步\n");
    printf("[psync]   MASTER -> SLAVE: +FULLRESYNC <replid> <offset>\n");
    printf("[psync]   MASTER -> SLAVE: <RDB dump>\n\n");

    printf("[psync] 复制积压缓冲区:\n");
    printf("[psync]   - server.repl_backlog (环形缓冲区)\n");
    printf("[psync]   - 默认 1MB\n");
    printf("[psync]   - 保存最近的写命令\n");
    printf("[psync]   - 如果断开太久 -> 全量同步\n\n");

    printf("[psync] 同步偏移量:\n");
    printf("[psync]   master_repl_offset: 主节点写入位置\n");
    printf("[psync]   repl_offset: 从节点已接收位置\n");
    printf("[psync]   差距 = 复制积压缓冲区中的待同步数据\n\n");
}

/* ============================================================
 * [buffer] Replication Buffer
 * ============================================================ */

static void demo_replication_buffer(void)
{
    printf("[buffer] ===== Replication Buffer =====\n\n");

    printf("[buffer] 问题:\n");
    printf("[buffer]   - 主节点并发处理多个从节点\n");
    printf("[buffer]   - 每个从节点需要独立缓冲\n");
    printf("[buffer]   - 缓冲区满会导致从节点延迟\n\n");

    printf("[buffer] 缓冲区管理:\n");
    printf("[buffer]   - client-output-buffer-limit slave 256mb 64mb 60\n");
    printf("[buffer]   - 硬限制 256MB\n");
    printf("[buffer]   - 软限制 64MB + 60秒\n");
    printf("[buffer]   - 超限断开连接\n\n");

    printf("[buffer] 缓冲区内容:\n");
    printf("[buffer]   - 发送给从节点的所有命令\n");
    printf("[buffer]   - 包括: 主节点执行的写命令\n");
    printf("[buffer]   - 包括: 脚本 MULTI/EXEC 批量命令\n\n");

    printf("[buffer] 内存占用:\n");
    printf("[buffer]   - 每个从节点 ~1-256MB\n");
    printf("[buffer]   - 100 从节点 -> 最多 ~25GB\n");
    printf("[buffer]   - 需要评估内存容量\n\n");
}

/* ============================================================
 * [failover] 故障切换
 * ============================================================ */

static void demo_failover(void)
{
    printf("[failover] ===== 故障切换（Sentinel）=====\n\n");

    printf("[failover] 哨兵职责:\n");
    printf("[failover]   - 监控: PING 心跳检测节点存活\n");
    printf("[failover]   - 通知: 故障时通知应用\n");
    printf("[failover]   - 自动切换: 主节点 down 时选新主\n\n");

    printf("[failover] 主观下线和客观下线:\n");
    printf("[failover]   - 主观下线: 哨兵认为主节点 down\n");
    printf("[failover]   - 客观下线: 多数哨兵认为 down\n");
    printf("[failover]   - quorum 配置决定客观下线\n\n");

    printf("[failover] 选主流程:\n");
    printf("[failover]   1. 发现主节点客观下线\n");
    printf("[failover]   2. 哨兵间协商，选出 leader\n");
    printf("[failover]   3. leader 向从节点发送 SLAVEOF NO ONE\n");
    printf("[failover]   4. 新主节点上线\n");
    printf("[failover]   5. 通知其他从节点指向新主\n");
    printf("[failover]   6. 通知应用\n\n");

    printf("[failover] Sentinel 配置:\n");
    printf("[failover]   sentinel monitor mymaster 127.0.0.1 6379 2\n");
    printf("[failover]   sentinel down-after-milliseconds mymaster 5000\n");
    printf("[failover]   sentinel parallel-syncs mymaster 1\n\n");
}

/* ============================================================
 * [topo] 复制拓扑
 * ============================================================ */

static void demo_topology(void)
{
    printf("[topo] ===== 复制拓扑 =====\n\n");

    printf("[topo] 一主一从:\n");
    printf("[topo]   [Master] ---> [Slave]\n\n");

    printf("[topo] 一主多从:\n");
    printf("[topo]        ---> [Slave1]\n");
    printf("[topo]   [Master] ---> [Slave2]\n");
    printf("[topo]        ---> [Slave3]\n");
    printf("[topo]   读写分离场景\n\n");

    printf("[topo] 链式复制（不支持）:\n");
    printf("[topo]   [Master] ---> [Slave1] ---> [Slave2]\n");
    printf("[topo]   Redis 不支持，会导致延迟累积\n\n");

    printf("[topo] 树形拓扑:\n");
    printf("[topo]          ---> [Slave1]\n");
    printf("[topo]   [M] ---> [Mid1] ---> [Slave2]\n");
    printf("[topo]          ---> [Mid2] ---> [Slave3]\n");
    printf("[topo]   虚拟 IP 或代理层转发\n\n");

    printf("[topo] 应用场景:\n");
    printf("[topo]   - 读写分离: 从节点处理读\n");
    printf("[topo]   - 故障恢复: 从节点切换为主\n");
    printf("[topo]   - 数据备份: 从节点定期备份\n\n");
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("   Redis 主从复制\n");
    printf("   PSYNC / Replication Buffer / 故障切换\n");
    printf("========================================\n\n");

    demo_psync_full_sync();
    demo_psync_partial_sync();
    demo_replication_buffer();
    demo_failover();
    demo_topology();

    printf("========================================\n");
    printf("   演示结束\n");
    printf("========================================\n");
    return 0;
}
