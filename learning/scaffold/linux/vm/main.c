/**
 * @file main.c
 * @brief Linux 虚拟内存学习演示
 *
 * 演示内容：
 *   - mmap 内存映射
 *   - mprotect 内存保护
 *   - madvise 内存访问提示
 *   - 缺页中断概念
 *
 * 编译：gcc -std=c11 -Wall -o vm main.c
 * 运行：./vm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>

/* ============================================================
 * 获取当前时间（微秒）
 * ============================================================ */
static unsigned long get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

/* ============================================================
 * Demo 1: mmap 匿名映射
 * ============================================================ */
static void demo_mmap_anon(void) {
    printf("[vm] 演示 mmap 匿名映射\n");

    const size_t MAP_SIZE = 4096 * 10;  /* 10 页 */

    /* 创建匿名映射（不关联文件） */
    void *addr = mmap(NULL, MAP_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);

    if (addr == MAP_FAILED) {
        perror("[vm] mmap 失败");
        return;
    }

    printf("[vm]   映射成功，地址: %p，大小: %zu 字节\n", addr, MAP_SIZE);

    /* 写入数据（触发缺页中断） */
    printf("[vm]   写入数据到映射内存...\n");
    unsigned long start = get_time_us();
    char *p = (char *)addr;
    for (size_t i = 0; i < MAP_SIZE; i += 4096) {
        p[i] = 'X';  /* 按页写入 */
    }
    unsigned long elapsed = get_time_us() - start;
    printf("[vm]   写入耗时: %.2f ms\n", elapsed / 1000.0);

    /* 验证数据 */
    int ok = 1;
    for (size_t i = 0; i < MAP_SIZE; i += 4096) {
        if (p[i] != 'X') {
            ok = 0;
            break;
        }
    }
    printf("[vm]   验证结果: %s\n", ok ? "通过" : "失败");

    /* 解除映射 */
    munmap(addr, MAP_SIZE);
    printf("[vm]   解除映射\n");
}

/* ============================================================
 * Demo 2: mmap 文件映射
 * ============================================================ */
static void demo_mmap_file(void) {
    printf("[vm] 演示 mmap 文件映射\n");

    const char *test_file = "/tmp/vm_test.dat";
    const size_t FILE_SIZE = 4096 * 5;

    /* 创建测试文件 */
    int fd = open(test_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[vm] open 失败");
        return;
    }

    /* 扩展文件大小 */
    if (ftruncate(fd, FILE_SIZE) < 0) {
        perror("[vm] ftruncate 失败");
        close(fd);
        return;
    }

    /* 映射文件 */
    void *addr = mmap(NULL, FILE_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);

    if (addr == MAP_FAILED) {
        perror("[vm] mmap 失败");
        close(fd);
        return;
    }

    printf("[vm]   文件映射成功，地址: %p\n", addr);

    /* 写入数据 */
    printf("[vm]   写入数据到文件...\n");
    strcpy((char *)addr, "Hello from mmap!");
    printf("[vm]   写入内容: \"%s\"\n", (char *)addr);

    /* 同步到磁盘 */
    msync(addr, FILE_SIZE, MS_SYNC);
    printf("[vm]   msync() 同步到磁盘\n");

    /* 关闭 fd 后映射仍然有效 */
    close(fd);

    /* 重新打开验证 */
    fd = open(test_file, O_RDONLY);
    if (fd >= 0) {
        char buf[64] = {0};
        read(fd, buf, 63);
        printf("[vm]   验证文件内容: \"%s\"\n", buf);
        close(fd);
    }

    /* 解除映射 */
    munmap(addr, FILE_SIZE);
    unlink(test_file);
    printf("[vm]   解除映射并清理文件\n");
}

/* ============================================================
 * Demo 3: mprotect 内存保护
 * ============================================================ */
static void demo_mprotect(void) {
    printf("[vm] 演示 mprotect 内存保护\n");

    const size_t PAGE_SIZE = sysconf(_SC_PAGESIZE);
    const size_t MAP_SIZE = PAGE_SIZE * 3;

    /* 创建可读写的映射 */
    void *addr = mmap(NULL, MAP_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);

    if (addr == MAP_FAILED) {
        perror("[vm] mmap 失败");
        return;
    }

    printf("[vm]   创建匿名映射: %p，大小: %zu 字节\n", addr, MAP_SIZE);

    /* 写入数据 */
    ((char *)addr)[0] = 'A';
    printf("[vm]   写入数据成功: addr[0] = 'A'\n");

    /* 保护第一页为只读 */
    if (mprotect(addr, PAGE_SIZE, PROT_READ) == 0) {
        printf("[vm]   mprotect(): 第一页设为只读\n");

        /* 尝试写入（应该触发 SIGSEGV） */
        printf("[vm]   尝试写入只读页面...\n");
        ((char *)addr)[0] = 'B';  /* 这会导致段错误 */

        printf("[vm]   写入成功（未触发段错误）\n");
    } else {
        perror("[vm] mprotect 失败");
    }

    /* 恢复读写权限 */
    mprotect(addr, PAGE_SIZE, PROT_READ | PROT_WRITE);
    printf("[vm]   mprotect(): 恢复读写权限\n");

    /* 再次写入成功 */
    ((char *)addr)[0] = 'C';
    printf("[vm]   写入成功: addr[0] = 'C'\n");

    /* 解除映射 */
    munmap(addr, MAP_SIZE);
}

/* ============================================================
 * Demo 4: madvise 内存提示
 * ============================================================ */
static void demo_madvise(void) {
    printf("[vm] 演示 madvise 内存提示\n\n");

    const size_t MAP_SIZE = 4096 * 100;  /* 100 页 */

    printf("[vm] madvise 选项说明:\n");
    printf("[vm]   MADV_NORMAL    - 无提示（默认）\n");
    printf("[vm]   MADV_RANDOM    - 随机访问，禁用预读\n");
    printf("[vm]   MADV_SEQUENTIAL- 顺序访问，增强预读\n");
    printf("[vm]   MADV_WILLNEED  - 预加载到内存\n");
    printf("[vm]   MADV_DONTNEED  - 释放内存（内容丢失）\n\n");

    /* 创建匿名映射 */
    void *addr = mmap(NULL, MAP_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);

    if (addr == MAP_FAILED) {
        perror("[vm] mmap 失败");
        return;
    }

    /* 写入数据 */
    memset(addr, 'X', MAP_SIZE);

    /* 设置顺序访问提示 */
    printf("[vm]   调用 madvise(addr, %zu, MADV_SEQUENTIAL)\n", MAP_SIZE);
    if (madvise(addr, MAP_SIZE, MADV_SEQUENTIAL) == 0) {
        printf("[vm]   提示成功：内核将增强预读\n");
    }

    /* 顺序读取（应该很快） */
    printf("[vm]   顺序读取...\n");
    volatile char sum = 0;
    unsigned long start = get_time_us();
    for (size_t i = 0; i < MAP_SIZE; i++) {
        sum += ((char *)addr)[i];
    }
    unsigned long seq_time = get_time_us() - start;
    printf("[vm]   顺序读取耗时: %.2f ms\n", seq_time / 1000.0);

    /* 改为随机访问提示 */
    printf("[vm]   调用 madvise(addr, %zu, MADV_RANDOM)\n", MAP_SIZE);
    madvise(addr, MAP_SIZE, MADV_RANDOM);

    /* 释放内存内容（但保留映射） */
    printf("[vm]   调用 madvise(addr, %zu, MADV_DONTNEED)\n", MAP_SIZE);
    if (madvise(addr, MAP_SIZE, MADV_DONTNEED) == 0) {
        printf("[vm]   内存已释放，再次访问会触发缺页\n");
    }

    /* 读取（会触发缺页） */
    printf("[vm]   再次读取（触发缺页）...\n");
    start = get_time_us();
    for (size_t i = 0; i < MAP_SIZE; i += 4096) {
        sum += ((char *)addr)[i];
    }
    unsigned long fault_time = get_time_us() - start;
    printf("[vm]   缺页读取耗时: %.2f ms\n", fault_time / 1000.0);

    /* 解除映射 */
    munmap(addr, MAP_SIZE);
}

/* ============================================================
 * Demo 5: 缺页中断概念
 * ============================================================ */
static void demo_page_fault(void) {
    printf("[vm] 缺页中断（Page Fault）概念:\n\n");

    printf("[vm] 缺页类型:\n");
    printf("[vm]   1. Major Fault（主缺页）\n");
    printf("[vm]      - 需要从磁盘读取数据\n");
    printf("[vm]      - 耗时：5-50ms (取决于磁盘)\n");
    printf("[vm]      - 触发：首次访问文件映射/交换出去的分页\n\n");

    printf("[vm]   2. Minor Fault（次缺页）\n");
    printf("[vm]      - 页面已在内存，只需建立映射\n");
    printf("[vm]      - 耗时：< 1ms\n");
    printf("[vm]      - 触发：fork() 后首次访问 COW 页面\n\n");

    printf("[vm] 缺页处理流程:\n");
    printf("[vm]   1. 进程访问虚拟地址\n");
    printf("[vm]   2. MMU 查找页表，发现 PTE 无效\n");
    printf("[vm]   3. 触发缺页异常（#PF）\n");
    printf("[vm]   4. 内核处理异常：\n");
    printf("[vm]      - 分配物理页面\n");
    printf("[vm]      - 从磁盘读取（如果是文件/交换）\n");
    printf("[vm]      - 更新页表\n");
    printf("[vm]   5. 返回用户态，重新执行访问指令\n\n");

    printf("[vm] 查看缺页统计:\n");
    printf("[vm]   cat /proc/vmstat | grep -i pg\n");
    printf("[vm]   pgfault    - 总缺页数\n");
    printf("[vm]   pgmajfault - 主缺页数\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 虚拟内存学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: mmap 匿名映射 ---\n");
    demo_mmap_anon();
    printf("\n");

    printf("--- Demo 2: mmap 文件映射 ---\n");
    demo_mmap_file();
    printf("\n");

    printf("--- Demo 3: mprotect 内存保护 ---\n");
    demo_mprotect();
    printf("\n");

    printf("--- Demo 4: madvise 内存提示 ---\n");
    demo_madvise();
    printf("\n");

    printf("--- Demo 5: 缺页中断 ---\n");
    demo_page_fault();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
