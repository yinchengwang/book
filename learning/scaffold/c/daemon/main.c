/*
 * daemon scaffold — 经典 daemonize 流程
 *
 * 流程：
 *   1. fork
 *   2. setsid (脱离控制终端)
 *   3. 第二次 fork（防止重新打开 tty）
 *   4. umask(0)
 *   5. chdir("/")
 *   6. close fd 0/1/2，重定向到 /dev/null 或 logfile
 *   7. 写 pidfile
 *   8. 注册 SIGHUP/SIGTERM handler
 *   9. 主循环
 *
 * 用法:
 *   ./daemon_demo                 # 前台跑（打印 fork 后状态）
 *   ./daemon_demo --background    # 真正 daemonize
 *
 * 工程意义：
 *   - minivecdb initdb / pg_ctl 风格控制面
 *   - rag server 独立进程
 */

/* POSIX-only */
#ifdef _WIN32
# error "daemon scaffold 不支持 Windows；请用 WSL 或原生 Linux/macOS"
#endif

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

#ifndef PIDFILE_PATH
#define PIDFILE_PATH "/tmp/daemon-scaffold.pid"
#endif

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_hup  = 0;

static void on_term(int signum) { (void)signum; g_stop = 1; }
static void on_hup(int signum)  { (void)signum; g_hup  = 1; }

/* daemonize 五步 */
static int daemonize(void) {
    pid_t pid;

    /* 1) 第一次 fork */
    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);  /* parent 退出 */

    /* 2) setsid */
    if (setsid() < 0) return -1;

    /* 3) 第二次 fork（确保未来不会获取 tty） */
    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);

    /* 4) 清理 */
    umask(0);

    /* chdir 到根（避免挂载 umount 受阻） */
    if (chdir("/") != 0) { /* 容忍 */ }

    /* 5) 关闭并重定向 std fd */
    int fd_null = open("/dev/null", O_RDWR);
    if (fd_null >= 0) {
        dup2(fd_null, STDIN_FILENO);
        dup2(fd_null, STDOUT_FILENO);
        dup2(fd_null, STDERR_FILENO);
        if (fd_null > STDERR_FILENO) close(fd_null);
    }
    return 0;
}

static int write_pidfile(pid_t pid) {
    FILE *f = fopen(PIDFILE_PATH, "w");
    if (!f) return -1;
    fprintf(f, "%d\n", (int)pid);
    fclose(f);
    return 0;
}

static int install_handler(int sig, void (*handler)(int)) {
    struct sigaction sa = {0};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    return sigaction(sig, &sa, NULL);
}

static void worker_loop(const char *name) {
    long tick = 0;
    while (!g_stop) {
        struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
        nanosleep(&ts, NULL);
        tick++;

        if (g_hup) {
            g_hup = 0;
            fprintf(stderr, "[daemon] %s: SIGHUP 收到，假装重读配置\n", name);
        }

        if ((tick % 5) == 0 && !g_stop) {
            fprintf(stderr, "[daemon] %s: tick=%ld\n", name, tick);
        }
    }
}

int main(int argc, char **argv) {
    int background = (argc >= 2 && strcmp(argv[1], "--background") == 0);

    if (background) {
        if (daemonize() != 0) { fprintf(stderr, "daemonize failed\n"); return 1; }
        if (write_pidfile(getpid()) != 0) { fprintf(stderr, "write pidfile failed\n"); return 1; }
    }

    install_handler(SIGTERM, on_term);
    install_handler(SIGINT,  on_term);
    install_handler(SIGHUP,  on_hup);

    fprintf(stderr, "[daemon] started pid=%d pidfile=%s\n", (int)getpid(), PIDFILE_PATH);
    worker_loop("daemon-demo");

    if (background) unlink(PIDFILE_PATH);
    fprintf(stderr, "[daemon] graceful stop\n");
    return 0;
}
