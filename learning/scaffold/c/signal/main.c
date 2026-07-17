/*
 * signal scaffold — 自触发信号 + 优雅停机演示
 *
 * 演示场景：
 *   - 注册 SIGINT（Ctrl-C）触发优雅退出
 *   - 注册 SIGUSR1 触发指标 dump（自触发：用 kill(getpid(), SIGUSR1)）
 *   - 主循环跑"工单"，每 5 次工单 dump 一次指标，每收到信号检查 stop 标志
 *
 * 用法:
 *   ./signal_demo            # 跑 30 次工单或到 Ctrl-C
 *
 * 工程意义：
 *   - rag server 优雅停机（不等 client 强杀）
 *   - SIGUSR1 触发 metrics dump 到 stdout（生产 pg_dump / prom exporter 风格）
 */

/* POSIX-only: 依赖 sigaction 自重启语义 */
#ifdef _WIN32
# error "signal scaffold 不支持 Windows；请用 WSL 或原生 Linux/macOS"
#endif

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_dump = 0;

static void on_sigint(int signum) {
    (void)signum;
    g_stop = 1;
}

static void on_sigusr1(int signum) {
    (void)signum;
    g_dump = 1;
}

static int install_handler(int signum, void (*handler)(int), int flags) {
    struct sigaction sa = {0};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = flags;
    if (sigaction(signum, &sa, NULL) != 0) {
        perror("sigaction");
        return 1;
    }
    return 0;
}

static void dump_metrics(long processed, long errors) {
    fprintf(stderr, "[metrics] processed=%ld errors=%ld rss_kb=%ld\n",
            processed, errors, (long)(get_current_dummy_rss()));
    (void)processed;
    (void)errors;
}

/* 简易伪 RSS（避免引入依赖） */
static long get_current_dummy_rss(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    long rss = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &rss);
            break;
        }
    }
    fclose(f);
    return rss * 1024 / 1024; /* kB */
}

int main(void) {
    install_handler(SIGINT, on_sigint, 0);
    install_handler(SIGUSR1, on_sigusr1, SA_RESTART);

    long processed = 0;
    long errors = 0;
    long dump_counter = 0;

    printf("[signal] running pid=%d (Ctrl-C to stop, kill -USR1 %d to dump)\n",
           (int)getpid(), (int)getpid());
    fflush(stdout);

    while (!g_stop) {
        /* 模拟一个工单：sleep 1 然后假装处理一下 */
        struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
        nanosleep(&ts, NULL);
        processed++;

        /* 每 5 个工单自触发一次 SIGUSR1 */
        if ((processed % 5) == 0) {
            dump_counter++;
            if (kill(getpid(), SIGUSR1) != 0) {
                perror("kill");
                errors++;
            }
        }

        /* 自旋等待 g_stop，sleep 间隔内可能漏信号检查 */
        if (g_dump) {
            g_dump = 0;
            dump_metrics(processed, errors);
        }
    }

    /* 优雅停机：处理最后几个挂起的事情 */
    printf("[signal] graceful stop (processed=%ld, errors=%ld, dumps=%ld)\n",
           processed, errors, dump_counter);
    return 0;
}
