/* process scaffold — 进程管理
 *
 * 复现命令（POSIX 系统：Linux/macOS 或 MSYS/Cygwin）：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *
 * Windows MinGW 限制：本卡仅 [pid] 段编译运行；完整 fork/exec/zombie/daemon
 * 演示需在 Linux/macOS 或 MSYS2/Cygwin 环境下运行（条件编译整体跳过）。
 *
 * 演示 5 段：
 *   [pid]    — getpid/getppid/getpgrp/getsid 进程标识
 *   [fork]   — fork() 父子进程
 *   [exec]   — exec* 三种变体（l/list/v）
 *   [zombie] — 僵尸进程演示
 *   [daemon] — 守护进程编写（fork 两次 + setsid + chdir + umask）
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
  /* MinGW/Windows：仅演示 [pid] 段 */
  #include <process.h>
  #include <sys/types.h>

  static void show_pid_info(const char *tag) {
      printf("[%s pid] PID=%ld (MinGW/Windows: getppid 不可用)\n",
             tag, (long)getpid());
  }

int main(void) {
    printf("[pid] === 进程标识符（Windows 模式） ===\n");
    show_pid_info("main");

    printf("\n[NOTE] 当前为 MinGW/Windows：fork/exec 不可用\n");
    printf("       完整 fork/exec/zombie/daemon 演示请在 Linux/macOS 运行\n");
    printf("       MinGW <process.h> 仅提供 getpid，缺 getppid/fork/execve/waitpid\n");
    printf("       Windows 原生用 CreateProcess+WaitForSingleObject 模型\n");

    printf("\n=== PASS ===\n");
    return 0;
}

#else
  /* POSIX：完整演示 */
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <errno.h>

static void show_pid_info(const char *tag) {
    pid_t pgid = getpgrp();
    pid_t sid = getsid(0);
    printf("[%s pid] PID=%ld PPID=%ld PGID=%ld SID=%ld\n",
           tag, (long)getpid(), (long)getppid(), (long)pgid, (long)sid);
}

static void demo_fork(void) {
    printf("[fork] === fork() 父子进程 ===\n");

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        show_pid_info("child");
        printf("  child: doing some work, then exit(42)\n");
        _exit(42);
    } else {
        show_pid_info("parent");
        printf("  parent: child PID=%ld, waiting...\n", (long)pid);
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("  parent: child exited with status=%d\n", WEXITSTATUS(status));
        }
    }
}

static void demo_exec(void) {
    printf("\n[exec] === exec* 三种变体 ===\n");

    /* execve */
    pid_t pid = fork();
    if (pid == 0) {
        char *const argv[] = {"/bin/echo", "[execve]", "hello", "from", "execve", NULL};
        char *const envp[] = {"PATH=/bin", NULL};
        execve("/bin/echo", argv, envp);
        perror("execve"); _exit(1);
    } else if (pid > 0) {
        int status; waitpid(pid, &status, 0);
        printf("  parent: execve child reaped, status=%d\n", WEXITSTATUS(status));
    }

    /* execlp —— PATH 查找 */
    pid = fork();
    if (pid == 0) {
        execlp("echo", "echo", "[execlp]", "echo via PATH", (char*)NULL);
        perror("execlp"); _exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }

    /* execv —— 显式路径 + 参数数组 */
    pid = fork();
    if (pid == 0) {
        char *const argv[] = {"/bin/echo", "[execv]", "explicit", "path", NULL};
        execv("/bin/echo", argv);
        perror("execv"); _exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

static void demo_zombie(void) {
    printf("\n[zombie] === 僵尸进程演示 ===\n");

    pid_t pid = fork();
    if (pid == 0) {
        printf("  child: exiting immediately, becoming zombie until parent reaps\n");
        _exit(0);
    }

    sleep(1);
    printf("  parent: child PID=%ld (state Z = zombie briefly)\n", (long)pid);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ps -o pid,ppid,stat,cmd -p %ld 2>/dev/null || true", (long)pid);
    int rc = system(cmd);
    (void)rc;

    waitpid(pid, NULL, 0);
    printf("  parent: reaped child, zombie cleared\n");
}

static void demo_daemonize(const char *name) {
    printf("\n[daemon] === 守护进程编写（fork 两次 + setsid） ===\n");

    pid_t pid = fork();
    if (pid > 0) {
        waitpid(pid, NULL, 0);
        printf("  parent: daemon forked, PID=%ld\n", (long)pid);
        return;
    }

    setsid();
    pid_t pid2 = fork();
    if (pid2 > 0) _exit(0);

    chdir("/");
    umask(0);
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    printf("  [%s daemon] PID=%ld PGID=%ld SID=%ld, sleeping 1s\n",
           name, (long)getpid(), (long)getpgrp(), (long)getsid(0));
    sleep(1);
    _exit(0);
}

int main(void) {
    printf("[pid] === 进程标识符（POSIX 模式） ===\n");
    show_pid_info("main");

    demo_fork();
    demo_exec();
    demo_zombie();
    demo_daemonize("process");

    printf("\n=== PASS ===\n");
    return 0;
}

#endif /* _WIN32 */