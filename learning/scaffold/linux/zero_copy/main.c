/**
 * @file main.c
 * @brief Linux 零拷贝技术演示
 *
 * 本文件演示：
 * 1. 传统 read/write 模式（4 次拷贝）
 * 2. mmap 文件映射（3 次拷贝）
 * 3. sendfile 零拷贝（2 次拷贝）
 * 4. 性能对比测量
 *
 * 学习目标：
 * - 理解数据在内核/用户空间之间的拷贝过程
 * - 掌握零拷贝技术的原理和使用场景
 * - 能够选择合适的 I/O 方式
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <time.h>

// ================================================================
// 常量
// ================================================================

#define FILE_SIZE     (10 * 1024 * 1024)  // 10 MB
#define BUFFER_SIZE   (64 * 1024)        // 64 KB
#define TEST_FILE     "/tmp/zero_copy_test.bin"
#define ITERATIONS    100

// 打印分隔符
void print_separator(const char *title) {
    printf("\n%s\n", "============================================================");
    printf("  %s\n", title);
    printf("%s\n", "============================================================");
}

// 获取当前时间（微秒）
static inline long get_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

// 创建测试文件
int create_test_file(void) {
    int fd = open(TEST_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    // 写入随机数据
    srand(time(NULL));
    char *buf = malloc(BUFFER_SIZE);
    if (!buf) {
        close(fd);
        return -1;
    }

    for (size_t remaining = FILE_SIZE; remaining > 0; remaining -= BUFFER_SIZE) {
        size_t chunk = remaining < BUFFER_SIZE ? remaining : BUFFER_SIZE;
        for (size_t i = 0; i < chunk; i++) {
            buf[i] = rand() & 0xFF;
        }
        write(fd, buf, chunk);
    }

    free(buf);
    close(fd);
    return 0;
}

// ================================================================
// 第一部分：传统 read/write 模式
// ================================================================

/**
 * @brief 传统 read/write 模式
 *
 * 数据流程（4 次拷贝）：
 * 1. 磁盘 -> 内核缓冲区（DMA）
 * 2. 内核缓冲区 -> 用户缓冲区（CPU）
 * 3. 用户缓冲区 -> socket 缓冲区（CPU）
 * 4. socket 缓冲区 -> 网卡（DMA）
 */
void demo_traditional(void) {
    print_separator("1. 传统 read/write 模式");

    printf("数据拷贝流程（4 次）：\n");
    printf("  磁盘 -> 内核缓冲区 (DMA)\n");
    printf("  内核缓冲区 -> 用户缓冲区 (CPU)\n");
    printf("  用户缓冲区 -> socket 缓冲区 (CPU)\n");
    printf("  socket 缓冲区 -> 网卡 (DMA)\n\n");

    int file_fd = open(TEST_FILE, O_RDONLY);
    if (file_fd < 0) {
        perror("open file");
        return;
    }

    // 使用 pipe 模拟 send（避免实际网络）
    int pipefd[2];
    pipe(pipefd);

    char *buf = malloc(BUFFER_SIZE);
    if (!buf) {
        close(file_fd);
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    long start = get_usec();
    ssize_t total = 0;

    // read -> write
    ssize_t n;
    while ((n = read(file_fd, buf, BUFFER_SIZE)) > 0) {
        write(pipefd[1], buf, n);  // 写入 pipe
        total += n;
    }

    long elapsed = get_usec() - start;

    printf("传输 %ld 字节，耗时 %.2f ms\n", (long)total, elapsed / 1000.0);
    printf("吞吐量: %.2f MB/s\n", (double)total / elapsed * 1000 / 1024 / 1024);

    free(buf);
    close(file_fd);
    close(pipefd[0]);
    close(pipefd[1]);
}

// ================================================================
// 第二部分：mmap 内存映射
// ================================================================

/**
 * @brief mmap 文件映射
 *
 * 数据流程（3 次拷贝）：
 * 1. 磁盘 -> 内核缓冲区（DMA）
 * 2. 内核缓冲区 -> 网卡（DMA）（直接映射，无需用户空间）
 *
 * 注意：mmap 减少了用户态和内核态之间的数据拷贝，
 * 但仍然是 3 次拷贝（没有完全消除）
 */
void demo_mmap(void) {
    print_separator("2. mmap 内存映射");

    printf("数据拷贝流程（3 次）：\n");
    printf("  磁盘 -> 内核缓冲区 (DMA)\n");
    printf("  内核缓冲区直接映射到用户空间\n");
    printf("  内核缓冲区 -> socket 缓冲区 (DMA)\n\n");

    int file_fd = open(TEST_FILE, O_RDONLY);
    if (file_fd < 0) {
        perror("open file");
        return;
    }

    struct stat st;
    fstat(file_fd, &st);

    // mmap 映射文件到内存
    char *mapped = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        close(file_fd);
        return;
    }

    // 使用 pipe 模拟
    int pipefd[2];
    pipe(pipefd);

    long start = get_usec();
    ssize_t total = 0;

    // mmap 后直接使用指针访问数据
    for (off_t offset = 0; offset < st.st_size; offset += BUFFER_SIZE) {
        size_t chunk = st.st_size - offset;
        if (chunk > BUFFER_SIZE) chunk = BUFFER_SIZE;

        // 写入 pipe（仍然需要拷贝，但避免了 read() 的额外拷贝）
        write(pipefd[1], mapped + offset, chunk);
        total += chunk;
    }

    long elapsed = get_usec() - start;

    printf("传输 %ld 字节，耗时 %.2f ms\n", (long)total, elapsed / 1000.0);
    printf("吞吐量: %.2f MB/s\n", (double)total / elapsed * 1000 / 1024 / 1024);

    munmap(mapped, st.st_size);
    close(file_fd);
    close(pipefd[0]);
    close(pipefd[1]);
}

// ================================================================
// 第三部分：sendfile 零拷贝
// ================================================================

/**
 * @brief sendfile 零拷贝
 *
 * 数据流程（2 次拷贝）：
 * 1. 磁盘 -> 内核缓冲区（DMA）
 * 2. 内核缓冲区 -> 网卡（DMA）
 *
 * sendfile 在内核空间内完成数据传递，无需进入用户态
 */
void demo_sendfile(void) {
    print_separator("3. sendfile 零拷贝");

    printf("数据拷贝流程（2 次）：\n");
    printf("  磁盘 -> 内核缓冲区 (DMA)\n");
    printf("  内核缓冲区 -> 网卡 (DMA)\n");
    printf("  【完全在内核空间完成，零用户态拷贝】\n\n");

    int file_fd = open(TEST_FILE, O_RDONLY);
    if (file_fd < 0) {
        perror("open file");
        return;
    }

    struct stat st;
    fstat(file_fd, &st);

    // 使用 pipe 模拟（Linux sendfile 需要 file -> socket）
    int pipefd[2];
    pipe(pipefd);

    long start = get_usec();
    ssize_t total = 0;

    // sendfile: file -> pipe (模拟 socket)
    off_t offset = 0;
    while (offset < st.st_size) {
        ssize_t sent = sendfile(pipefd[1], file_fd, &offset, st.st_size - offset);
        if (sent <= 0) break;
        total += sent;
    }

    long elapsed = get_usec() - start;

    printf("传输 %ld 字节，耗时 %.2f ms\n", (long)total, elapsed / 1000.0);
    printf("吞吐量: %.2f MB/s\n", (double)total / elapsed * 1000 / 1024 / 1024);

    close(file_fd);
    close(pipefd[0]);
    close(pipefd[1]);
}

// ================================================================
// 第四部分：性能对比
// ================================================================

void demo_compare(void) {
    print_separator("4. 性能对比");

    printf("%-20s | %-12s | %-12s | %s\n", "方法", "拷贝次数", "系统调用", "特点");
    printf("%-20s-+-%-12s-+-%-12s-+-%s\n", "--------------------", "------------", "------------", "----");
    printf("%-20s | %-12d | %-12s | %s\n", "read/write", 4, "read+write", "最简单");
    printf("%-20s | %-12d | %-12s | %s\n", "mmap", 3, "mmap+write", "减少一次拷贝");
    printf("%-20s | %-12d | %-12s | %s\n", "sendfile", 2, "sendfile", "真正零拷贝");

    printf("\n适用场景：\n");
    printf("- 文件下载/静态资源服务 → sendfile\n");
    printf("- 需要处理数据再发送 → mmap + 自定义处理\n");
    printf("- 小文件或简单场景 → read/write\n");

    printf("\n实际性能测试结果（运行后显示）：\n");
}

// ================================================================
// 主函数
// ================================================================

int main(int argc, char *argv[]) {
    printf("Linux 零拷贝技术演示\n");
    printf("====================\n");

    // 创建测试文件
    printf("创建测试文件 (%d MB)...\n", FILE_SIZE / (1024 * 1024));
    if (create_test_file() < 0) {
        fprintf(stderr, "创建测试文件失败\n");
        return 1;
    }
    printf("测试文件创建完成\n");

    if (argc > 1 && strcmp(argv[1], "compare") == 0) {
        demo_compare();
    } else {
        demo_compare();
        demo_traditional();
        demo_mmap();
        demo_sendfile();
    }

    // 清理
    unlink(TEST_FILE);

    print_separator("演示完成");
    printf("本演示覆盖了以下主题：\n");
    printf("1. 传统 read/write（4 次拷贝）\n");
    printf("2. mmap 内存映射（3 次拷贝）\n");
    printf("3. sendfile 零拷贝（2 次拷贝）\n");
    printf("4. 性能对比与适用场景\n");

    return 0;
}
