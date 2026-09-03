# CPU 亲和性

## 概述

CPU 亲和性（CPU Affinity）是一种将进程或线程绑定到特定 CPU 核心运行的机制。通过设置亲和性，可以：

- **性能优化**：减少缓存失效，提高缓存命中率
- **延迟降低**：避免 CPU 迁移带来的开销
- **资源隔离**：为特定工作负载预留专用 CPU
- **NUMA 优化**：让进程访问本地内存，减少跨节点内存访问

## 核心 API

### 1. sched_setaffinity / sched_getaffinity（进程级）

```c
#include <sched.h>

int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask);
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);
```

- `pid`：目标进程 ID，0 表示当前进程
- `cpusetsize`：cpu_set_t 结构体大小
- `mask`：CPU 亲和性掩码

### 2. pthread_setaffinity_np / pthread_getaffinity_np（线程级）

```c
#include <pthread.h>

int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const cpu_set_t *mask);
int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize, cpu_set_t *mask);
```

- 针对单个线程设置亲和性
- 常用于多线程服务器，为不同工作线程绑定不同 CPU

### 3. CPU_SET 宏操作

```c
void CPU_ZERO(cpu_set_t *set);           // 清空集合
void CPU_SET(int cpu, cpu_set_t *set);   // 添加 CPU
void CPU_CLR(int cpu, cpu_set_t *set);   // 移除 CPU
int  CPU_ISSET(int cpu, cpu_set_t *set); // 检查是否设置
```

### 4. getcpu()

```c
#include <sched.h>

int getcpu(unsigned int *cpu, unsigned int *node, void *tcache);
```

- 获取当前线程运行的 CPU 编号和 NUMA 节点
- `node` 参数可为 NULL

## 编译运行

```bash
# 编译
gcc -std=c11 -Wall -O2 -o cpu_affinity main.c

# 或使用 Makefile
make

# 运行
./cpu_affinity

# 清理
make clean
```

## 注意事项

1. 需要定义 `_GNU_SOURCE` 宏以启用非标准扩展
2. 亲和性设置需要相应权限
3. 绑定到不存在的 CPU 会导致失败
4. 容器环境中 CPU 可能被限制
