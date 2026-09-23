/**
 * @file rpc_stream.h
 * @brief Gap#4 流式 TCP 帧通道——pxwire 数据面的传输层
 *
 * 独立于 rpc.h 连接池：发送方一条专用 socket 顺序发帧，
 * 接收方监听线程 accept 后按帧回调。帧含 CRC32。
 * close 自动发送 RPCS_FRAME_END 通知对端流结束。
 */
#ifndef DB_DISTRIBUTED_RPC_STREAM_H
#define DB_DISTRIBUTED_RPC_STREAM_H

#include <stdint.h>
#include <pthread.h>   /* rpc.h 的公开结构体内嵌 pthread_mutex_t/pthread_t，需先引入 */
#include "rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RPCS_FRAME_DATA 0x05u
#define RPCS_FRAME_END  0x06u

typedef struct rpc_stream rpc_stream_t;
typedef struct rpcs_listener rpcs_listener_t;

typedef void (*rpcs_frame_cb)(uint8_t frame_type, const uint8_t *data,
                              uint32_t size, void *ctx);

rpc_stream_t    *rpc_stream_connect(const rpc_node_address_t *addr);
int              rpc_stream_send(rpc_stream_t *s, uint8_t frame_type,
                                 const void *data, uint32_t size);
void             rpc_stream_close(rpc_stream_t *s);

rpcs_listener_t *rpcs_listen(const rpc_node_address_t *bind_addr,
                             rpcs_frame_cb cb, void *ctx);
void             rpcs_listener_stop(rpcs_listener_t *l);

#ifdef __cplusplus
}
#endif

#endif /* DB_DISTRIBUTED_RPC_STREAM_H */
