# numa_observ — NUMA 观测与内存分布

## 简介

演示 Linux NUMA 架构观测，包括拓扑检测、numa_alloc_onnode 就近分配、numactl 资源控制。

## 编译

```bash
gcc -std=c11 -Wall -o numa_observ main.c -lnuma
```

## 运行

```bash
./numa_observ
```

## 前提条件

```bash
sudo apt install numactl libnuma-dev
```

## 核心概念

### NUMA 架构

```
┌─────────────┐       ┌─────────────┐
│  CPU 0-7    │←QPI→  │  CPU 8-15   │
│  内存 0     │       │  内存 1     │
│ (本地快)     │       │ (远程慢)     │
└─────────────┘       └─────────────┘
```

### 内存分配策略

- **默认**：分配到当前 CPU 本地节点
- **绑定**：严格分配到指定节点
- **交织**：轮询分配到所有节点
- **优先**：优先指定节点，满后 scatter

## 关键工具

- `numactl --hardware`：查看拓扑
- `numastat -p <pid>`：查看进程分布
- `perf stat -e numa-*`：监控 NUMA 事件

## 扩展

- 尝试 `numactl --interleave=all ./program` 对比性能
