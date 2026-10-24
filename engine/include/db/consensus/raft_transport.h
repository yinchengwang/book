/**
 * @file raft_transport.h
 * @brief Raft 网络传输层接口
 *
 * Phase12 - 实现 Raft 集群的真实 TCP 网络通信。
 *
 * 设计目标：
 * - 基于现有 RPC 框架实现可靠传输
 * - 支持 RequestVote 和 AppendEntries RPC
 * - 支持快照传输
 * - 自动重连和心跳
 *
 * 架构：
 * - RaftTransport_t: 传输层抽象接口
 * - TcpRaftTransport: 基于 TCP 的传输实现
 * - 集成到 raft_cluster 用于多节点通信
 */
#ifndef DB_CONSENSUS_RAFT_TRANSPORT_H
#define DB_CONSENSUS_RAFT_TRANSPORT_H

#include "db/consensus/raft.h"
#include "db/distributed/rpc.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Raft RPC 消息类型扩展
 * ======================================================================== */

/** Raft 网络消息类型 */
typedef enum RaftTransportMsgType {
    RAFT_TRANSPORT_REQUEST_VOTE = 100,   /**< RequestVote RPC */
    RAFT_TRANSPORT_REQUEST_VOTE_RESP,     /**< RequestVote 响应 */
    RAFT_TRANSPORT_APPEND_ENTRIES,       /**< AppendEntries RPC */
    RAFT_TRANSPORT_APPEND_ENTRIES_RESP,  /**< AppendEntries 响应 */
    RAFT_TRANSPORT_SNAPSHOT,             /**< 快照传输 */
    RAFT_TRANSPORT_SNAPSHOT_RESP,        /**< 快照传输响应 */
    RAFT_TRANSPORT_HEARTBEAT,           /**< 心跳消息 */
    RAFT_TRANSPORT_HEARTBEAT_RESP,       /**< 心跳响应 */
} RaftTransportMsgType_t;

/* ========================================================================
 * 节点地址和连接管理
 * ======================================================================== */

/** Raft 节点地址 */
typedef struct RaftNodeAddr {
    uint64_t node_id;            /**< 节点 ID */
    char host[256];             /**< 主机名或 IP */
    uint16_t port;              /**< 端口号 */
} RaftNodeAddr_t;

/** 节点连接状态 */
typedef enum RaftNodeState {
    RAFT_NODE_DISCONNECTED = 0,  /**< 未连接 */
    RAFT_NODE_CONNECTING,         /**< 连接中 */
    RAFT_NODE_CONNECTED,          /**< 已连接 */
    RAFT_NODE_FAILED              /**< 连接失败 */
} RaftNodeState_t;

/** 节点连接信息 */
typedef struct RaftPeerConnection {
    uint64_t node_id;                    /**< 对端节点 ID */
    rpc_client_t *client;               /**< RPC 客户端 */
    RaftNodeState_t state;               /**< 连接状态 */
    uint64_t last_heartbeat;            /**< 最后心跳时间 */
    uint64_t next_retry;                /**< 下次重连时间 */
    int retry_count;                    /**< 重试次数 */
} RaftPeerConnection_t;

/* ========================================================================
 * 传输层接口
 * ======================================================================== */

/**
 * @brief Raft 传输层接口
 */
typedef struct RaftTransportOps {
    /** 发送 RequestVote RPC */
    int (*send_request_vote)(void *impl,
                             uint64_t to_node,
                             const RaftRequestVoteArgs_t *args,
                             RaftRequestVoteResult_t *result);

    /** 发送 AppendEntries RPC */
    int (*send_append_entries)(void *impl,
                               uint64_t to_node,
                               const RaftAppendEntriesArgs_t *args,
                               RaftAppendEntriesResult_t *result);

    /** 发送快照 */
    int (*send_snapshot)(void *impl,
                        uint64_t to_node,
                        const void *data,
                        size_t size,
                        uint64_t last_index,
                        uint64_t last_term);

    /** 发送心跳 */
    int (*send_heartbeat)(void *impl, uint64_t to_node);

    /** 检查节点是否可达 */
    bool (*is_peer_reachable)(void *impl, uint64_t node_id);

    /** 销毁传输层 */
    void (*destroy)(void *impl);
} RaftTransportOps_t;

/**
 * @brief Raft 传输层不透明类型
 */
typedef struct RaftTransport {
    const RaftTransportOps_t *ops;
    void *impl;
} RaftTransport_t;

/* ========================================================================
 * TCP 传输层实现
 * ======================================================================== */

/**
 * @brief TCP 传输层配置
 */
typedef struct TcpRaftTransportConfig {
    uint64_t local_node_id;              /**< 本地节点 ID */
    uint16_t listen_port;                /**< 本地监听端口 */
    RaftNodeAddr_t *peers;             /**< 对端节点地址数组 */
    uint32_t peer_count;                /**< 对端节点数量 */
    uint32_t connect_timeout_ms;        /**< 连接超时 */
    uint32_t request_timeout_ms;        /**< 请求超时 */
    uint32_t heartbeat_interval_ms;      /**< 心跳间隔 */
    uint32_t max_retry;                 /**< 最大重试次数 */
} TcpRaftTransportConfig_t;

/**
 * @brief TCP 传输层
 */
typedef struct TcpRaftTransport {
    TcpRaftTransportConfig_t config;    /**< 配置 */

    /* 本地服务 */
    rpc_server_t *server;               /**< RPC 服务端 */
    pthread_t server_thread;             /**< 服务端线程 */

    /* 对端连接 */
    RaftPeerConnection_t *connections;   /**< 对端连接数组 */
    uint32_t connection_count;           /**< 连接数 */

    /* 状态 */
    pthread_mutex_t lock;                /**< 保护所有状态 */
    bool is_running;                    /**< 是否运行中 */
    bool is_connected;                  /**< 是否有活跃连接 */

    /* 回调 */
    void (*on_request_vote)(uint64_t from_node,
                           const RaftRequestVoteArgs_t *args,
                           RaftRequestVoteResult_t *result,
                           void *user_data);
    void (*on_append_entries)(uint64_t from_node,
                              const RaftAppendEntriesArgs_t *args,
                              RaftAppendEntriesResult_t *result,
                              void *user_data);
    void (*on_snapshot)(uint64_t from_node,
                        const void *data,
                        size_t size,
                        uint64_t last_index,
                        uint64_t last_term,
                        void *user_data);
    void (*on_heartbeat)(uint64_t from_node, void *user_data);
    void *user_data;                    /**< 用户数据 */
} TcpRaftTransport_t;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 创建 TCP Raft 传输层
 *
 * @param config 传输层配置
 * @return 成功返回传输层指针，失败返回 NULL
 */
RaftTransport_t *tcp_raft_transport_create(const TcpRaftTransportConfig_t *config);

/**
 * @brief 销毁 Raft 传输层
 *
 * @param transport 传输层
 */
void raft_transport_destroy(RaftTransport_t *transport);

/**
 * @brief 启动传输层
 *
 * @param transport 传输层
 * @return 0 成功，非 0 失败
 */
int raft_transport_start(RaftTransport_t *transport);

/**
 * @brief 停止传输层
 *
 * @param transport 传输层
 * @return 0 成功，非 0 失败
 */
int raft_transport_stop(RaftTransport_t *transport);

/**
 * @brief 发送 RequestVote RPC
 *
 * @param transport 传输层
 * @param to_node 目标节点
 * @param args RequestVote 参数
 * @param result RequestVote 结果
 * @return 0 成功，非 0 失败
 */
int raft_transport_send_request_vote(RaftTransport_t *transport,
                                     uint64_t to_node,
                                     const RaftRequestVoteArgs_t *args,
                                     RaftRequestVoteResult_t *result);

/**
 * @brief 发送 AppendEntries RPC
 *
 * @param transport 传输层
 * @param to_node 目标节点
 * @param args AppendEntries 参数
 * @param result AppendEntries 结果
 * @return 0 成功，非 0 失败
 */
int raft_transport_send_append_entries(RaftTransport_t *transport,
                                       uint64_t to_node,
                                       const RaftAppendEntriesArgs_t *args,
                                       RaftAppendEntriesResult_t *result);

/**
 * @brief 发送快照
 *
 * @param transport 传输层
 * @param to_node 目标节点
 * @param data 快照数据
 * @param size 快照大小
 * @param last_index 快照最后索引
 * @param last_term 快照最后 term
 * @return 0 成功，非 0 失败
 */
int raft_transport_send_snapshot(RaftTransport_t *transport,
                                uint64_t to_node,
                                const void *data,
                                size_t size,
                                uint64_t last_index,
                                uint64_t last_term);

/**
 * @brief 发送心跳
 *
 * @param transport 传输层
 * @param to_node 目标节点
 * @return 0 成功，非 0 失败
 */
int raft_transport_send_heartbeat(RaftTransport_t *transport, uint64_t to_node);

/**
 * @brief 批量发送心跳到所有节点
 *
 * @param transport 传输层
 * @return 0 成功，非 0 失败
 */
int raft_transport_broadcast_heartbeat(RaftTransport_t *transport);

/**
 * @brief 检查节点是否可达
 *
 * @param transport 传输层
 * @param node_id 节点 ID
 * @return true 可达，false 不可达
 */
bool raft_transport_is_peer_reachable(RaftTransport_t *transport, uint64_t node_id);

/**
 * @brief 获取活跃连接数
 *
 * @param transport 传输层
 * @return 活跃连接数
 */
uint32_t raft_transport_get_active_connection_count(const RaftTransport_t *transport);

/**
 * @brief 设置 RPC 回调
 *
 * @param transport 传输层
 * @param on_request_vote RequestVote 回调
 * @param on_append_entries AppendEntries 回调
 * @param on_snapshot 快照回调
 * @param on_heartbeat 心跳回调
 * @param user_data 用户数据
 */
void raft_transport_set_callbacks(RaftTransport_t *transport,
                                  void (*on_request_vote)(uint64_t, const RaftRequestVoteArgs_t *,
                                                          RaftRequestVoteResult_t *, void *),
                                  void (*on_append_entries)(uint64_t, const RaftAppendEntriesArgs_t *,
                                                           RaftAppendEntriesResult_t *, void *),
                                  void (*on_snapshot)(uint64_t, const void *, size_t,
                                                     uint64_t, uint64_t, void *),
                                  void (*on_heartbeat)(uint64_t, void *),
                                  void *user_data);

/* ========================================================================
 * 消息编解码
 * ======================================================================== */

/**
 * @brief 编码 RequestVote 消息
 *
 * @param args RequestVote 参数
 * @param payload 输出缓冲区
 * @param capacity 缓冲区容量
 * @return 实际编码字节数
 */
size_t raft_encode_request_vote(const RaftRequestVoteArgs_t *args,
                               void *payload, size_t capacity);

/**
 * @brief 解码 RequestVote 消息
 *
 * @param payload 输入数据
 * @param size 数据大小
 * @param args 输出参数
 * @return 0 成功，非 0 失败
 */
int raft_decode_request_vote(const void *payload, size_t size,
                            RaftRequestVoteArgs_t *args);

/**
 * @brief 编码 AppendEntries 消息
 *
 * @param args AppendEntries 参数
 * @param payload 输出缓冲区
 * @param capacity 缓冲区容量
 * @return 实际编码字节数
 */
size_t raft_encode_append_entries(const RaftAppendEntriesArgs_t *args,
                                 void *payload, size_t capacity);

/**
 * @brief 解码 AppendEntries 消息
 *
 * @param payload 输入数据
 * @param size 数据大小
 * @param args 输出参数
 * @return 0 成功，非 0 失败
 */
int raft_decode_append_entries(const void *payload, size_t size,
                              RaftAppendEntriesArgs_t *args);

/**
 * @brief 编码 RequestVote 响应
 *
 * @param result 响应结果
 * @param payload 输出缓冲区
 * @param capacity 缓冲区容量
 * @return 实际编码字节数
 */
size_t raft_encode_request_vote_resp(const RaftRequestVoteResult_t *result,
                                    void *payload, size_t capacity);

/**
 * @brief 解码 RequestVote 响应
 *
 * @param payload 输入数据
 * @param size 数据大小
 * @param result 输出结果
 * @return 0 成功，非 0 失败
 */
int raft_decode_request_vote_resp(const void *payload, size_t size,
                                 RaftRequestVoteResult_t *result);

/**
 * @brief 编码 AppendEntries 响应
 *
 * @param result 响应结果
 * @param payload 输出缓冲区
 * @param capacity 缓冲区容量
 * @return 实际编码字节数
 */
size_t raft_encode_append_entries_resp(const RaftAppendEntriesResult_t *result,
                                      void *payload, size_t capacity);

/**
 * @brief 解码 AppendEntries 响应
 *
 * @param payload 输入数据
 * @param size 数据大小
 * @param result 输出结果
 * @return 0 成功，非 0 失败
 */
int raft_decode_append_entries_resp(const void *payload, size_t size,
                                   RaftAppendEntriesResult_t *result);

#ifdef __cplusplus
}
#endif

#endif /* DB_CONSENSUS_RAFT_TRANSPORT_H */
