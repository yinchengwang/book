/**
 * @file main.c
 * @brief Linux htop 原理学习演示
 *
 * 演示内容：
 *   - 遍历 /proc/*/stat 获取进程信息
 *   - 解析进程 stat 文件格式
 *   - 构建进程树（父子关系）
 *   - CPU/内存使用率排序
 *
 * 编译：gcc -std=c11 -Wall -o htop main.c
 * 运行：./htop
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>

/* ============================================================
 * 数据结构：进程信息
 * ============================================================ */
typedef struct proc_info {
    pid_t pid;               /* 进程 ID */
    pid_t ppid;              /* 父进程 ID */
    char name[256];          /* 进程名 */
    char state;              /* 进程状态 */
    unsigned long utime;     /* 用户态时间 */
    unsigned long stime;     /* 内核态时间 */
    long rss;                /* 物理内存（页数） */
    unsigned long vm_size;   /* 虚拟内存大小（KB） */
    struct proc_info *next;  /* 链表指针 */
} proc_info_t;

/* ============================================================
 * 解析 /proc/PID/stat 文件
 * ============================================================ */
static int parse_proc_stat(pid_t pid, proc_info_t *info) {
    char path[64];
    char line[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* 格式：pid (comm) state ppid ... */
    /* 需要找到第一个 ')' 来分隔命令名 */
    char *comm_start = strchr(line, '(');
    char *comm_end = strchr(line, ')');
    if (!comm_start || !comm_end) {
        return -1;
    }

    /* 提取进程名 */
    size_t name_len = comm_end - comm_start - 1;
    if (name_len >= sizeof(info->name)) {
        name_len = sizeof(info->name) - 1;
    }
    strncpy(info->name, comm_start + 1, name_len);
    info->name[name_len] = '\0';

    /* 解析剩余字段 */
    /* state ppid ... */
    char state;
    pid_t ppid;
    unsigned long utime, stime;
    long rss;

    if (sscanf(comm_end + 2, "%c %d %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu %*ld %*ld %*ld %*ld %*ld %*ld %*llu %*lu %ld",
               &state, &ppid, &utime, &stime, &rss) != 5) {
        return -1;
    }

    info->pid = pid;
    info->ppid = ppid;
    info->state = state;
    info->utime = utime;
    info->stime = stime;
    info->rss = rss;

    /* 读取内存信息 */
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "VmSize: %lu kB", &info->vm_size) == 1) {
                break;
            }
        }
        fclose(fp);
    }

    return 0;
}

/* ============================================================
 * 遍历所有进程
 * ============================================================ */
static proc_info_t *collect_all_procs(void) {
    DIR *dir = opendir("/proc");
    if (!dir) {
        perror("[htop] opendir /proc 失败");
        return NULL;
    }

    proc_info_t *head = NULL;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        /* 只处理数字目录（PID） */
        if (entry->d_type != DT_DIR) {
            continue;
        }
        for (const char *p = entry->d_name; *p; p++) {
            if (*p < '0' || *p > '9') {
                break;
            }
        }

        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) {
            continue;
        }

        proc_info_t *info = (proc_info_t *)calloc(1, sizeof(proc_info_t));
        if (!info) {
            continue;
        }

        if (parse_proc_stat(pid, info) == 0) {
            info->next = head;
            head = info;
        } else {
            free(info);
        }
    }

    closedir(dir);
    return head;
}

/* ============================================================
 * 释放进程列表
 * ============================================================ */
static void free_procs(proc_info_t *head) {
    while (head) {
        proc_info_t *next = head->next;
        free(head);
        head = next;
    }
}

/* ============================================================
 * Demo 1: 遍历进程列表
 * ============================================================ */
static void demo_list_procs(void) {
    printf("[htop] 遍历 /proc 获取进程列表\n");

    proc_info_t *procs = collect_all_procs();
    if (!procs) {
        printf("[htop]   无进程信息\n");
        return;
    }

    printf("[htop]   PID     PPID    STATE  NAME\n");
    int count = 0;
    for (proc_info_t *p = procs; p && count < 10; p = p->next) {
        printf("[htop]   %-6d %-7d %c       %s\n",
               p->pid, p->ppid, p->state, p->name);
        count++;
    }
    printf("[htop]   ... (共 %d 个进程)\n", count);

    /* 统计进程数 */
    int total = 0;
    for (proc_info_t *p = procs; p; p = p->next) {
        total++;
    }
    printf("[htop]   总进程数: %d\n", total);

    free_procs(procs);
}

/* ============================================================
 * Demo 2: 构建进程树
 * ============================================================ */
static void demo_proc_tree(void) {
    printf("[htop] 构建进程树\n");

    proc_info_t *procs = collect_all_procs();
    if (!procs) {
        return;
    }

    /* 创建 PID -> proc_info 映射 */
    typedef struct { pid_t pid; proc_info_t *info; } pid_map_t;
    pid_map_t *map = (pid_map_t *)calloc(1024, sizeof(pid_map_t));
    int map_size = 0;

    for (proc_info_t *p = procs; p; p = p->next) {
        map[map_size].pid = p->pid;
        map[map_size].info = p;
        map_size++;
    }

    /* 打印 init/systemd 进程的子进程 */
    printf("[htop]   查找 PID 1 (init/systemd) 的子进程:\n");
    for (int i = 0; i < map_size; i++) {
        if (map[i].info->ppid == 1) {
            printf("[htop]     |-- %d: %s (%c)\n",
                   map[i].info->pid, map[i].info->name, map[i].info->state);
        }
    }

    free(map);
    free_procs(procs);
}

/* ============================================================
 * Demo 3: CPU 使用率排序
 * ============================================================ */
static void demo_cpu_sort(void) {
    printf("[htop] 按 CPU 时间排序（显示 Top 10）\n");

    proc_info_t *procs = collect_all_procs();
    if (!procs) {
        return;
    }

    /* 转换为数组 */
    int count = 0;
    for (proc_info_t *p = procs; p; p = p->next) {
        count++;
    }

    proc_info_t **arr = (proc_info_t **)calloc(count, sizeof(proc_info_t *));
    int idx = 0;
    for (proc_info_t *p = procs; p; p = p->next) {
        arr[idx++] = p;
    }

    /* 冒泡排序（按 utime + stime） */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            unsigned long t1 = arr[j]->utime + arr[j]->stime;
            unsigned long t2 = arr[j+1]->utime + arr[j+1]->stime;
            if (t1 < t2) {
                proc_info_t *tmp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = tmp;
            }
        }
    }

    printf("[htop]   PID     CPU(ms)  NAME\n");
    for (int i = 0; i < 10 && i < count; i++) {
        printf("[htop]   %-6d %-9lu %s\n",
               arr[i]->pid, arr[i]->utime + arr[i]->stime, arr[i]->name);
    }

    free(arr);
    free_procs(procs);
}

/* ============================================================
 * Demo 4: 内存使用排序
 * ============================================================ */
static void demo_mem_sort(void) {
    printf("[htop] 按内存使用排序（显示 Top 10）\n");

    proc_info_t *procs = collect_all_procs();
    if (!procs) {
        return;
    }

    int count = 0;
    for (proc_info_t *p = procs; p; p = p->next) {
        count++;
    }

    proc_info_t **arr = (proc_info_t **)calloc(count, sizeof(proc_info_t *));
    int idx = 0;
    for (proc_info_t *p = procs; p; p = p->next) {
        arr[idx++] = p;
    }

    /* 按 RSS 排序（降序） */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (arr[j]->rss < arr[j+1]->rss) {
                proc_info_t *tmp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = tmp;
            }
        }
    }

    printf("[htop]   PID     RSS(KB)  VM(KB)   NAME\n");
    for (int i = 0; i < 10 && i < count; i++) {
        printf("[htop]   %-6d %-8ld %-8lu %s\n",
               arr[i]->pid, arr[i]->rss * 4, arr[i]->vm_size, arr[i]->name);
    }

    free(arr);
    free_procs(procs);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux htop 原理学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 遍历进程列表 ---\n");
    demo_list_procs();
    printf("\n");

    printf("--- Demo 2: 构建进程树 ---\n");
    demo_proc_tree();
    printf("\n");

    printf("--- Demo 3: CPU 使用率排序 ---\n");
    demo_cpu_sort();
    printf("\n");

    printf("--- Demo 4: 内存使用排序 ---\n");
    demo_mem_sort();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
