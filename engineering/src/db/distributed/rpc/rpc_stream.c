/* rpc_stream.c - 流式 TCP 帧通道（Gap#4 Task 9）
 *
 * controller resolutions：
 * - CRC32 表用 pthread_once 一次性初始化（镜像 px_wire.c 的根修复，
 *   消除 lazy static int init 标志的数据竞争）。
 * - rpcs_wsa_init 同样走 pthread_once（WSAStartup 幂等，但静态标志
 *   本身有竞争，统一用 once 根除）。
 * - 帧尺寸上限直接用 rpc.h 的 RPC_MAX_MESSAGE_SIZE（已存在，64MB）。
 */
#include "db/distributed/rpc_stream.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define RPCS_INVALID INVALID_SOCKET
#define SHUT_RDWR SD_BOTH
typedef SOCKET rpcs_fd_t;
static int rpcs_close_fd(rpcs_fd_t fd) { return closesocket(fd); }
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define RPCS_INVALID (-1)
typedef int rpcs_fd_t;
static int rpcs_close_fd(rpcs_fd_t fd) { return close(fd); }
#endif

#define RPCS_MAGIC 0x31535052u   /* "RPS1" 小端 */

/* CRC32 与 px_wire 同源（多项式 0xEDB88320），pthread_once 一次性建表 */
static uint32_t rpcs_crc_table[256];
static pthread_once_t rpcs_crc_once = PTHREAD_ONCE_INIT;

static void rpcs_crc_table_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        rpcs_crc_table[i] = c;
    }
}

static uint32_t rpcs_crc32(const uint8_t *data, size_t size) {
    pthread_once(&rpcs_crc_once, rpcs_crc_table_init);
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++)
        crc = rpcs_crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

#ifdef _WIN32
static pthread_once_t rpcs_wsa_once = PTHREAD_ONCE_INIT;
static void rpcs_wsa_startup(void) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
}
#endif

static void rpcs_wsa_init(void) {
#ifdef _WIN32
    pthread_once(&rpcs_wsa_once, rpcs_wsa_startup);
#endif
}

/* ---- 发送方 ---- */

struct rpc_stream {
    rpcs_fd_t fd;
    pthread_mutex_t send_mu;   /* 多 worker 共享一条流时串行化整帧 */
};

rpc_stream_t *rpc_stream_connect(const rpc_node_address_t *addr) {
    if (!addr) return NULL;
    rpcs_wsa_init();

    rpcs_fd_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == RPCS_INVALID) return NULL;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(addr->port);
#ifdef _WIN32
    sa.sin_addr.s_addr = inet_addr(addr->host);
#else
    inet_pton(AF_INET, addr->host, &sa.sin_addr);
#endif

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        rpcs_close_fd(fd);
        return NULL;
    }

    rpc_stream_t *s = (rpc_stream_t *)calloc(1, sizeof(rpc_stream_t));
    if (!s) { rpcs_close_fd(fd); return NULL; }
    s->fd = fd;
    pthread_mutex_init(&s->send_mu, NULL);
    return s;
}

static int send_all(rpcs_fd_t fd, const uint8_t *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        int rc = send(fd, (const char *)buf + sent, (int)(n - sent), 0);
        if (rc <= 0) return -1;
        sent += (size_t)rc;
    }
    return 0;
}

int rpc_stream_send(rpc_stream_t *s, uint8_t frame_type,
                    const void *data, uint32_t size) {
    if (!s || (size > 0 && !data)) return -1;
    pthread_mutex_lock(&s->send_mu);

    uint8_t hdr[9];
    uint32_t magic = RPCS_MAGIC;
    memcpy(hdr, &magic, 4);
    hdr[4] = frame_type;
    memcpy(hdr + 5, &size, 4);

    uint32_t crc = rpcs_crc32((const uint8_t *)data, size);

    int rc = 0;
    if (send_all(s->fd, hdr, 9) != 0) rc = -1;
    if (rc == 0 && size > 0 && send_all(s->fd, (const uint8_t *)data, size) != 0) rc = -1;
    if (rc == 0 && send_all(s->fd, (const uint8_t *)&crc, 4) != 0) rc = -1;

    pthread_mutex_unlock(&s->send_mu);
    return rc;
}

void rpc_stream_close(rpc_stream_t *s) {
    if (!s) return;
    rpc_stream_send(s, RPCS_FRAME_END, NULL, 0);
    rpcs_close_fd(s->fd);
    pthread_mutex_destroy(&s->send_mu);
    free(s);
}

/* A6：不发 END 直接断连（模拟节点崩溃）。对端 rpcs_serve_conn 检测
 * 到无 END 退出后合成 RPCS_FRAME_ABORT 伪帧通知 cb（见 A5）。 */
void rpc_stream_abort(rpc_stream_t *s) {
    if (!s) return;
    rpcs_close_fd(s->fd);
    pthread_mutex_destroy(&s->send_mu);
    free(s);
}

/* ---- 接收方 ---- */

struct rpcs_listener {
    rpcs_fd_t listen_fd;
    rpcs_frame_cb cb;
    void *ctx;
    pthread_t thread;
    volatile int stop;
};

static int recv_all(rpcs_fd_t fd, uint8_t *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        int rc = recv(fd, (char *)buf + got, (int)(n - got), 0);
        if (rc <= 0) return -1;
        got += (size_t)rc;
    }
    return 0;
}

static void rpcs_serve_conn(rpcs_fd_t cfd, rpcs_frame_cb cb, void *ctx) {
    int saw_end = 0;   /* A5：区分正常 END 收尾与对端异常断开 */
    for (;;) {
        uint8_t hdr[9];
        if (recv_all(cfd, hdr, 9) != 0) break;
        uint32_t magic, size;
        memcpy(&magic, hdr, 4);
        memcpy(&size, hdr + 5, 4);
        if (magic != RPCS_MAGIC || size > RPC_MAX_MESSAGE_SIZE) break;

        uint8_t *payload = NULL;
        if (size > 0) {
            payload = (uint8_t *)malloc(size);
            if (!payload) break;
            if (recv_all(cfd, payload, size) != 0) { free(payload); break; }
        }
        uint32_t crc;
        if (recv_all(cfd, (uint8_t *)&crc, 4) != 0) { free(payload); break; }
        if (rpcs_crc32(payload, size) != crc) { free(payload); break; }

        cb(hdr[4], payload, size, ctx);
        free(payload);
        if (hdr[4] == RPCS_FRAME_END) { saw_end = 1; break; }
    }
    /* A5：对端未发 END 就断开（崩溃/rpc_stream_abort）时合成 ABORT 伪帧，
     * 否则接收方队列永远等不到 producer_done/abort，next() 永久阻塞 */
    if (!saw_end) cb(RPCS_FRAME_ABORT, NULL, 0, ctx);
    rpcs_close_fd(cfd);
}

static void *rpcs_accept_loop(void *varg) {
    rpcs_listener_t *l = (rpcs_listener_t *)varg;
    while (!l->stop) {
        rpcs_fd_t cfd = accept(l->listen_fd, NULL, NULL);
        if (cfd == RPCS_INVALID) break;    /* stop 时 listen_fd 被关 */
        rpcs_serve_conn(cfd, l->cb, l->ctx);
    }
    return NULL;
}

rpcs_listener_t *rpcs_listen(const rpc_node_address_t *bind_addr,
                             rpcs_frame_cb cb, void *ctx) {
    if (!bind_addr || !cb) return NULL;
    rpcs_wsa_init();

    rpcs_fd_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == RPCS_INVALID) return NULL;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(bind_addr->port);
#ifdef _WIN32
    sa.sin_addr.s_addr = inet_addr(bind_addr->host);
#else
    inet_pton(AF_INET, bind_addr->host, &sa.sin_addr);
#endif

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0
        || listen(fd, 16) != 0) {
        rpcs_close_fd(fd);
        return NULL;
    }

    rpcs_listener_t *l = (rpcs_listener_t *)calloc(1, sizeof(rpcs_listener_t));
    if (!l) { rpcs_close_fd(fd); return NULL; }
    l->listen_fd = fd;
    l->cb = cb;
    l->ctx = ctx;
    l->stop = 0;
    if (pthread_create(&l->thread, NULL, rpcs_accept_loop, l) != 0) {
        rpcs_close_fd(fd);
        free(l);
        return NULL;
    }
    return l;
}

void rpcs_listener_stop(rpcs_listener_t *l) {
    if (!l) return;
    l->stop = 1;
    shutdown(l->listen_fd, SHUT_RDWR);  /* 唤醒阻塞的 accept；Windows 监听 socket 上会失败，无害（closesocket 仍可唤醒） */
    rpcs_close_fd(l->listen_fd);           /* 唤醒阻塞的 accept */
    pthread_join(l->thread, NULL);
    free(l);
}
