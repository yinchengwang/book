/**
 * @file main.c
 * @brief cgroup v2 资源控制演示
 *
 * 演示 cgroup2 层级结构、资源限制机制
 * 展示 cgroup 在容器和数据库隔离中的应用
 *
 * 编译：gcc -std=c11 -o cgroup2 main.c
 * 运行：./cgroup2
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

/* 检测 cgroup 版本 */
static int detect_cgroup_version(void) {
    /* 检查 /sys/fs/cgroup 下的文件 */
    FILE *fp = fopen("/sys/fs/cgroup/cgroup.controllers", "r");
    if (fp) {
        fclose(fp);
        return 2;  /* cgroup v2 */
    }

    fp = fopen("/sys/fs/cgroup/cpu/cgroup.procs", "r");
    if (fp) {
        fclose(fp);
        return 1;  /* cgroup v1 */
    }

    return 0;  /* 未知或不支持 */
}

/* 读取当前进程的 cgroup 信息 */
static void show_process_cgroup(void) {
    printf("\n=== 当前进程 cgroup 信息 ===\n");
    FILE *fp = fopen("/proc/self/cgroup", "r");
    if (!fp) {
        perror("fopen /proc/self/cgroup");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("  %s", line);
    }
    fclose(fp);
}

/* 读取当前进程的 cgroup v2 控制器 */
static void show_cgroup2_controllers(void) {
    printf("\n=== cgroup v2 可用控制器 ===\n");
    FILE *fp = fopen("/sys/fs/cgroup/cgroup.controllers", "r");
    if (!fp) {
        printf("cgroup v2 不可用\n");
        return;
    }

    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        /* 每个词就是一个控制器 */
        char *tok = strtok(line, " \t\n");
        while (tok) {
            printf("  - %s\n", tok);
            tok = strtok(NULL, " \t\n");
        }
    }
    fclose(fp);
}

/* 读取 cgroup 内存统计 */
static void show_cgroup_memory(void) {
    printf("\n=== cgroup 内存统计 ===\n");

    const char *files[] = {
        "/sys/fs/cgroup/memory.current",
        "/sys/fs/cgroup/memory.max",
        "/sys/fs/cgroup/memory.stat",
        NULL
    };

    for (int i = 0; files[i]; i++) {
        FILE *fp = fopen(files[i], "r");
        if (!fp) continue;

        printf("\n--- %s ---\n",
               strrchr(files[i], '/') ? strrchr(files[i], '/') + 1 : files[i]);

        char line[256];
        int count = 0;
        while (fgets(line, sizeof(line), fp) && count++ < 5) {
            printf("  %s", line);
        }
        if (count >= 5) printf("  ... (截断)\n");
        fclose(fp);
    }
}

/* 模拟数据库处理内存请求 */
static void sim_db_memory_work(size_t mb) {
    printf("分配 %zu MB 内存...\n", mb);
    size_t count = (mb * 1024 * 1024) / sizeof(int);
    int *buf = calloc(count, sizeof(int));
    if (!buf) {
        printf("[受限] 内存分配失败，可能被 cgroup 限制\n");
        return;
    }

    /* 使用内存 */
    for (size_t i = 0; i < count; i += 1024) {
        buf[i] = (int)i;
    }

    printf("[成功] 分配并初始化 %zu MB\n", mb);
    free(buf);
}

int main(void) {
    printf("===========================================\n");
    printf("  cgroup v2 资源控制演示\n");
    printf("===========================================\n");

    /* 1. cgroup 版本检测 */
    printf("\n=== 1. cgroup 版本 ===\n");
    int version = detect_cgroup_version();
    printf("检测到 cgroup v%d\n", version ? version : -1);

    /* 2. 当前进程 cgroup */
    show_process_cgroup();

    /* 3. cgroup v2 概念 */
    printf("\n\n=== 2. cgroup v2 核心概念 ===\n");
    printf(
        "cgroup v2 的单层级模型:\n\n"
        "  根 (/)                               \n"
        "  ├── system.slice/                    \n"
        "  │   ├── sshd.service                \n"
        "  │   └── nginx.service               \n"
        "  └── user.slice/                      \n"
        "      └── user-1000.slice/            \n"
        "          └── db-server.scope         \n\n"
        "关键改进（相比 v1）:\n"
        "  - 单层级（不再有子系统的独立层级）\n"
        "  - 无内务线程（所有 cgroup 成员都是进程）\n"
        "  - 统一内存控制器\n"
        "  - 压力计数（PSI）集成\n"
    );

    /* 4. 控制器 */
    printf("\n=== 3. 资源控制器 ===\n");
    show_cgroup2_controllers();

    /* 5. cgroup 内存限制演示 */
    printf("\n\n=== 4. cgroup 内存限制 ===\n");
    show_cgroup_memory();

    /* 6. 受限内存分配测试 */
    printf("\n\n=== 5. 受限内存分配测试 ===\n");
    printf("[说明] 如果没有 cgroup 限制，以下分配都应该成功\n");
    printf("       在 cgroup 限制下可能触发 OOM\n\n");

    sim_db_memory_work(50);
    sim_db_memory_work(10);
    sim_db_memory_work(10);

    /* 7. cgroup 命令速查 */
    printf("\n\n=== 6. cgroup 管理命令 ===\n");
    printf(
        "\n# 查看进程的 cgroup\n"
        "cat /proc/<pid>/cgroup\n\n"
        "# 查看内存使用\n"
        "cat /sys/fs/cgroup/memory.current\n\n"
        "# 设置内存限制\n"
        "echo 512M > /sys/fs/cgroup/<group>/memory.max\n\n"
        "# 查看 CPU 使用\n"
        "cat /sys/fs/cgroup/cpu.stat\n\n"
        "# 查看所有 cgroup 层级 (systemd)\n"
        "systemd-cgls\n\n"
        "# 查看 cgroup 资源统计\n"
        "systemd-cgtop\n"
    );

    /* 8. 数据库与 cgroup */
    printf("\n\n=== 7. 数据库中的 cgroup 应用 ===\n");
    printf(
        "  1. Docker 容器隔离: 每个 MySQL/PostgreSQL 实例独立 cgroup\n"
        "  2. Kubernetes QoS: Guaranteed/Burstable/BestEffort 映射到 cgroup\n"
        "  3. 多租户: 不同库分配到不同 cgroup，资源隔离\n"
        "  4. shared_buffers: 数据库内存限制需与 cgroup 对齐\n"
        "  5. OOM 保护: cgroup 防止单个数据库耗尽宿主机内存\n"
    );

    printf("\n===========================================\n");
    printf("  cgroup v2 演示完成\n");
    printf("===========================================\n");

    return 0;
}
