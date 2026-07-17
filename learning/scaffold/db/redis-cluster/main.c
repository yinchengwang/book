/**
 * @file main.c
 * @brief Redis 集群架构演示
 *
 * 演示 Redis Cluster 的 Hash Slot 分配、MOVED/ASK 重定向、
 * 故障检测（FAIL 协议）、Gossip 节点发现。
 *
 * @see engineering/src/redis/cluster.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * 集群相关类型
 * ============================================================ */

/** 集群节点状态 */
typedef enum {
    NODE_STATE_DISCONNECTED = 0,
    NODE_STATE_HANDSHAKE    = 1,
    NODE_STATE_CONNECTED    = 2
} NodeState;

/** 节点标志 */
typedef enum {
    NODE_FLAG_MASTER    = 1 << 0,
    NODE_FLAG_SLAVE     = 1 << 1,
    NODE_FLAG_PFAIL     = 1 << 2,   /* 可能下线 */
    NODE_FLAG_FAIL      = 1 << 3,   /* 已下线 */
    NODE_FLAG_MYSELF    = 1 << 4    /* 本节点 */
} NodeFlags;

/** 集群节点 */
typedef struct ClusterNode {
    char      name[64];      /* 节点名称 */
    char      ip[32];       /* IP 地址 */
    int       port;          /* 端口 */
    int       slaveof;       /* 主节点 */
    NodeFlags flags;         /* 节点标志 */
    int       slots[16384];  /* 槽分配 */
    int       slot_count;    /* 槽数量 */
    NodeState state;         /* 连接状态 */
} ClusterNode;

/* ============================================================
 * [slot] Hash Slot 分配
 * ============================================================ */

/* Redis Cluster 不使用 CRC64，使用简单哈希 */

/* 简化版 CRC */
static uint16_t simple_hash(const char *str, int len)
{
    uint32_t hash = 5381;
    for (int i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + str[i];
    }
    return hash % 16384;
}

static void demo_hash_slot(void)
{
    printf("[slot] ===== Hash Slot 分配 =====\n\n");

    printf("[slot] 槽数量: 16384 (2^14)\n\n");

    printf("[slot] 槽分配规则:\n");
    printf("[slot]   slot = CRC16(key) % 16384\n\n");

    printf("[slot] key 格式:\n");
    printf("[slot]   普通 key: key -> CRC16(key) % 16384\n");
    printf("[slot]   hash tag: {user:1}:name -> CRC16(user:1) % 16384\n\n");

    /* 演示 */
    printf("[slot] 示例:\n");
    const char *keys[] = {"name", "age", "user:1:posts", "user:2:posts"};
    for (int i = 0; i < 4; i++) {
        uint16_t slot = simple_hash(keys[i], strlen(keys[i]));
        printf("[slot]   key=\"%s\" -> slot=%d\n", keys[i], slot);
    }
    printf("\n");

    printf("[slot] hash tag 示例:\n");
    printf("[slot]   {user:1}:name -> slot 假设 = 500\n");
    printf("[slot]   {user:1}:age  -> slot = 500 (同一槽)\n");
    printf("[slot]   user:2:name   -> slot = 1500 (不同槽)\n");
    printf("[slot]   用途: 保证多 key 在同一槽，支持批量操作\n\n");

    printf("[slot] 槽迁移:\n");
    printf("[slot]   CLUSTER SETSLOT <slot> MIGRATING <node>\n");
    printf("[slot]   CLUSTER SETSLOT <slot> IMPORTING <node>\n");
    printf("[slot]   MOVED 重定向用户请求\n\n");
}

/* ============================================================
 * [redirect] MOVED 和 ASK 重定向
 * ============================================================ */

static void demo_moved_redirect(void)
{
    printf("[redirect] ===== MOVED 重定向 =====\n\n");

    printf("[redirect] MOVED 响应:\n");
    printf("[redirect]   MOVED <slot> <ip:port>\n");
    printf("[redirect]   表示: 该槽已迁移到其他节点\n\n");

    printf("[redirect] 客户端行为:\n");
    printf("[redirect]   1. 发送 GET key 命令\n");
    printf("[redirect]   2. 收到 MOVED 500 127.0.0.1:6380\n");
    printf("[redirect]   3. 更新本地槽映射\n");
    printf("[redirect]   4. 重发请求到 6380\n");
    printf("[redirect]   5. 收到正常响应\n\n");

    printf("[redirect] 智能客户端 vs 普通客户端:\n");
    printf("[redirect]   智能客户端:\n");
    printf("[redirect]     - 缓存槽映射\n");
    printf("[redirect]     - MOVED 时更新本地映射\n");
    printf("[redirect]     - 大多数请求直接发到正确节点\n");
    printf("[redirect]   普通客户端:\n");
    printf("[redirect]     - 每次 MOVED 都重定向\n");
    printf("[redirect]     - 性能略差\n\n");
}

static void demo_ask_redirect(void)
{
    printf("[redirect] ASK 重定向 =====\n\n");

    printf("[redirect] ASK vs MOVED:\n");
    printf("[redirect]   MOVED: 槽永久迁移，客户端更新映射\n");
    printf("[redirect]   ASK:   槽迁移中，客户端先尝试旧节点\n\n");

    printf("[redirect] ASK 响应:\n");
    printf("[redirect]   ASK <slot> <ip:port>\n\n");

    printf("[redirect] ASK 处理流程:\n");
    printf("[redirect]   1. 请求 key，槽正在迁移\n");
    printf("[redirect]   2. 旧节点无 key，回复 ASK <slot> <new>\n");
    printf("[redirect]   3. 客户端发送 ASKING 命令\n");
    printf("[redirect]   4. 客户端发送原请求到新节点\n");
    printf("[redirect]   5. 新节点处理（即使槽 IMPORTING）\n\n");

    printf("[redirect] 为什么需要 ASKING:\n");
    printf("[redirect]   - 槽 IMPORTING 状态拒绝普通请求\n");
    printf("[redirect]   - ASKING 打开客户端的 ASK 标志\n");
    printf("[redirect]   - 允许本次请求穿透 IMPORTING 限制\n\n");
}

/* ============================================================
 * [failover] 故障检测与切换
 * ============================================================ */

static void demo_failover(void)
{
    printf("[failover] ===== 故障检测与切换 =====\n\n");

    printf("[failover] 节点标记流程:\n");
    printf("[failover]   1. PFAIL: 节点 A 认为节点 B 疑似下线\n");
    printf("[failover]   2. FAIL: 多数节点认为 B 已下线\n");
    printf("[failover]   3. 从节点收到 FAIL 通知，尝试切换\n\n");

    printf("[failover] Gossip 协议:\n");
    printf("[failover]   - 每个节点定期随机向其他节点发送 Gossip\n");
    printf("[failover]   - 包含自己的状态 + 其他节点信息\n");
    printf("[failover]   - 逐渐传播，达到最终一致\n\n");

    printf("[failover] FAIL 消息:\n");
    printf("[failover]   - 节点发现另一个节点不可达\n");
    printf("[failover]   - 广播 FAIL 消息给所有可达节点\n");
    printf("[failover]   - 确保所有节点快速达成共识\n\n");

    printf("[failover] 从节点选举:\n");
    printf("[failover]   1. 从节点发现主节点 FAIL\n");
    printf("[failover]   2. 等待一段随机延迟 (100-500ms)\n");
    printf("[failover]   3. 向主节点发 CLUSTERMSG_TYPE_FAILOVER_AUTH_REQUEST\n");
    printf("[failover]   4. 获得多数主节点投票\n");
    printf("[failover]   5. 发起选举成为新主节点\n\n");
}

/* ============================================================
 * [topology] 集群拓扑
 * ============================================================ */

static void demo_topology(void)
{
    printf("[topo] ===== 集群拓扑 =====\n\n");

    printf("[topo] 典型 3 主 3 从:\n");
    printf("[topo]   [M1]---[S1]  [M2]---[S2]  [M3]---[S3]\n");
    printf("[topo]     |          |          |\n");
    printf("[topo]     +---- Bus ----+\n\n");

    printf("[topo] 槽分配示例 (16384 槽):\n");
    printf("[topo]   Node1: slot 0-5460    (0-5460)\n");
    printf("[topo]   Node2: slot 5461-10922 (5461-10922)\n");
    printf("[topo]   Node3: slot 10923-16383 (10923-16383)\n\n");

    printf("[topo] 节点发现:\n");
    printf("[topo]   - 启动时连接 seed 节点\n");
    printf("[topo]   - 通过 MEET 消息加入集群\n");
    printf("[topo]   - Gossip 传播节点信息\n\n");

    printf("[topo] 客户端连接:\n");
    printf("[topo]   CLUSTER NODES 获取集群拓扑\n");
    printf("[topo]   客户端计算槽，直接连接对应节点\n");
    printf("[topo]   支持重定向 (MOVED/ASK)\n\n");
}

/* ============================================================
 * [key] 集群下 key 操作限制
 * ============================================================ */

static void demo_key_operations(void)
{
    printf("[key] ===== 集群下 key 操作 =====\n\n");

    printf("[key] 支持的操作:\n");
    printf("[key]   - 单 key 操作 (GET/SET/DEL 等)\n");
    printf("[key]   - hash tag 批量操作 (MGET/MSET)\n\n");

    printf("[key] 不支持的操作:\n");
    printf("[key]   - 跨槽批量操作: MGET key1 key2 (不同槽)\n");
    printf("[key]   - 跨槽事务: MULTI + 不同槽命令\n");
    printf("[key]   - keys/scan (可能遗漏或重复)\n");
    printf("[key]   - Lua 脚本访问多个槽\n\n");

    printf("[key] 跨槽操作的替代:\n");
    printf("[key]   - 使用 hash tag 保证同一槽\n");
    printf("[key]   - {user:1}:name, {user:1}:age -> 同一槽\n");
    printf("[key]   - Pipeline 发送到多个节点\n\n");

    printf("[key] Hash tag 规则:\n");
    printf("[key]   key = \"{tag}rest\"\n");
    printf("[key]   使用 {} 内的内容计算槽\n");
    printf("[key]   无 {} 或 {} 为空: 使用完整 key\n\n");
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("   Redis 集群架构\n");
    printf("   Hash Slot / MOVED / ASK / Gossip\n");
    printf("========================================\n\n");

    demo_hash_slot();
    demo_moved_redirect();
    demo_ask_redirect();
    demo_failover();
    demo_topology();
    demo_key_operations();

    printf("========================================\n");
    printf("   演示结束\n");
    printf("========================================\n");
    return 0;
}
