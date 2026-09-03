# Linux htop 原理学习笔记

本文档记录进程监控在项目工程代码中的实际应用对照。

## 工程对照

### 1. 进程信息获取

项目代码通过 /proc 文件系统获取进程信息：

```c
// 工程中可能需要获取当前进程/线程信息
// 类似于 demo 中的 parse_proc_stat()
```

**htop 的核心数据源：**

| 数据源 | 内容 | 用途 |
|--------|------|------|
| /proc/PID/stat | CPU 时间、状态 | 排序、状态显示 |
| /proc/PID/status | 内存、线程数 | 内存排序 |
| /proc/PID/cmdline | 命令行参数 | 显示完整命令 |
| /proc/PID/fd/ | 文件描述符 | 查看打开的文件 |

### 2. 进程树构建

htop 通过 PPID 构建进程树：

```c
// 简化版进程树构建
typedef struct node {
    pid_t pid;
    char name[256];
    struct node *children[64];
    int child_count;
} proc_node_t;

// 1. 收集所有进程
// 2. 按 PID 索引
// 3. 根据 PPID 连接父子节点
// 4. 从根节点（PID=1）开始打印
```

**打印缩进算法：**

```c
void print_tree(proc_node_t *node, int depth) {
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    printf("|-- %s (PID=%d)\n", node->name, node->pid);
    for (int i = 0; i < node->child_count; i++) {
        print_tree(node->children[i], depth + 1);
    }
}
```

### 3. CPU 使用率计算

htop 使用以下公式计算 CPU 使用率：

```c
// 获取进程 CPU 时间
utime = proc_stat.utime;   // 用户态时间
stime = proc_stat.stime;   // 内核态时间
total_time = utime + stime;

// 获取系统总 CPU 时间（需要两次采样）
elapsed_time = end_total_cpu - start_total_cpu;

// 计算使用率
cpu_usage = 100.0 * (total_time / elapsed_time);
```

**采样间隔的影响：**

- 间隔太短：数据波动大
- 间隔太长：实时性差
- 推荐：1-2 秒间隔

### 4. 内存使用率

htop 显示的内存字段：

| 字段 | 来源 | 说明 |
|------|------|------|
| VmSize | /proc/PID/status | 虚拟内存大小 |
| VmRSS | /proc/PID/status | 物理内存（Resident Set） |
| VmData | /proc/PID/status | 数据段大小 |

**RSS 与实际内存占用：**

```
RSS = 代码段 + 数据段 + 堆 + 栈 + 共享库
```

### 5. 进程监控工具对比

| 工具 | 数据源 | 特点 |
|------|--------|------|
| top | /proc/stat, /proc/PID/stat | 简单、广泛可用 |
| htop | 同上 + /proc/PID/status | 彩色、进程树 |
| ps | 同上 | 适合脚本、批量处理 |
| pstree | /proc/PID/stat | 专门显示进程树 |

## 工程应用场景

### 诊断数据库连接泄漏

```c
// 工程代码中诊断连接问题
// 类似 htop 遍历 /proc/net/tcp 统计 TCP 连接
typedef struct {
    int total_conn;
    int established;
    int time_wait;
    int close_wait;
} net_stat_t;
```

### 监控后台任务

```c
// 工程中监控后台工作线程
// 类似 demo 中的 demo_cpu_sort()
// 按 CPU 时间排序，找出异常线程
```

## 参考资料

- `man proc` — /proc 文件系统完整说明
- `man 5 proc` — 进程信息的内核文档
- htop 源码：https://github.com/htop-dev/htop
