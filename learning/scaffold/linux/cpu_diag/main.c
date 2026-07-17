/**
 * @file main.c
 * @brief CPU 诊断演示
 *
 * 演示获取 CPU 核心数、利用率、上下文切换
 * 对应 Linux 系统性能监控中的 CPU 观测
 *
 * 编译：gcc -std=c11 -o cpu_diag main.c
 * 运行：./cpu_diag
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sched.h>

/* 获取 CPU 核心数 */
static int get_cpu_count(void) {
    return sysconf(_SC_NPROCESSORS_CONF);
}

static int get_online_cpu_count(void) {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

/* CPU 利用率采样 */
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
} CpuTimes;

static int read_cpu_times(CpuTimes *times) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return -1;

    char line[256];
    /* 读取第一行（aggregate CPU） */
    if (fgets(line, sizeof(line), fp) &&
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu",
               &times->user, &times->nice, &times->system,
               &times->idle, &times->iowait, &times->irq, &times->softirq) != 7) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static double calc_cpu_usage(const CpuTimes *prev, const CpuTimes *curr) {
    unsigned long long prev_idle = prev->idle + prev->iowait;
    unsigned long long curr_idle = curr->idle + curr->iowait;

    unsigned long long prev_total = prev->user + prev->nice + prev->system +
                                    prev->idle + prev->iowait + prev->irq + prev->softirq;
    unsigned long long curr_total = curr->user + curr->nice + curr->system +
                                    curr->idle + curr->iowait + curr->irq + curr->softirq;

    unsigned long long total_diff = curr_total - prev_total;
    unsigned long long idle_diff = curr_idle - prev_idle;

    if (total_diff == 0) return 0.0;
    return 100.0 * (1.0 - (double)idle_diff / total_diff);
}

/* 上下文切换统计 */
static void show_context_switches(void) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;

    char line[256];
    printf("\n=== 上下文切换统计 ===\n");
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "ctxt ", 5) == 0) {
            unsigned long long ctxt;
            sscanf(line + 5, "%llu", &ctxt);
            printf("上下文切换次数: %llu\n", ctxt);
        } else if (strncmp(line, "intr ", 5) == 0) {
            unsigned long long intr;
            sscanf(line + 5, "%llu", &intr);
            printf("中断次数: %llu\n", intr);
        } else if (strncmp(line, "softirq ", 8) == 0) {
            unsigned long long softirq;
            sscanf(line + 8, "%llu", &softirq);
            printf("软中断次数: %llu\n", softirq);
        }
    }
    fclose(fp);
}

/* 进程 CPU 使用（遍历 /proc/[pid]/stat）*/
static void show_top_cpu_processes(void) {
    printf("\n=== CPU 占用最高的进程（采样）===\n");
    printf("%-10s %-8s %-8s\n", "PID", "COMM", "CPU%%");

    DIR *proc = opendir("/proc");
    if (!proc) return;

    struct dirent *entry;
    typedef struct { int pid; char comm[256]; double cpu; } ProcInfo;
    ProcInfo procs[20];
    int count = 0;

    while ((entry = readdir(proc)) && count < 20) {
        if (entry->d_type != DT_DIR) continue;

        char *end;
        int pid = strtol(entry->d_name, &end, 10);
        if (*end != '\0' || pid <= 0) continue;

        char stat_path[64];
        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);

        FILE *fp = fopen(stat_path, "r");
        if (!fp) continue;

        char buf[1024];
        if (fgets(buf, sizeof(buf), fp)) {
            /* 解析 stat 文件：(pid) comm state ... utime stime */
            char comm[256] = {0};
            unsigned long utime, stime;

            /* 找第一个 ) 的位置 */
            char *p = strchr(buf, ')');
            if (p) {
                sscanf(p + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                       &utime, &stime);
                /* 提取 comm */
                char *start = strchr(buf, '(');
                char *end2 = strchr(buf, ')');
                if (start && end2 && end2 > start) {
                    int len = end2 - start - 1;
                    if (len > 0 && len < 256) {
                        memcpy(comm, start + 1, len);
                        comm[len] = '\0';
                    }
                }
                procs[count].pid = pid;
                strncpy(procs[count].comm, comm, 255);
                procs[count].cpu = (utime + stime) / 100.0;  // 简化计算
                count++;
            }
        }
        fclose(fp);
    }
    closedir(proc);

    /* 按 CPU 使用排序（简单选择排序）*/
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (procs[j].cpu > procs[i].cpu) {
                ProcInfo tmp = procs[i];
                procs[i] = procs[j];
                procs[j] = tmp;
            }
        }
    }

    for (int i = 0; i < count && i < 5; i++) {
        printf("%-10d %-8s %6.1f%%\n", procs[i].pid, procs[i].comm, procs[i].cpu);
    }
}

int main(void) {
    printf("===========================================\n");
    printf("  CPU 诊断演示\n");
    printf("===========================================\n");

    /* 核心数信息 */
    int configured = get_cpu_count();
    int online = get_online_cpu_count();
    printf("\n=== CPU 核心数 ===\n");
    printf("配置核心数: %d\n", configured);
    printf("在线核心数: %d\n", online);

    /* CPU 利用率采样 */
    printf("\n=== CPU 利用率 ===\n");
    CpuTimes before, after;

    if (read_cpu_times(&before) == 0) {
        printf("采样中（1秒）...\n");
        sleep(1);
        if (read_cpu_times(&after) == 0) {
            double usage = calc_cpu_usage(&before, &after);
            printf("总体 CPU 使用率: %.1f%%\n", usage);
        }
    }

    /* 上下文切换 */
    show_context_switches();

    /* Top CPU 进程 */
    show_top_cpu_processes();

    printf("\n===========================================\n");
    return 0;
}
