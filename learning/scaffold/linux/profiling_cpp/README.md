# profiling_cpp

C++ 性能分析演示项目。展示 CPU 热点（计算密集型）和 I/O 热点的创建与测量方法。

## Demo 使用

### 编译

```bash
make
```

### 运行

```bash
make run
# 或直接
./profiling_demo
```

### perf 工具速查

| 命令 | 说明 |
|------|------|
| `perf top` | 实时显示各函数 CPU 占用率（按 % 排序） |
| `perf top -p PID` | 监控指定进程 |
| `perf top -t TID` | 监控指定线程 |
| `perf record -F 99 -g -p PID` | 99Hz 采样并记录调用栈（Ctrl+C 停止） |
| `perf report -g` | 以调用图方式查看采样报告 |
| `perf stat -e cycles,instructions ./prog` | 统计程序执行的硬件事件 |
| `perf sched latancy` | 分析调度延迟（定位 lock 竞争） |
| `perf list` | 列出当前系统支持的性能事件 |

## 分析链路

逐级深入，快速定位热点：

```
进程级   perf top -p PID         # 看哪个进程最耗 CPU
  │
  ▼
线程级   perf top -t TID          # 看哪个线程最耗 CPU
  │
  ▼
函数级   perf record -F 99 -g -p PID
         perf report -g          # 展开调用栈，看热点的父函数
  │
  ▼
指令级   perf stat -e cycles,instructions ./prog
                                 # 量化 CPI，区分计算瓶颈 or 访存瓶颈
```

### 常见瓶颈特征

| 热点位置 | 可能的瓶颈 |
|----------|------------|
| `sqrt`/`pow`/`sin` | 计算密集，CPU 算力不足 |
| `memcpy`/`memset` | 内存带宽瓶颈 |
| `buffer_get_page` | Buffer Pool Cache Miss |
| `malloc`/`free` | 堆分配竞争 |
| `futex`/`pthread_mutex` | 锁竞争 |

## 交叉平台

- **Linux/macOS**：使用 `getrusage()` 获取 CPU 时间，`getpid()` 获取 PID
- **Windows**：使用 `QueryPerformanceCounter()` 计时，`GetCurrentProcessId()` 获取 PID

perf 工具仅在 Linux 下可用，Windows 下请使用 WSL。
