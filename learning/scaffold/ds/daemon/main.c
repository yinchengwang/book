/**
 * @file main.c
 * @brief 守护进程概念演示（跨平台）
 *
 * 演示守护进程的核心概念（模拟在 Windows/跨平台环境下）：
 * 1. 长运行进程模式 - 模拟守护进程行为
 * 2. 信号处理概念 - Windows 等效实现
 * 3. 进程监控 - fork/waitpid 模式演示
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Windows 兼容处理 */
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #define getppid() GetCurrentProcessId()
#else
    #include <unistd.h>
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/wait.h>
#endif

/* ══════════════════════════════════════════════════════════════════════
 * 全局状态
 * ══════════════════════════════════════════════════════════════════════ */

static volatile int g_running = 1;
static volatile int g_reload = 0;
static volatile int g_signal_count = 0;

/* ══════════════════════════════════════════════════════════════════════
 * 1. 守护进程概念演示
 * ══════════════════════════════════════════════════════════════════════ */

/** 演示守护进程的核心特征 */
static void demo_daemon_concepts(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: 守护进程核心概念\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("\n  守护进程（Daemon）定义:\n");
    printf("    • 后台运行，不受终端控制\n");
    printf("    • 生命周期长，系统启动时运行，关闭时退出\n");
    printf("    • 通常作为服务运行（如 mysqld, nginx）\n");

    printf("\n  守护进程创建步骤（Linux）:\n");
    printf("    1. fork() - 创建子进程，父进程退出\n");
    printf("    2. setsid() - 创建新会话，成为 session leader\n");
    printf("    3. chdir(\"/\") - 切换到根目录\n");
    printf("    4. umask(0) - 重置文件权限掩码\n");
    printf("    5. close(STDIN/STDOUT/STDERR)\n");
    printf("    6. 重定向到 /dev/null\n");

    printf("\n  当前进程信息:\n");
    printf("    PID:   %d\n", (int)getpid());
    printf("    PPID:  %d\n", (int)getppid());
#ifndef _WIN32
    printf("    PGID:  %d\n", (int)getpgid(0));
    printf("    SID:   %d\n", (int)getsid(0));
#else
    printf("    (Windows 不支持 PGID/SID)\n");
#endif
}

/* ══════════════════════════════════════════════════════════════════════
 * 2. 信号处理演示（概念，Windows 使用线程模拟）
 * ══════════════════════════════════════════════════════════════════════ */

#ifndef _WIN32
/** SIGTERM 处理 */
static void sigterm_handler(int sig)
{
    (void)sig;
    g_running = 0;
    printf("\n  [信号] 收到 SIGTERM，准备退出...\n");
}

/** SIGUSR1 处理 */
static void sigusr1_handler(int sig)
{
    (void)sig;
    g_reload = 1;
    g_signal_count++;
    printf("\n  [信号] 收到 SIGUSR1，计数器=%d\n", g_signal_count);
}
#endif

/** 信号处理演示 */
static void demo_signal_handling(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: 信号处理（跨平台模拟）\n");
    printf("═══════════════════════════════════════════════════════════════\n");

#ifndef _WIN32
    /* Linux 信号处理 */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);

    printf("\n  已注册信号处理器:\n");
    printf("    SIGTERM (%d) -> 优雅退出\n", SIGTERM);
    printf("    SIGUSR1 (%d) -> 用户自定义（计数器）\n", SIGUSR1);
#else
    printf("\n  Windows 等效实现:\n");
    printf("    • 使用 SetConsoleCtrlHandler 模拟信号\n");
    printf("    • 或使用命名管道进行进程间通信\n");
#endif

    printf("\n  模拟主循环（5秒或收到信号）:\n");
    int seconds = 5;
    while (g_running && seconds > 0) {
        printf("  [%ds] 运行中...\r", seconds);
        fflush(stdout);
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
        if (g_reload) {
            g_reload = 0;
            printf("\n  [配置重载] 模拟重载完成\n");
        }
        seconds--;
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 3. 进程监控演示
 * ══════════════════════════════════════════════════════════════════════ */

#ifndef _WIN32
/** 演示父子进程关系 */
static void demo_process_monitor(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 3: 进程监控（fork/waitpid 模式）\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("\n  创建子进程...\n");
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork 失败");
        return;
    }

    if (pid == 0) {
        /* 子进程：执行简单任务 */
        printf("  [子进程] PID=%d 开始执行\n", (int)getpid());
#ifdef _WIN32
        Sleep(500);
#else
        usleep(500000);  /* 0.5 秒 */
#endif
        printf("  [子进程] 任务完成，退出\n");
        exit(42);  /* 退出码 42 */
    } else {
        /* 父进程：监控子进程 */
        printf("  [父进程] PID=%d 创建子进程 PID=%d\n", (int)getpid(), (int)pid);

        int status;
        pid_t waited = waitpid(pid, &status, 0);

        if (waited == pid) {
            if (WIFEXITED(status)) {
                printf("  [监控] 子进程正常退出，退出码=%d\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("  [监控] 子进程被信号终止，信号=%d\n", WTERMSIG(status));
            }
        }
    }
}
#else
/** Windows 进程创建演示 */
static void demo_process_monitor(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 3: 进程监控（Windows 模式）\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("\n  Windows 进程创建示例:\n");
    printf("    STARTUPINFOA si = {0};\n");
    printf("    PROCESS_INFORMATION pi = {0};\n");
    printf("    CreateProcess(NULL, \"notepad.exe\", ...);\n");
    printf("    WaitForSingleObject(pi.hProcess, INFINITE);\n");
    printf("    GetExitCodeProcess(pi.hProcess, &exit_code);\n");
}
#endif

/* ══════════════════════════════════════════════════════════════════════
 * 4. 常见守护进程示例
 * ══════════════════════════════════════════════════════════════════════ */

/** 演示常见守护进程 */
static void demo_common_daemons(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 4: 常见守护进程\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("\n  系统守护进程:\n");
    printf("    • cron     - 定时任务调度\n");
    printf("    • sshd     - SSH 远程登录\n");
    printf("    • syslogd  - 系统日志\n");
    printf("    • systemd  - 服务管理器\n");

    printf("\n  应用守护进程:\n");
    printf("    • mysqld   - MySQL 数据库\n");
    printf("    • postgres - PostgreSQL 数据库\n");
    printf("    • nginx    - Web 服务器\n");
    printf("    • redis-server - 内存数据库\n");

    printf("\n  管理命令:\n");
#ifndef _WIN32
    printf("    ps aux | grep <name>     # 查看进程\n");
    printf("    kill -TERM <pid>         # 优雅终止\n");
    printf("    kill -HUP <pid>         # 重载配置\n");
    printf("    systemctl status <name>  # 查看状态\n");
#else
    printf("    tasklist | findstr <name>  # 查看进程\n");
    printf("    taskkill /PID <pid>        # 终止进程\n");
    printf("    sc query <name>            # 查看服务\n");
#endif
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                  守护进程 (Daemon) 演示                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

#ifdef _WIN32
    printf("\n  [平台] Windows 环境（使用等效实现）\n");
#else
    printf("\n  [平台] Linux/Unix 环境\n");
#endif

    demo_daemon_concepts();
    demo_signal_handling();
    demo_process_monitor();
    demo_common_daemons();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  守护进程要点总结:\n");
    printf("    • 后台运行，脱离控制终端\n");
    printf("    • 长生命周期，独立于登录会话\n");
    printf("    • 优雅处理信号（TERM/HUP/USR1）\n");
    printf("    • 使用 PID 文件或服务管理器管理\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
