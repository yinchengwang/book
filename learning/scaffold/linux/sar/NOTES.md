# Linux SAR 学习笔记

本文档记录性能数据收集在项目工程代码中的实际应用对照。

## 工程对照

### 1. sysinfo 在工程中的应用

工程代码可能使用 sysinfo 监控资源使用：

```c
// 类似 demo 中的 sysinfo() 调用
// 工程中诊断内存压力
struct sysinfo info;
sysinfo(&info);

double mem_usage = 100.0 * (1.0 - (double)info.freeram / info.totalram);
if (mem_usage > 90.0) {
    // 触发紧急刷盘或拒绝新请求
}
```

**sysinfo vs /proc/meminfo：**

| 特性 | sysinfo | /proc/meminfo |
|------|---------|---------------|
| 接口 | 系统调用 | 虚拟文件 |
| 实时性 | 内核直接返回 | 读取文件 |
| 详细程度 | 较低 | 高（多个字段） |

### 2. 性能数据采样模式

工程代码实现性能监控时常用的采样模式：

```c
// engineering/src/db/monitor/ 中可能有类似实现
typedef struct {
    unsigned long long timestamp;
    double cpu_usage;
    double mem_usage;
    double disk_io;
} perf_sample_t;

// 采样线程
void *sampling_thread(void *arg) {
    while (running) {
        perf_sample_t sample = {0};
        sample.timestamp = get_time_ms();
        sample.cpu_usage = get_cpu_usage();
        sample.mem_usage = get_mem_usage();

        add_sample(&sample);
        sleep(1);  // 每秒采样
    }
}
```

### 3. 历史数据收集

SAR 的核心功能是历史数据收集：

```c
// 类似 sar 的数据存储格式
typedef struct {
    time_t time;
    unsigned long cpu_user;
    unsigned long cpu_system;
    unsigned long mem_used;
} sar_record_t;

// 环形缓冲区存储历史数据
#define MAX_SAMPLES 86400  // 保存 24 小时
static sar_record_t samples[MAX_SAMPLES];
static int sample_index = 0;
```

**数据压缩策略：**

| 保留周期 | 采样间隔 | 数据点 |
|----------|----------|--------|
| 1 小时 | 1 秒 | 3600 |
| 1 天 | 1 分钟 | 1440 |
| 7 天 | 5 分钟 | 2016 |

### 4. 负载计算

sysinfo 返回的负载值是固定点数：

```c
// 负载值转换
double load_1min = info.loads[0] / 65536.0;
double load_5min = info.loads[1] / 65536.0;
double load_15min = info.loads[2] / 65536.0;
```

**负载与 CPU 使用率的关系：**

- 负载 < CPU 核数：CPU 有空闲
- 负载 ≈ CPU 核数：CPU 满载
- 负载 > CPU 核数：有进程在等待

### 5. 工程中的监控集成

工程代码的性能监控架构：

```
┌─────────────────────────────────────────┐
│           性能监控层                      │
├─────────────────────────────────────────┤
│  CPU 采样    │  内存采样   │  I/O 采样   │
│  /proc/stat  │  /proc/mem  │  /proc/disk │
└──────┬───────┴──────┬──────┴──────┬─────┘
       │              │             │
       ▼              ▼             ▼
┌─────────────────────────────────────────┐
│         指标聚合器                        │
│    (计算平均值/峰值/百分位)               │
└──────────────────┬──────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│         告警系统                          │
│    (阈值检查 + 通知)                      │
└─────────────────────────────────────────┘
```

## 参考源码

- `man sysinfo` — 系统调用文档
- `man proc` — /proc 文件系统说明
- `sysstat` 包源码 — 官方 sar 实现参考
