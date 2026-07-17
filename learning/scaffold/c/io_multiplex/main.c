/*
 * io_multiplex scaffold — epoll LT 模式 echo server
 *
 * 用法（Linux only，需 WSL 或原生 Linux）:
 *   ./epoll_demo <port>           # 默认 9000
 *   ./epoll_demo 9001
 *
 * 客户端测试:
 *   nc 127.0.0.1 9000             # 输入什么返回什么 + 换行
 *
 * 设计要点：
 *   - LT（level-triggered）模式：每次 epoll_wait 后只读一次，未读完下次继续触发
 *   - 单线程：epoll 同时管理 listen fd 与所有 client fd
 *   - 所有 IO 必须 non-blocking；EAGAIN 是常态不是错
 */

/* Linux-only: 依赖 <sys/epoll.h> 与 epoll_wait。Windows/MSYS 不支持 */
#ifdef _WIN32
# error "io_multiplex scaffold 不支持 Windows；请用 WSL 或原生 Linux"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_EVENTS 64
#define BUF_SIZE   4096

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int serve(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        return 1;
    }
    if (listen(listen_fd, 16) != 0) { perror("listen"); return 1; }
    set_nonblock(listen_fd);
    printf("[epoll] listening on port %d\n", port);

    int epfd = epoll_create1(0);
    if (epfd < 0) { perror("epoll_create1"); return 1; }

    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) != 0) {
        perror("epoll_ctl");
        return 1;
    }

    struct epoll_event events[MAX_EVENTS];
    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (n < 0) { if (errno == EINTR) continue; perror("epoll_wait"); break; }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                /* accept all pending */
                while (1) {
                    int cfd = accept4(listen_fd, NULL, NULL, SOCK_NONBLOCK);
                    if (cfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept4");
                        break;
                    }
                    struct epoll_event cev = {0};
                    cev.events = EPOLLIN;
                    cev.data.fd = cfd;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &cev);
                    printf("[epoll] client fd=%d connected\n", cfd);
                }
            } else {
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    close(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    printf("[epoll] client fd=%d hung up\n", fd);
                    continue;
                }
                /* read & echo (LT 模式只读一次) */
                char buf[BUF_SIZE];
                ssize_t total = 0;
                while (1) {
                    ssize_t r = read(fd, buf + total, BUF_SIZE - total);
                    if (r > 0) { total += r; continue; }
                    if (r == 0) {
                        /* peer closed */
                        close(fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        printf("[epoll] client fd=%d closed\n", fd);
                        break;
                    }
                    /* r < 0 */
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        /* 已无更多数据；写回 buffer */
                        if (total > 0) {
                            ssize_t w = write(fd, buf, total);
                            (void)w;
                        }
                        break;
                    }
                    perror("read");
                    close(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    break;
                }
            }
        }
    }

    close(epfd);
    close(listen_fd);
    return 0;
}

int main(int argc, char **argv) {
    int port = argc >= 2 ? atoi(argv[1]) : 9000;
    return serve(port);
}
