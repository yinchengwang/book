/**
 * @file main.c
 * @brief Linux 文件系统缓存学习演示
 *
 * 演示内容：
 *   - /proc/sys/vm/drop_caches 缓存回收（echo 1/2/3）
 *   - /proc/meminfo 读取 Cached/Dirty/Writeback 状态
 *   - 页缓存 LRU 回收策略
 *   - 缓存压力阈值与 sync + drop_caches 演示
 *
 * 编译：gcc -std=c11 -Wall -o fs_cache main.c
 * 运行：./fs_cache
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

/* sync() 在 Windows 上不可用 */
#ifdef _WIN32
#define sync() ((void)0)
#endif

/* ============================================================
 * 获取当前时间（微秒）
 * ============================================================ */
static unsigned long get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

/* ============================================================
 * 读取 /proc/meminfo 中的缓存状态
 * ============================================================ */
static void print_meminfo_cache(void) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        printf("[fs_cache] 无法打开 /proc/meminfo（可能非 Linux 环境）\n");
        return;
    }

    char line[256];
    unsigned long cached = 0, dirty = 0, writeback = 0, memfree = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "Cached: %lu kB", &cached) == 1) continue;
        if (sscanf(line, "Dirty: %lu kB", &dirty) == 1) continue;
        if (sscanf(line, "Writeback: %lu kB", &writeback) == 1) continue;
        if (sscanf(line, "MemFree: %lu kB", &memfree) == 1) continue;
    }
    fclose(fp);

    printf("[fs_cache]   MemFree:  %lu MB\n", memfree / 1024);
    printf("[fs_cache]   Cached:   %lu MB\n", cached / 1024);
    printf("[fs_cache]   Dirty:    %lu MB\n", dirty / 1024);
    printf("[fs_cache]   Writeback:%lu MB\n", writeback / 1024);
}

/* ============================================================
 * 向 /proc/sys/vm/drop_caches 写入值（需要 root）
 * ============================================================ */
static int drop_caches(int level) {
    int fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
    if (fd < 0) {
        printf("[fs_cache] drop_caches 需要 root 权限，跳过实际操作\n");
        return -1;
    }
    char buf[4];
    int len = snprintf(buf, sizeof(buf), "%d\n", level);
    if (write(fd, buf, len) < 0) {
        printf("[fs_cache] 写入 drop_caches 失败\n");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/* ============================================================
 * Demo 1: drop_caches 三级缓存回收
 * ============================================================ */
static void demo_drop_caches(void) {
    printf("[fs_cache] ====== Demo 1: drop_caches 三级缓存回收 ======\n\n");

    printf("[fs_cache] /proc/sys/vm/drop_caches 参数说明:\n");
    printf("[fs_cache]   1 - 释放页缓存（page cache）\n");
    printf("[fs_cache]   2 - 释放可回收的 slab 对象（dentries/inodes）\n");
    printf("[fs_cache]   3 - 释放页缓存 + slab 对象\n\n");

    printf("[fs_cache] 操作前状态:\n");
    print_meminfo_cache();

    printf("\n[fs_cache] 执行: echo 1 > /proc/sys/vm/drop_caches\n");
    printf("[fs_cache]   - 仅释放 page cache，不影响 dirty 页\n\n");

    printf("[fs_cache] 提示: 实际执行需要 root 权限，此处仅演示概念\n");
    printf("[fs_cache]   sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'\n");
}

/* ============================================================
 * Demo 2: 页缓存 LRU 回收策略
 * ============================================================ */
static void demo_lru_reclaim(void) {
    printf("[fs_cache] ====== Demo 2: 页缓存 LRU 回收策略 ======\n\n");

    printf("[fs_cache] Linux 内核使用双 LRU 链表管理页缓存:\n\n");

    printf("[fs_cache] 1. 活跃链表（Active List）\n");
    printf("[fs_cache]    - 存放最近被访问过的页面\n");
    printf("[fs_cache]    - 页面被再次访问时提升到此链表\n\n");

    printf("[fs_cache] 2. 非活跃链表（Inactive List）\n");
    printf("[fs_cache]    - 存放较久未被访问的页面\n");
    printf("[fs_cache]    - 内存压力时优先从此链表回收\n\n");

    printf("[fs_cache] 3. 回收流程:\n");
    printf("[fs_cache]    活跃链表(尾部) → 非活跃链表(头部)\n");
    printf("[fs_cache]    非活跃链表(尾部) → 释放或写回\n");
    printf("[fs_cache]    干净页直接释放，脏页先写回再释放\n\n");

    printf("[fs_cache] 4. 关键内核参数:\n");
    printf("[fs_cache]    /proc/sys/vm/swappiness\n");
    printf("[fs_cache]    /proc/sys/vm/vfs_cache_pressure\n");
    printf("[fs_cache]    /proc/sys/vm/dirty_ratio\n");
    printf("[fs_cache]    /proc/sys/vm/dirty_background_ratio\n");
}

/* ============================================================
 * Demo 3: 缓存压力阈值与 dirty 页回写
 * ============================================================ */
static void demo_cache_pressure(void) {
    printf("[fs_cache] ====== Demo 3: 缓存压力与 dirty 页回写 ======\n\n");

    /* 创建测试文件并写入数据，观察 dirty 页变化 */
    const char *test_file = "/tmp/fs_cache_test.dat";
    int fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[fs_cache] 创建测试文件失败");
        return;
    }

    /* 写入 50MB 数据，产生 dirty 页 */
    size_t write_size = 50 * 1024 * 1024;
    char *data = (char *)malloc(write_size);
    if (!data) { close(fd); return; }
    memset(data, 'A', write_size);

    printf("[fs_cache] 写入 50MB 数据前（dirty 页状态）:\n");
    print_meminfo_cache();

    unsigned long start = get_time_us();
    ssize_t written = write(fd, data, write_size);
    unsigned long elapsed = get_time_us() - start;

    printf("\n[fs_cache] 写入 50MB 数据后:\n");
    printf("[fs_cache]   实际写入: %zd 字节, 耗时: %.2f ms\n",
           written, elapsed / 1000.0);
    print_meminfo_cache();

    /* sync 强制刷盘 */
    printf("\n[fs_cache] 执行 sync 强制刷盘:\n");
    start = get_time_us();
    sync();
    elapsed = get_time_us() - start;
    printf("[fs_cache]   sync 耗时: %.2f ms\n", elapsed / 1000.0);
    print_meminfo_cache();

    free(data);
    close(fd);
    unlink(test_file);

    printf("\n[fs_cache] 缓存压力阈值说明:\n");
    printf("[fs_cache]   dirty_ratio:       脏页达到总内存百分比时，写入进程自己回写\n");
    printf("[fs_cache]   dirty_background_ratio: 脏页达到此百分比时，后台 pdflush 开始回写\n");
}

/* ============================================================
 * Demo 4: sync + drop_caches 完整缓存管理流程
 * ============================================================ */
static void demo_sync_drop(void) {
    printf("[fs_cache] ====== Demo 4: sync + drop_caches 完整流程 ======\n\n");

    printf("[fs_cache] 数据库/存储系统的安全缓存清理流程:\n\n");

    printf("[fs_cache] 步骤 1: 停止新写入（或锁表）\n");
    printf("[fs_cache] 步骤 2: 执行 sync — 将所有 dirty 页刷入磁盘\n");
    printf("[fs_cache]    sync() 系统调用:\n");
    printf("[fs_cache]      - 遍历所有文件系统的 dirty 页\n");
    printf("[fs_cache]      - 提交写请求到块设备\n");
    printf("[fs_cache]      - 等待所有写操作完成\n");
    printf("[fs_cache]      - 阻塞直到所有数据落盘\n\n");

    printf("[fs_cache] 步骤 3: 执行 drop_caches — 释放内存缓存\n");
    printf("[fs_cache]    echo 3 > /proc/sys/vm/drop_caches\n");
    printf("[fs_cache]      1: 仅页缓存  2: 仅 slab  3: 全部\n\n");

    printf("[fs_cache] 重要警告:\n");
    printf("[fs_cache]   - drop_caches 是非破坏性操作，不会丢数据\n");
    printf("[fs_cache]   - 但会清空热缓存，导致短期内 I/O 性能下降\n");
    printf("[fs_cache]   - 生产环境慎用，建议仅在测试/基准测试中使用\n\n");

    printf("[fs_cache] 典型应用场景:\n");
    printf("[fs_cache]   - 基准测试前清缓存，确保冷启动一致性\n");
    printf("[fs_cache]   - 排查 I/O 问题时确认是否缓存导致\n");
    printf("[fs_cache]   - 内存压力大时临时释放内存\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 文件系统缓存学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: drop_caches 三级回收 ---\n");
    demo_drop_caches();
    printf("\n");

    printf("--- Demo 2: LRU 回收策略 ---\n");
    demo_lru_reclaim();
    printf("\n");

    printf("--- Demo 3: 缓存压力与 dirty 页回写 ---\n");
    demo_cache_pressure();
    printf("\n");

    printf("--- Demo 4: sync + drop_caches 完整流程 ---\n");
    demo_sync_drop();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
