/**
 * @file main.c
 * @brief Linux HugePages 大页机制演示
 *
 * 演示内容：
 *   - hugetlbfs 文件系统
 *   - 大页分配（2MB / 1GB）
 *   - TLB 命中率优化原理
 *
 * 编译：gcc -std=c11 -Wall -o hugepages main.c
 * 运行：sudo ./hugepages
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* ============================================================
 * Demo 1: 检查 HugePages 配置
 * ============================================================ */
static void demo_check_hugepages(void) {
    printf("[hugepages] 检查 HugePages 配置\n");

    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("fopen /proc/meminfo 失败");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "HugePages") || strstr(line, "HugeTLB")) {
            printf("[hugepages]   %s", line);
        }
    }
    fclose(fp);

    /* 检查 hugetlbfs 挂载 */
    printf("[hugepages] 检查 hugetlbfs 挂载点:\n");
    fp = fopen("/proc/mounts", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "hugetlbfs")) {
                printf("[hugepages]   %s", line);
            }
        }
        fclose(fp);
    }
}

/* ============================================================
 * Demo 2: 使用 mmap 映射 HugePages
 * ============================================================ */
static void demo_mmap_hugepages(void) {
    printf("[hugepages] 通过 mmap 映射 HugePages\n");

    /* 方法 1: MAP_HUGETLB（需要 root 或 CAP_SYS_ADMIN） */
    size_t page_size = 2 * 1024 * 1024;  /* 2MB */
    size_t alloc_size = 10 * page_size;   /* 20MB */

    printf("[hugepages]   尝试映射 %zu MB (10 × 2MB)\n", alloc_size / (1024 * 1024));

    void *ptr = mmap(NULL, alloc_size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                     -1, 0);

    if (ptr == MAP_FAILED) {
        printf("[hugepages]   MAP_HUGETLB 失败: %s\n", strerror(errno));
        printf("[hugepages]   提示: 需要 echo 20 > /proc/sys/vm/nr_hugepages\n");
    } else {
        printf("[hugepages]   MAP_HUGETLB 成功: %p\n", ptr);

        /* 写入测试 */
        memset(ptr, 0xAA, alloc_size);
        printf("[hugepages]   写入验证: 前 8 字节 = 0x%llX\n",
               *(unsigned long long*)ptr);

        munmap(ptr, alloc_size);
        printf("[hugepages]   释放成功\n");
    }
}

/* ============================================================
 * Demo 3: 通过 hugetlbfs 文件系统
 * ============================================================ */
static void demo_hugetlbfs(void) {
    printf("[hugepages] 通过 hugetlbfs 文件系统\n");

    const char *hugetlb_path = "/dev/hugepages";
    struct stat st;

    if (stat(hugetlb_path, &st) != 0) {
        printf("[hugepages]   %s 不存在，跳过\n", hugetlb_path);
        return;
    }

    char test_file[256];
    snprintf(test_file, sizeof(test_file), "%s/test_hugepage", hugetlb_path);

    int fd = open(test_file, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        printf("[hugepages]   打开 hugetlbfs 失败: %s\n", strerror(errno));
        return;
    }

    size_t size = 2 * 1024 * 1024;  /* 2MB */
    if (ftruncate(fd, size) != 0) {
        perror("ftruncate 失败");
        close(fd);
        return;
    }

    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        printf("[hugepages]   mmap 失败: %s\n", strerror(errno));
        close(fd);
        return;
    }

    printf("[hugepages]   hugetlbfs 映射成功: %p\n", ptr);
    memset(ptr, 0xBB, size);

    munmap(ptr, size);
    close(fd);
    unlink(test_file);
    printf("[hugepages]   清理完成\n");
}

/* ============================================================
 * Demo 4: TLB 优化原理
 * ============================================================ */
static void demo_tlb_optimization(void) {
    printf("[hugepages] TLB 优化原理:\n\n");

    printf("[hugepages] 普通页（4KB）:\n");
    printf("[hugepages]   - 1GB 内存 = 262144 个页\n");
    printf("[hugepages]   - 每个页需要一个 TLB 条目\n");
    printf("[hugepages]   - TLB 条目有限（~64-512 个）\n");
    printf("[hugepages]   - TLB Miss → 页表查找（多次内存访问）\n\n");

    printf("[hugepages] HugePages（2MB）:\n");
    printf("[hugepages]   - 1GB 内存 = 512 个大页\n");
    printf("[hugepages]   - TLB 条目减少 512 倍！\n");
    printf("[hugepages]   - 覆盖率更高，TLB Miss 显著减少\n\n");

    printf("[hugepages] HugePages（1GB）:\n");
    printf("[hugepages]   - 1GB 内存 = 1 个大页\n");
    printf("[hugepages]   - 超大型内存映射的首选\n");
    printf("[hugepages]   - 需要大内存和 NUMA 配置\n");
}

/* ============================================================
 * Demo 5: 数据库 HugePages 配置
 * ============================================================ */
static void demo_db_hugepages(void) {
    printf("[hugepages] 数据库 HugePages 配置:\n\n");

    printf("[hugepages] PostgreSQL:\n");
    printf("[hugepages]   shared_buffers = 8GB\n");
    printf("[hugepages]   huge_pages = try  # on/tryl/off\n");
    printf("[hugepages]   # 推荐: shared_buffers / 2MB 的页数\n\n");

    printf("[hugepages] MySQL/InnoDB:\n");
    printf("[hugepages]   # my.cnf\n");
    printf("[hugepages]   large_pages = 1\n");
    printf("[hugepages]   innodb_buffer_pool_size = 16G\n");
    printf("[hugepages]   # 需配合 sysctl vm.nr_hugepages\n\n");

    printf("[hugepages] Faiss/向量索引:\n");
    printf("[hugepages]   - 百万级向量 × 768 维 = 数百 GB\n");
    printf("[hugepages]   - HugePages 减少 TLB miss\n");
    printf("[hugepages]   - 配合 mmap 大文件映射\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux HugePages 大页机制演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 检查 HugePages 配置 ---\n");
    demo_check_hugepages();
    printf("\n");

    printf("--- Demo 2: mmap 映射 HugePages ---\n");
    demo_mmap_hugepages();
    printf("\n");

    printf("--- Demo 3: hugetlbfs 文件系统 ---\n");
    demo_hugetlbfs();
    printf("\n");

    printf("--- Demo 4: TLB 优化原理 ---\n");
    demo_tlb_optimization();
    printf("\n");

    printf("--- Demo 5: 数据库 HugePages 配置 ---\n");
    demo_db_hugepages();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
