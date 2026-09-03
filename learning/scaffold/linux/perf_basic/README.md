# perf_basic — perf 性能分析基础

## 简介

演示 Linux `perf` 性能分析工具的基本用法，包括 `perf stat`、`perf record`、`perf report`。

## 编译

```bash
gcc -std=c11 -Wall -o perf_basic main.c
```

## 运行

```bash
./perf_basic
```

## 前提条件

```bash
# 安装 perf（Linux-tools）
sudo apt install linux-tools-common linux-tools-generic

# 解除采样限制（非 root 用户）
sudo sysctl kernel.perf_event_paranoid=-1
```

## 核心功能

### 1. perf stat

统计程序执行的硬件/软件事件：

```bash
perf stat ./program
# 输出：
#   1,234,567  cycles
#     567,890  instructions
#      12,345  cache-misses
```

### 2. perf record

采样记录，生成 `perf.data`：

```bash
perf record -g ./program
perf record -e cycles -p <pid>  # 采样运行中进程
```

### 3. perf report

分析采样数据：

```bash
perf report --stdio -i perf.data
```

## 扩展

- 尝试 `perf top` 实时查看系统热点
- 使用 `perf sched` 分析调度延迟
