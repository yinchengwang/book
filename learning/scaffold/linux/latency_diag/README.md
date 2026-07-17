# latency_diag — 延迟诊断与瓶颈定位

## 简介

演示延迟分解方法论（USE/RED/延迟分解法），分析数据库查询各阶段耗时组成并定位瓶颈。

## 编译

```bash
gcc -std=c11 -Wall -o latency_diag main.c
```

## 运行

```bash
./latency_diag
```

## 核心方法

### USE 方法

- **Utilization（利用率）**：资源忙碌时间的百分比
- **Saturation（饱和度）**：等待队列长度
- **Errors（错误）**：错误计数

### RED 方法

- **Rate（速率）**：每秒请求数
- **Errors（错误）**：失败请求数
- **Duration（延迟）**：处理时长

### 延迟分解

将总延迟拆解为各阶段耗时，找出占比最大的阶段优先优化。

## 诊断工具

- `perf sched latency`：调度延迟分析
- `bpftrace funclatency.bt`：函数延迟分布
- Prometheus + Grafana：时序监控

## 扩展

- 结合 `perf trace -s` 分析系统调用延迟
