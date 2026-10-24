/**
 * @file raft_transport_test.c
 * @brief Raft 网络传输层测试
 */

#include "db/consensus/raft_transport.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * 消息编解码测试
 * ======================================================================== */

static void test_encode_decode_request_vote(void) {
    printf("Testing RequestVote encode/decode...\n");

    RaftRequestVoteArgs_t args = {
        .term = 5,
        .candidate_id = 100,
        .last_log_index = 42,
        .last_log_term = 3
    };

    uint8_t buffer[256];
    size_t encoded = raft_encode_request_vote(&args, buffer, sizeof(buffer));
    assert(encoded == 32);

    RaftRequestVoteArgs_t decoded;
    int ret = raft_decode_request_vote(buffer, encoded, &decoded);
    assert(ret == 0);
    assert(decoded.term == args.term);
    assert(decoded.candidate_id == args.candidate_id);
    assert(decoded.last_log_index == args.last_log_index);
    assert(decoded.last_log_term == args.last_log_term);

    printf("  RequestVote encode/decode: PASSED\n");
}

static void test_encode_decode_request_vote_resp(void) {
    printf("Testing RequestVoteResp encode/decode...\n");

    RaftRequestVoteResult_t result = {
        .term = 5,
        .vote_granted = true
    };

    uint8_t buffer[64];
    size_t encoded = raft_encode_request_vote_resp(&result, buffer, sizeof(buffer));
    assert(encoded == 16);

    RaftRequestVoteResult_t decoded;
    int ret = raft_decode_request_vote_resp(buffer, encoded, &decoded);
    assert(ret == 0);
    assert(decoded.term == result.term);
    assert(decoded.vote_granted == result.vote_granted);

    printf("  RequestVoteResp encode/decode: PASSED\n");
}

static void test_encode_decode_append_entries(void) {
    printf("Testing AppendEntries encode/decode...\n");

    RaftAppendEntriesArgs_t args = {
        .term = 5,
        .leader_id = 100,
        .prev_log_index = 41,
        .prev_log_term = 3,
        .leader_commit = 40,
        .entry_count = 2,
        .entries = NULL /* 简化测试，不包含entries */
    };

    uint8_t buffer[1024];
    size_t encoded = raft_encode_append_entries(&args, buffer, sizeof(buffer));
    assert(encoded > 0);

    RaftAppendEntriesArgs_t decoded;
    int ret = raft_decode_append_entries(buffer, encoded, &decoded);
    assert(ret == 0);
    assert(decoded.term == args.term);
    assert(decoded.leader_id == args.leader_id);
    assert(decoded.prev_log_index == args.prev_log_index);
    assert(decoded.leader_commit == args.leader_commit);

    printf("  AppendEntries encode/decode: PASSED\n");
}

static void test_encode_decode_append_entries_resp(void) {
    printf("Testing AppendEntriesResp encode/decode...\n");

    RaftAppendEntriesResult_t result = {
        .term = 5,
        .success = true,
        .match_index = 42
    };

    uint8_t buffer[64];
    size_t encoded = raft_encode_append_entries_resp(&result, buffer, sizeof(buffer));
    assert(encoded == 24);

    RaftAppendEntriesResult_t decoded;
    int ret = raft_decode_append_entries_resp(buffer, encoded, &decoded);
    assert(ret == 0);
    assert(decoded.term == result.term);
    assert(decoded.success == result.success);
    assert(decoded.match_index == result.match_index);

    printf("  AppendEntriesResp encode/decode: PASSED\n");
}

/* ========================================================================
 * 传输层创建测试
 * ======================================================================== */

static void test_transport_creation(void) {
    printf("Testing transport creation...\n");

    /* 配置节点地址 */
    RaftNodeAddr_t peers[2];
    peers[0].node_id = 2;
    strcpy(peers[0].host, "127.0.0.1");
    peers[0].port = 9002;

    peers[1].node_id = 3;
    strcpy(peers[1].host, "127.0.0.1");
    peers[1].port = 9003;

    /* 创建传输层配置 */
    TcpRaftTransportConfig_t config = {
        .local_node_id = 1,
        .listen_port = 9001,
        .peers = peers,
        .peer_count = 2,
        .connect_timeout_ms = 5000,
        .request_timeout_ms = 30000,
        .heartbeat_interval_ms = 150,
        .max_retry = 3
    };

    /* 创建传输层 */
    RaftTransport_t *transport = tcp_raft_transport_create(&config);
    assert(transport != NULL);
    assert(transport->ops != NULL);
    assert(transport->impl != NULL);

    /* 启动传输层 */
    int ret = raft_transport_start(transport);
    assert(ret == 0);

    /* 检查活跃连接数（初始为0，因为还没连接） */
    uint32_t count = raft_transport_get_active_connection_count(transport);
    printf("  Initial active connections: %u\n", count);

    /* 停止传输层 */
    ret = raft_transport_stop(transport);
    assert(ret == 0);

    /* 销毁传输层 */
    raft_transport_destroy(transport);

    printf("  Transport creation: PASSED\n");
}

static void test_peer_reachability(void) {
    printf("Testing peer reachability...\n");

    RaftNodeAddr_t peers[1];
    peers[0].node_id = 2;
    strcpy(peers[0].host, "127.0.0.1");
    peers[0].port = 9002;

    TcpRaftTransportConfig_t config = {
        .local_node_id = 1,
        .listen_port = 9001,
        .peers = peers,
        .peer_count = 1,
        .connect_timeout_ms = 5000,
        .request_timeout_ms = 30000,
        .heartbeat_interval_ms = 150,
        .max_retry = 3
    };

    RaftTransport_t *transport = tcp_raft_transport_create(&config);
    assert(transport != NULL);

    /* 未连接时应该不可达 */
    bool reachable = raft_transport_is_peer_reachable(transport, 2);
    assert(reachable == false);

    raft_transport_destroy(transport);

    printf("  Peer reachability: PASSED\n");
}

static void test_callback_registration(void) {
    printf("Testing callback registration...\n");

    RaftNodeAddr_t peers[1];
    peers[0].node_id = 2;
    strcpy(peers[0].host, "127.0.0.1");
    peers[0].port = 9002;

    TcpRaftTransportConfig_t config = {
        .local_node_id = 1,
        .listen_port = 9001,
        .peers = peers,
        .peer_count = 1,
        .connect_timeout_ms = 5000,
        .request_timeout_ms = 30000,
        .heartbeat_interval_ms = 150,
        .max_retry = 3
    };

    RaftTransport_t *transport = tcp_raft_transport_create(&config);
    assert(transport != NULL);

    /* 设置回调（这些回调在收到RPC时会被调用） */
    raft_transport_set_callbacks(
        transport,
        NULL, /* on_request_vote */
        NULL, /* on_append_entries */
        NULL, /* on_snapshot */
        NULL, /* on_heartbeat */
        NULL  /* user_data */
    );

    raft_transport_destroy(transport);

    printf("  Callback registration: PASSED\n");
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Raft Transport Layer Tests ===\n\n");

    /* 消息编解码测试 */
    printf("--- Message Encode/Decode Tests ---\n");
    test_encode_decode_request_vote();
    test_encode_decode_request_vote_resp();
    test_encode_decode_append_entries();
    test_encode_decode_append_entries_resp();

    /* 传输层测试 */
    printf("\n--- Transport Layer Tests ---\n");
    test_transport_creation();
    test_peer_reachability();
    test_callback_registration();

    printf("\n=== All Tests Passed ===\n");
    return 0;
}
