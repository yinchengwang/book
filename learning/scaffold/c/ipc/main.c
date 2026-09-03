/*
 * ipc scaffold — POSIX 三件 IPC：pipe / FIFO / signal
 *
 * 演示场景：
 *   - pipe：父子进程单向传递 8-byte 消息
 *   - FIFO：命令行参数指定路径，让任意两个进程通信
 *   - signal：父进程注册 SIGUSR1 处理器，子进程触发
 *
 * 用法：
 *   ./ipc_demo           // 默认跑全部三个
 *   ./ipc_demo pipe
 *   ./ipc_demo fifo <path>
 *   ./ipc_demo signal
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

/* POSIX-only：依赖 sys/wait.h 与原生 pipe/FIFO；Windows/MSYS 用户请用 WSL */
/* mingw-w64 不带 <sys/wait.h>，MinGW 用户请跳过本 scaffold 在 WSL 内运行 */
#ifdef _WIN32
# warning "ipc scaffold 不支持 MinGW-w64，请在 WSL 或 Linux/macOS 下运行"
#endif
#include <sys/wait.h>

static volatile sig_atomic_t g_signal_received = 0;

static void on_sigusr1(int signum) {
    (void)signum;
    g_signal_received = 1;
}

static int demo_pipe(void) {
    int fd[2];
    char msg[] = "hello-pipe";
    char buf[64] = {0};

    if (pipe(fd) != 0) { perror("pipe"); return 1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        /* child: 写入 */
        close(fd[0]);
        write(fd[1], msg, sizeof(msg));
        close(fd[1]);
        _exit(0);
    }
    /* parent: 读取 */
    close(fd[1]);
    ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
    close(fd[0]);
    int status;
    waitpid(pid, &status, 0);

    if (n <= 0) { fprintf(stderr, "read failed\n"); return 1; }
    printf("[pipe] parent read: '%s' (n=%zd)\n", buf, n);
    return strcmp(buf, msg) == 0 ? 0 : 1;
}

static int demo_fifo(const char *path) {
    /* create FIFO if missing */
    if (mkfifo(path, 0644) != 0 && errno != EEXIST) {
        perror("mkfifo");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        /* writer */
        int fd = open(path, O_WRONLY);
        if (fd < 0) { perror("open"); _exit(1); }
        const char *msg = "via-fifo";
        write(fd, msg, strlen(msg));
        close(fd);
        _exit(0);
    }
    /* reader */
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    char buf[64] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    waitpid(pid, NULL, 0);
    unlink(path);

    if (n <= 0) return 1;
    printf("[fifo] read: '%s' (n=%zd)\n", buf, n);
    return 0;
}

static int demo_signal(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_sigusr1;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        perror("sigaction");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        /* child: 给父进程发信号 */
        kill(getppid(), SIGUSR1);
        _exit(0);
    }
    /* parent: 等待信号 */
    while (!g_signal_received) {
        pause();
    }
    int status;
    waitpid(pid, &status, 0);
    printf("[signal] parent received SIGUSR1 from child (pid=%d)\n", pid);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "pipe") == 0)  return demo_pipe();
    if (argc >= 2 && strcmp(argv[1], "fifo") == 0)  return demo_fifo(argc >= 3 ? argv[2] : "/tmp/ipc-scaffold.fifo");
    if (argc >= 2 && strcmp(argv[1], "signal") == 0) return demo_signal();
    /* default: 全部 */
    int rc = 0;
    rc |= demo_pipe();
    rc |= demo_fifo("/tmp/ipc-scaffold.fifo");
    rc |= demo_signal();
    printf("OK\n");
    return rc;
}
