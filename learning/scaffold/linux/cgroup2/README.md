# cgroup2 — cgroup v2 资源控制

## 简介

演示 Linux cgroup v2 单层级资源控制模型，包括内存限制、CPU 限制和容器隔离。

## 编译

```bash
gcc -std=c11 -Wall -o cgroup2 main.c
```

## 运行

```bash
./cgroup2
```

## 核心概念

### cgroup v2 层级

```
根 cgroup
├── system.slice (系统服务)
│   └── db-server.service
└── user.slice (用户应用)
    └── container-abc123 (Docker 容器)
```

### 主要控制器

- `memory`：内存使用限制与统计
- `cpu`：CPU 带宽限制与权重
- `io`：磁盘 I/O 限制
- `pids`：进程数限制
- `cpuset`：CPU 亲和性

## 关键文件

- `/sys/fs/cgroup/cgroup.controllers`：可用控制器
- `/sys/fs/cgroup/memory.max`：内存硬限制
- `/sys/fs/cgroup/memory.current`：当前使用量

## 扩展

- 查看 `systemd-cgls` 了解 cgroup 层级树
- 尝试 `systemd-run --scope -p MemoryMax=256M ./program` 创建临时 cgroup
