#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>
#include <mutex>

extern "C" {
#include "db/distributed/rpc_stream.h"
#include "db/distributed/rpc.h"
}

struct Sink {
    std::mutex mu;
    std::vector<std::vector<uint8_t>> frames;
    std::atomic<int> ended{0};
};

static void sink_cb(uint8_t type, const uint8_t *data, uint32_t size, void *vctx) {
    Sink *s = (Sink *)vctx;
    if (type == RPCS_FRAME_END) { s->ended++; return; }
    std::lock_guard<std::mutex> lk(s->mu);
    s->frames.emplace_back(data, data + size);
}

TEST(RpcStream, FramesArriveInOrder) {
    Sink sink;
    rpc_node_address_t addr{};
    strcpy(addr.host, "127.0.0.1");
    addr.port = 19571;
    addr.node_id = 1;

    rpcs_listener_t *l = rpcs_listen(&addr, sink_cb, &sink);
    ASSERT_NE(l, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));   /* 等监听就绪 */

    rpc_stream_t *s = rpc_stream_connect(&addr);
    ASSERT_NE(s, nullptr);

    for (uint32_t i = 0; i < 100; i++) {
        uint32_t payload[64];
        for (int j = 0; j < 64; j++) payload[j] = i * 64 + j;
        ASSERT_EQ(rpc_stream_send(s, RPCS_FRAME_DATA, payload, sizeof(payload)), 0);
    }
    rpc_stream_close(s);   /* 自动发 END */

    for (int i = 0; i < 50 && sink.ended == 0; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(sink.ended.load(), 1);

    std::lock_guard<std::mutex> lk(sink.mu);
    ASSERT_EQ(sink.frames.size(), 100u);
    for (uint32_t i = 0; i < 100; i++) {
        const uint32_t *p = (const uint32_t *)sink.frames[i].data();
        ASSERT_EQ(sink.frames[i].size(), 256u);
        EXPECT_EQ(p[0], i * 64);
        EXPECT_EQ(p[63], i * 64 + 63);
    }
    rpcs_listener_stop(l);
}

TEST(RpcStream, ConnectRefusedReturnsNull) {
    rpc_node_address_t addr{};
    strcpy(addr.host, "127.0.0.1");
    addr.port = 19599;                 /* 无监听 */
    addr.node_id = 99;
    EXPECT_EQ(rpc_stream_connect(&addr), nullptr);
}
