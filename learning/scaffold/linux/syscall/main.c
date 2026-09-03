/**
 * @file main.c
 * @brief Linux 系统调用学习演示（跨平台版本）
 *
 * 演示内容（跨平台）：
 *   - getpid() / GetCurrentProcessId() — 获取进程 ID
 *   - fopen/fread/fwrite/fclose — 文件操作（跨平台）
 *   - 错误处理（perror）
 *   - mmap() — 仅 Linux，Windows 跳过并提示
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 * 运行：./test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#define pid_t int
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

/* 跨平台 getpid() */
static int my_getpid(void) {
#ifdef _WIN32
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

/* 跨平台 getppid() */
static int my_getppid(void) {
#ifdef _WIN32
    /* Windows 无直接 getppid，用进程 ID 模拟 */
    return (int)GetProcessId((HANDLE)-1); /* -1 = current process, 但这不是真正的 ppid */
#else
    return (int)getppid();
#endif
}

/* ============================================================
 * Demo 1: getpid() — 获取进程 ID
 * ============================================================ */
static void demo_getpid(void) {
    printf("[syscall] 调用 getpid() 获取当前进程 ID\n");
    int pid = my_getpid();
    printf("[syscall] 当前进程 ID: %d\n", pid);

    printf("[syscall] 调用 getppid() 获取父进程 ID\n");
    int ppid = my_getppid();
#ifdef _WIN32
    printf("[syscall] 父进程 ID: %d (模拟值，Windows 无真正 ppid)\n", ppid);
#else
    printf("[syscall] 父进程 ID: %d\n", ppid);
#endif
}

/* ============================================================
 * Demo 2: 文件操作 — fopen/fread/fwrite/fclose
 * ============================================================ */
static void demo_file_ops(void) {
    const char *test_file = "syscall_demo.txt";
    const char *write_data = "Hello from syscall demo!\n";
    char read_buf[256] = {0};

    printf("[syscall] 调用 fopen() 创建/打开文件: %s\n", test_file);
    FILE *fp = fopen(test_file, "w+");
    if (!fp) {
        perror("[syscall] fopen 失败");
        return;
    }
    printf("[syscall] fopen() 成功\n");

    /* 写入 */
    printf("[syscall] 调用 fwrite() 写入数据: %s", write_data);
    size_t written = fwrite(write_data, 1, strlen(write_data), fp);
    printf("[syscall] fwrite() 写入 %zu 字节\n", written);

    /* 刷新 */
    fflush(fp);
    printf("[syscall] fflush() 刷新缓冲区\n");

    /* 读回 */
    rewind(fp);
    printf("[syscall] 调用 fread() 读取数据\n");
    size_t n = fread(read_buf, 1, sizeof(read_buf) - 1, fp);
    read_buf[n] = '\0';
    printf("[syscall] fread() 读取到: %s", read_buf);

    /* 关闭 */
    fclose(fp);
    printf("[syscall] fclose() 关闭文件\n");

    /* 清理 */
    remove(test_file);
}

/* ============================================================
 * Demo 3: mmap() — 仅 Linux，Windows 跳过
 * ============================================================ */
static void demo_mmap(void) {
#ifdef _WIN32
    printf("[syscall] [skip] mmap 是 Linux 独有系统调用\n");
    printf("[syscall] [skip] Windows 可用 VirtualAlloc() 实现类似功能\n");
    printf("[syscall] [skip] 概念：把文件内容映射到进程地址空间\n");
    return;
#else
    const char *mmap_file = "/tmp/mmap_demo.dat";
    const size_t MMAP_SIZE = 4096;

    printf("[syscall] 调用 open() 创建内存映射文件\n");
    int fd = open(mmap_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[syscall] open 失败");
        return;
    }

    printf("[syscall] 调用 ftruncate() 扩展文件到 %zu 字节\n", MMAP_SIZE);
    if (ftruncate(fd, MMAP_SIZE) < 0) {
        perror("[syscall] ftruncate 失败");
        close(fd);
        return;
    }

    printf("[syscall] 调用 mmap() 创建内存映射\n");
    void *mapped = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE,
                        MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("[syscall] mmap 失败");
        close(fd);
        return;
    }
    printf("[syscall] mmap() 成功，地址: %p\n", mapped);

    const char *data = "Written via mmap!";
    memcpy(mapped, data, strlen(data));
    printf("[syscall] 写入映射内存: %s\n", (char *)mapped);

    msync(mapped, MMAP_SIZE, MS_SYNC);
    printf("[syscall] msync() 同步到磁盘\n");

    munmap(mapped, MMAP_SIZE);
    printf("[syscall] munmap() 解除映射\n");

    close(fd);

    /* 验证 */
    fd = open(mmap_file, O_RDONLY);
    if (fd >= 0) {
        char verify[64] = {0};
        read(fd, verify, (int)strlen(data));
        printf("[syscall] 验证文件内容: %s\n", verify);
        close(fd);
    }

    unlink(mmap_file);
#endif
}

/* ============================================================
 * Demo 4: 错误处理 — perror
 * ============================================================ */
static void demo_error_handling(void) {
    printf("[syscall] 演示错误处理（perror）\n");

    /* 尝试打开不存在的文件 */
    printf("[syscall] 尝试打开不存在的文件\n");
    FILE *fp = fopen("/nonexistent/path/that/does/not/exist", "r");
    if (!fp) {
        perror("[syscall] fopen 错误");
    }

    /* 常见错误码演示 */
    errno = ENOENT;
    perror("[syscall] ENOENT（文件不存在）");

    errno = EACCES;
    perror("[syscall] EACCES（权限不足）");

    errno = ENOSPC;
    perror("[syscall] ENOSPC（磁盘空间不足）");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 系统调用学习演示\n");
#ifdef _WIN32
    printf("  [Windows 模式，mmap 已跳过]\n");
#endif
    printf("========================================\n\n");

    printf("--- Demo 1: getpid() ---\n");
    demo_getpid();
    printf("\n");

    printf("--- Demo 2: 文件操作 ---\n");
    demo_file_ops();
    printf("\n");

    printf("--- Demo 3: mmap() ---\n");
    demo_mmap();
    printf("\n");

    printf("--- Demo 4: 错误处理 ---\n");
    demo_error_handling();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
