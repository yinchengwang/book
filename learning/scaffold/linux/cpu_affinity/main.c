/*
 * CPU 亲和性演示程序
 *
 * 演示内容：
 *   1. sched_setaffinity - 设置进程级 CPU 亲和性
 *   2. pthread_setaffinity_np - 设置线程级 CPU 亲和性
 *   3. getcpu() - 获取当前运行的 CPU 编号
 *   4. NUMA 节点亲和性查询
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>

// 获取系统 CPU 核心数
static int get_cpu_count(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

// 演示 getcpu() - 获取当前 CPU
static void demo_getcpu(void)
{
    int cpu, node;
    if (getcpu(&cpu, &node, NULL) == 0) {
        printf("  [getcpu] 当前运行在 CPU %d, NUMA 节点 %d\n", cpu, node);
    } else {
        perror("  [getcpu] 获取失败");
    }
}

// 演示 sched_setaffinity - 设置进程级 CPU 亲和性
static void demo_process_affinity(void)
{
    printf("\n[演示 1] 进程级 CPU 亲和性 (sched_setaffinity)\n");

    cpu_set_t mask;
    int cpu_count = get_cpu_count();

    // 获取当前亲和性掩码
    CPU_ZERO(&mask);
    if (sched_getaffinity(0, sizeof(mask), &mask) == 0) {
        printf("  当前进程允许运行的 CPU:");
        for (int i = 0; i < cpu_count; i++) {
            if (CPU_ISSET(i, &mask))
                printf(" %d", i);
        }
        printf("\n");
    }

    // 绑定到第一个 CPU
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) == 0) {
        printf("  已绑定到 CPU 0\n");
        demo_getcpu();
    } else {
        perror("  [sched_setaffinity] 绑定失败");
    }

    // 如果有多个 CPU，尝试绑定到第二个 CPU
    if (cpu_count > 1) {
        CPU_ZERO(&mask);
        CPU_SET(1, &mask);
        if (sched_setaffinity(0, sizeof(mask), &mask) == 0) {
            printf("  已切换到 CPU 1\n");
            demo_getcpu();
        }
    }
}

// 工作线程函数
static void *worker_thread(void *arg)
{
    int thread_id = *(int *)arg;
    printf("  线程 %d 启动\n", thread_id);

    // 线程级亲和性设置
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(thread_id % get_cpu_count(), &mask);

    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) == 0) {
        printf("  线程 %d 绑定到 CPU %d\n", thread_id, thread_id % get_cpu_count());
    }

    // 模拟一些计算
    for (int i = 0; i < 3; i++) {
        demo_getcpu();
        usleep(100000);
    }

    printf("  线程 %d 结束\n", thread_id);
    return NULL;
}

// 演示 pthread_setaffinity_np - 线程级亲和性
static void demo_thread_affinity(void)
{
    printf("\n[演示 2] 线程级 CPU 亲和性 (pthread_setaffinity_np)\n");

    pthread_t threads[2];
    int ids[2] = {0, 1};

    // 创建两个线程，观察它们分别绑定到的 CPU
    for (int i = 0; i < 2; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, &ids[i]) != 0) {
            perror("  [pthread_create] 创建线程失败");
            return;
        }
    }

    // 等待线程结束
    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }
}

// 演示 NUMA 节点信息
static void demo_numa_info(void)
{
    printf("\n[演示 3] NUMA 节点信息\n");

    // 尝试读取 NUMA 节点数量
    char path[128];
    FILE *fp;
    int numa_nodes = 1;  // 默认值

    fp = fopen("/sys/devices/system/node/has_kernel_cpu", "r");
    if (fp != NULL) {
        if (fscanf(fp, "%d", &numa_nodes) != 1) {
            numa_nodes = 1;
        }
        fclose(fp);
    }

    printf("  NUMA 节点数: %d\n", numa_nodes);

    // 列出每个节点上的 CPU
    for (int node = 0; node < numa_nodes; node++) {
        snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/cpulist", node);
        fp = fopen(path, "r");
        if (fp != NULL) {
            char buf[256];
            if (fgets(buf, sizeof(buf), fp)) {
                printf("  节点 %d 上的 CPU: %s", node, buf);
            }
            fclose(fp);
        }
    }

    // 获取当前进程的 NUMA 亲和性
    unsigned long nodemask = 0;
    int node = -1;
    if (getcpu(NULL, &node, &nodemask) == 0) {
        printf("  当前 CPU 属于 NUMA 节点: %d\n", node);
    }
}

int main(void)
{
    printf("========================================\n");
    printf("       CPU 亲和性演示程序\n");
    printf("========================================\n");
    printf("系统 CPU 核心数: %d\n", get_cpu_count());

    demo_getcpu();
    demo_process_affinity();
    demo_thread_affinity();
    demo_numa_info();

    printf("\n========================================\n");
    printf("演示完成\n");
    printf("========================================\n");

    return 0;
}
