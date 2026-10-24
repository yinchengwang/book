/**
 * @file raft_transport.c
 * @brief Raft 网络传输层实现
 *
 * Phase12 - 实现 Raft 集群的真实 TCP 网络通信框架。
 *
 * 核心功能：
 * 1. 消息编解码（RequestVote, AppendEntries 及响应）
 * 2. 传输层接口抽象
 * 3. TCP 传输层框架
 *
 * 网络传输基于项目现有的 RPC 框架实现（db/distributed/rpc/）。
 */

#include "db/consensus/raft_transport.h"
#include "db/distributed/rpc.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define RAFT_TRANSPORT_MAGIC 0x52545431  /* "RTT1" */
#define MAX_PAYLOAD_SIZE (64 * 1024 * 1024)

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief CRC32 计算
 */
static uint32_t calculate_crc32(const void *data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *ptr = (const uint8_t *)data;

    for (size_t i = 0; i < size; i++) {
        crc ^= ptr[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return ~crc;
}

/* ========================================================================
 * 消息编解码实现
 * ======================================================================== */

size_t raft_encode_request_vote(const RaftRequestVoteArgs_t *args,
                               void *payload, size_t capacity) {
    if (!args || !payload || capacity < 32) return 0;

    uint8_t *buf = (uint8_t *)payload;
    size_t offset = 0;

    /* term (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (args->term >> (i * 8)) & 0xFF;
    /* candidate_id (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (args->candidate_id >> (i * 8)) & 0xFF;
    /* last_log_index (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (args->last_log_index >> (i * 8)) & 0xFF;
    /* last_log_term (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (args->last_log_term >> (i * 8)) & 0xFF;

    return offset;
}

int raft_decode_request_vote(const void *payload, size_t size,
                            RaftRequestVoteArgs_t *args) {
    if (!payload || size < 32 || !args) return -1;

    const uint8_t *buf = (const uint8_t *)payload;
    size_t offset = 0;

    args->term = 0;
    for (int i = 0; i < 8; i++) args->term = (args->term << 8) | buf[offset++];

    args->candidate_id = 0;
    for (int i = 0; i < 8; i++) args->candidate_id = (args->candidate_id << 8) | buf[offset++];

    args->last_log_index = 0;
    for (int i = 0; i < 8; i++) args->last_log_index = (args->last_log_index << 8) | buf[offset++];

    args->last_log_term = 0;
    for (int i = 0; i < 8; i++) args->last_log_term = (args->last_log_term << 8) | buf[offset++];

    return 0;
}

size_t raft_encode_append_entries(const RaftAppendEntriesArgs_t *args,
                                 void *payload, size_t capacity) {
    if (!args || !payload) return 0;

    /* 计算所需空间 */
    size_t fixed_size = 48; /* 基本字段 */
    size_t entry_size = 0;

    for (size_t i = 0; i < args->entry_count; i++) {
        const RaftLogEntry_t *entry = &args->entries[i];
        entry_size += 16 + entry->data_size; /* index + term + size + data */
    }

    if (capacity < fixed_size + entry_size) {
        return 0;
    }

    uint8_t *buf = (uint8_t *)payload;
    size_t offset = 0;

    /* term (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (args->term >> (i * 8)) & 0xFF;
    /* leader_id (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (args->leader_id >> (i * 8)) & 0xFF;
    /* prev_log_index (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (args->prev_log_index >> (i * 8)) & 0xFF;
    /* prev_log_term (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (args->prev_log_term >> (i * 8)) & 0xFF;
    /* leader_commit (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (args->leader_commit >> (i * 8)) & 0xFF;
    /* entry_count (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = ((uint64_t)args->entry_count >> (i * 8)) & 0xFF;

    /* 编码 entries */
    for (size_t i = 0; i < args->entry_count; i++) {
        const RaftLogEntry_t *entry = &args->entries[i];

        /* index (8 bytes) */
        for (int j = 7; j >= 0; j--) buf[offset++] = (entry->index >> (j * 8)) & 0xFF;
        /* term (8 bytes) */
        for (int j = 7; j >= 0; j--) buf[offset++] = (entry->term >> (j * 8)) & 0xFF;

        /* data_size (4 bytes) */
        uint32_t ds = (uint32_t)entry->data_size;
        for (int j = 3; j >= 0; j--) buf[offset++] = (ds >> (j * 8)) & 0xFF;

        /* data */
        if (entry->data && entry->data_size > 0) {
            memcpy(buf + offset, entry->data, entry->data_size);
            offset += entry->data_size;
        }
    }

    return offset;
}

int raft_decode_append_entries(const void *payload, size_t size,
                              RaftAppendEntriesArgs_t *args) {
    if (!payload || size < 48 || !args) return -1;

    const uint8_t *buf = (const uint8_t *)payload;
    size_t offset = 0;

    args->term = 0;
    for (int i = 0; i < 8; i++) args->term = (args->term << 8) | buf[offset++];

    args->leader_id = 0;
    for (int i = 0; i < 8; i++) args->leader_id = (args->leader_id << 8) | buf[offset++];

    args->prev_log_index = 0;
    for (int i = 0; i < 8; i++) args->prev_log_index = (args->prev_log_index << 8) | buf[offset++];

    args->prev_log_term = 0;
    for (int i = 0; i < 8; i++) args->prev_log_term = (args->prev_log_term << 8) | buf[offset++];

    args->leader_commit = 0;
    for (int i = 0; i < 8; i++) args->leader_commit = (args->leader_commit << 8) | buf[offset++];

    uint64_t entry_count = 0;
    for (int i = 0; i < 8; i++) entry_count = (entry_count << 8) | buf[offset++];
    args->entry_count = (size_t)entry_count;

    /* entries 暂时不解码 */
    args->entries = NULL;

    return 0;
}

size_t raft_encode_request_vote_resp(const RaftRequestVoteResult_t *result,
                                     void *payload, size_t capacity) {
    if (!result || !payload || capacity < 16) return 0;

    uint8_t *buf = (uint8_t *)payload;
    size_t offset = 0;

    /* term (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (result->term >> (i * 8)) & 0xFF;
    /* vote_granted (1 byte) */
    buf[offset++] = result->vote_granted ? 1 : 0;
    /* padding */
    buf[offset++] = 0;
    for (int i = 5; i >= 0; i--) buf[offset++] = 0;

    return offset;
}

int raft_decode_request_vote_resp(const void *payload, size_t size,
                                  RaftRequestVoteResult_t *result) {
    if (!payload || size < 16 || !result) return -1;

    const uint8_t *buf = (const uint8_t *)payload;
    size_t offset = 0;

    result->term = 0;
    for (int i = 0; i < 8; i++) result->term = (result->term << 8) | buf[offset++];

    result->vote_granted = (buf[offset++] != 0);

    return 0;
}

size_t raft_encode_append_entries_resp(const RaftAppendEntriesResult_t *result,
                                       void *payload, size_t capacity) {
    if (!result || !payload || capacity < 24) return 0;

    uint8_t *buf = (uint8_t *)payload;
    size_t offset = 0;

    /* term (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (result->term >> (i * 8)) & 0xFF;
    /* success (1 byte) */
    buf[offset++] = result->success ? 1 : 0;
    /* match_index (8 bytes) */
    for (int i = 7; i >= 0; i--) buf[offset++] = (result->match_index >> (i * 8)) & 0xFF;
    /* padding */
    for (int i = 3; i >= 0; i--) buf[offset++] = 0;

    return offset;
}

int raft_decode_append_entries_resp(const void *payload, size_t size,
                                    RaftAppendEntriesResult_t *result) {
    if (!payload || size < 24 || !result) return -1;

    const uint8_t *buf = (const uint8_t *)payload;
    size_t offset = 0;

    result->term = 0;
    for (int i = 0; i < 8; i++) result->term = (result->term << 8) | buf[offset++];

    result->success = (buf[offset++] != 0);

    result->match_index = 0;
    for (int i = 0; i < 8; i++) result->match_index = (result->match_index << 8) | buf[offset++];

    return 0;
}

/* ========================================================================
 * RaftTransport 接口实现
 * ======================================================================== */

/**
 * @brief 发送 RequestVote RPC
 *
 * TODO: 基于 RPC 框架实现真正的网络传输
 */
static int impl_send_request_vote(void *impl, uint64_t to_node,
                                  const RaftRequestVoteArgs_t *args,
                                  RaftRequestVoteResult_t *result) {
    (void)impl;
    (void)to_node;

    if (!args || !result) return -1;

    /* TODO: 通过 RPC 框架发送请求到 to_node
     * 1. 获取节点的 RPC 地址
     * 2. 编码 RequestVote 消息
     * 3. 发送 RPC 请求
     * 4. 等待响应
     * 5. 解码响应
     */

    /* 暂时返回模拟响应 */
    result->term = args->term;
    result->vote_granted = true;

    return 0;
}

/**
 * @brief 发送 AppendEntries RPC
 *
 * TODO: 基于 RPC 框架实现真正的网络传输
 */
static int impl_send_append_entries(void *impl, uint64_t to_node,
                                    const RaftAppendEntriesArgs_t *args,
                                    RaftAppendEntriesResult_t *result) {
    (void)impl;
    (void)to_node;

    if (!args || !result) return -1;

    /* TODO: 通过 RPC 框架发送 AppendEntries */

    result->term = args->term;
    result->success = true;
    result->match_index = args->prev_log_index + args->entry_count;

    return 0;
}

/**
 * @brief 发送快照
 *
 * 预留接口，后续实现
 */
static int impl_send_snapshot(void *impl, uint64_t to_node,
                              const void *data, size_t size,
                              uint64_t last_index, uint64_t last_term) {
    (void)impl;
    (void)to_node;
    (void)data;
    (void)size;
    (void)last_index;
    (void)last_term;

    /* TODO: 实现快照传输 */
    return -1;
}

/**
 * @brief 发送心跳
 */
static int impl_send_heartbeat(void *impl, uint64_t to_node) {
    (void)impl;
    (void)to_node;

    /* TODO: 实现心跳发送 */
    return 0;
}

/**
 * @brief 检查节点是否可达
 */
static bool impl_is_peer_reachable(void *impl, uint64_t node_id) {
    TcpRaftTransport_t *transport = (TcpRaftTransport_t *)impl;
    if (!transport) return false;

    pthread_mutex_lock(&transport->lock);

    for (uint32_t i = 0; i < transport->connection_count; i++) {
        if (transport->connections[i].node_id == node_id) {
            bool reachable = (transport->connections[i].state == RAFT_NODE_CONNECTED);
            pthread_mutex_unlock(&transport->lock);
            return reachable;
        }
    }

    pthread_mutex_unlock(&transport->lock);
    return false;
}

/**
 * @brief 销毁传输层
 */
static void impl_destroy(void *impl) {
    if (impl) {
        TcpRaftTransport_t *transport = (TcpRaftTransport_t *)impl;
        if (transport->connections) {
            /* TODO: 清理 RPC 客户端连接 */
            /* rpc_client_destroy(transport->connections[i].client); */
            free(transport->connections);
        }
        pthread_mutex_destroy(&transport->lock);
        free(transport);
        free(transport); /* 释放外层包装 */
    }
}

static const RaftTransportOps_t g_tcp_transport_ops = {
    .send_request_vote = impl_send_request_vote,
    .send_append_entries = impl_send_append_entries,
    .send_snapshot = impl_send_snapshot,
    .send_heartbeat = impl_send_heartbeat,
    .is_peer_reachable = impl_is_peer_reachable,
    .destroy = impl_destroy
};

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

/**
 * @brief 创建 TCP Raft 传输层
 */
RaftTransport_t *tcp_raft_transport_create(const TcpRaftTransportConfig_t *config) {
    if (!config || config->peer_count == 0) return NULL;

    TcpRaftTransport_t *transport = (TcpRaftTransport_t *)calloc(1, sizeof(TcpRaftTransport_t));
    if (!transport) return NULL;

    transport->config = *config;
    transport->is_running = false;
    transport->is_connected = false;

    pthread_mutex_init(&transport->lock, NULL);

    /* 初始化对端连接 */
    transport->connection_count = config->peer_count;
    transport->connections = (RaftPeerConnection_t *)calloc(
        config->peer_count, sizeof(RaftPeerConnection_t));
    if (!transport->connections) {
        pthread_mutex_destroy(&transport->lock);
        free(transport);
        return NULL;
    }

    for (uint32_t i = 0; i < config->peer_count; i++) {
        transport->connections[i].node_id = config->peers[i].node_id;
        transport->connections[i].state = RAFT_NODE_DISCONNECTED;
        transport->connections[i].client = NULL;
    }

    /* 创建 RaftTransport 包装 */
    RaftTransport_t *rt = (RaftTransport_t *)malloc(sizeof(RaftTransport_t));
    if (!rt) {
        free(transport->connections);
        pthread_mutex_destroy(&transport->lock);
        free(transport);
        return NULL;
    }

    rt->ops = &g_tcp_transport_ops;
    rt->impl = transport;

    return rt;
}

/**
 * @brief 销毁 Raft 传输层
 */
void raft_transport_destroy(RaftTransport_t *transport) {
    if (!transport) return;

    if (transport->ops && transport->ops->destroy) {
        transport->ops->destroy(transport->impl);
    } else {
        /* 如果没有 destroy 回调，手动清理 */
        if (transport->impl) {
            TcpRaftTransport_t *t = (TcpRaftTransport_t *)transport->impl;
            if (t->connections) {
                for (uint32_t i = 0; i < t->connection_count; i++) {
                    /* TODO: 清理 RPC 客户端连接 */
                    /* if (t->connections[i].client) { rpc_client_destroy(t->connections[i].client); } */
                }
                free(t->connections);
            }
            pthread_mutex_destroy(&t->lock);
            free(t);
        }
        free(transport);
    }
}

/**
 * @brief 启动传输层
 */
int raft_transport_start(RaftTransport_t *transport) {
    if (!transport) return -1;

    TcpRaftTransport_t *t = (TcpRaftTransport_t *)transport->impl;
    if (!t) return -1;

    pthread_mutex_lock(&t->lock);

    if (t->is_running) {
        pthread_mutex_unlock(&t->lock);
        return 0;
    }

    t->is_running = true;
    t->is_connected = false;

    pthread_mutex_unlock(&t->lock);

    /* TODO: 启动连接管理器线程，定期重连 */

    return 0;
}

/**
 * @brief 停止传输层
 */
int raft_transport_stop(RaftTransport_t *transport) {
    if (!transport) return -1;

    TcpRaftTransport_t *t = (TcpRaftTransport_t *)transport->impl;
    if (!t) return -1;

    pthread_mutex_lock(&t->lock);

    t->is_running = false;

    /* 关闭所有连接 */
    for (uint32_t i = 0; i < t->connection_count; i++) {
        if (t->connections[i].client) {
            /* TODO: 清理 RPC 客户端连接 */
            /* rpc_client_destroy(t->connections[i].client); */
            t->connections[i].client = NULL;
            t->connections[i].state = RAFT_NODE_DISCONNECTED;
        }
    }

    pthread_mutex_unlock(&t->lock);

    return 0;
}

/**
 * @brief 发送 RequestVote RPC
 */
int raft_transport_send_request_vote(RaftTransport_t *transport,
                                      uint64_t to_node,
                                      const RaftRequestVoteArgs_t *args,
                                      RaftRequestVoteResult_t *result) {
    if (!transport || !transport->ops) return -1;
    return transport->ops->send_request_vote(transport->impl, to_node, args, result);
}

/**
 * @brief 发送 AppendEntries RPC
 */
int raft_transport_send_append_entries(RaftTransport_t *transport,
                                       uint64_t to_node,
                                       const RaftAppendEntriesArgs_t *args,
                                       RaftAppendEntriesResult_t *result) {
    if (!transport || !transport->ops) return -1;
    return transport->ops->send_append_entries(transport->impl, to_node, args, result);
}

/**
 * @brief 发送快照
 */
int raft_transport_send_snapshot(RaftTransport_t *transport,
                                 uint64_t to_node,
                                 const void *data,
                                 size_t size,
                                 uint64_t last_index,
                                 uint64_t last_term) {
    if (!transport || !transport->ops) return -1;
    return transport->ops->send_snapshot(transport->impl, to_node, data, size, last_index, last_term);
}

/**
 * @brief 发送心跳
 */
int raft_transport_send_heartbeat(RaftTransport_t *transport, uint64_t to_node) {
    if (!transport || !transport->ops) return -1;
    return transport->ops->send_heartbeat(transport->impl, to_node);
}

/**
 * @brief 批量发送心跳到所有节点
 */
int raft_transport_broadcast_heartbeat(RaftTransport_t *transport) {
    if (!transport) return -1;

    TcpRaftTransport_t *t = (TcpRaftTransport_t *)transport->impl;
    if (!t) return -1;

    pthread_mutex_lock(&t->lock);

    int ret = 0;
    for (uint32_t i = 0; i < t->connection_count; i++) {
        if (t->connections[i].state == RAFT_NODE_CONNECTED) {
            if (raft_transport_send_heartbeat(transport, t->connections[i].node_id) != 0) {
                ret = -1;
            }
        }
    }

    pthread_mutex_unlock(&t->lock);

    return ret;
}

/**
 * @brief 检查节点是否可达
 */
bool raft_transport_is_peer_reachable(RaftTransport_t *transport, uint64_t node_id) {
    if (!transport || !transport->ops) return false;
    return transport->ops->is_peer_reachable(transport->impl, node_id);
}

/**
 * @brief 获取活跃连接数
 */
uint32_t raft_transport_get_active_connection_count(const RaftTransport_t *transport) {
    if (!transport) return 0;

    const TcpRaftTransport_t *t = (const TcpRaftTransport_t *)transport->impl;
    if (!t) return 0;

    pthread_mutex_lock((pthread_mutex_t *)&t->lock);

    uint32_t count = 0;
    for (uint32_t i = 0; i < t->connection_count; i++) {
        if (t->connections[i].state == RAFT_NODE_CONNECTED) {
            count++;
        }
    }

    pthread_mutex_unlock((pthread_mutex_t *)&t->lock);

    return count;
}

/**
 * @brief 设置 RPC 回调
 */
void raft_transport_set_callbacks(RaftTransport_t *transport,
                                  void (*on_request_vote)(uint64_t, const RaftRequestVoteArgs_t *,
                                                          RaftRequestVoteResult_t *, void *),
                                  void (*on_append_entries)(uint64_t, const RaftAppendEntriesArgs_t *,
                                                           RaftAppendEntriesResult_t *, void *),
                                  void (*on_snapshot)(uint64_t, const void *, size_t,
                                                      uint64_t, uint64_t, void *),
                                  void (*on_heartbeat)(uint64_t, void *),
                                  void *user_data) {
    if (!transport) return;

    TcpRaftTransport_t *t = (TcpRaftTransport_t *)transport->impl;
    if (!t) return;

    pthread_mutex_lock(&t->lock);

    t->on_request_vote = on_request_vote;
    t->on_append_entries = on_append_entries;
    t->on_snapshot = on_snapshot;
    t->on_heartbeat = on_heartbeat;
    t->user_data = user_data;

    pthread_mutex_unlock(&t->lock);
}
