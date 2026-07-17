# NOTES.md — cgroup2 工程对照

## cgroup v2 在容器化数据库中的应用

### 背景

cgroup v2 是 Linux 资源控制的基础，所有容器技术（Docker、Podman、Kubernetes）都依赖 cgroup 实现资源隔离和限制。数据库作为容器中的核心应用，正确配置 cgroup 是生产稳定性的前提。

### 工程对照：数据库资源隔离

在我们的 `engineering/src/db/core/` 模块中，需要与 cgroup 协作：

```c
// db_init.c — 数据库启动时检测 cgroup 限制
typedef struct {
    uint64_t memory_limit_mb;    // cgroup 内存限制
    double cpu_limit;            // cgroup CPU 限制
    int cgroup_version;          // v1 还是 v2
} CgroupInfo;

/* 检测 cgroup 资源限制 */
int detect_cgroup_limits(CgroupInfo *info) {
    FILE *fp;
    char buf[64];

    // 检测内存限制
    fp = fopen("/sys/fs/cgroup/memory.max", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            if (strcmp(buf, "max\n") == 0) {
                info->memory_limit_mb = 0;  // 无限制
            } else {
                info->memory_limit_mb = strtoull(buf, NULL, 10) / (1024 * 1024);
            }
        }
        fclose(fp);
        info->cgroup_version = 2;
    }

    // 检测 CPU 限制
    fp = fopen("/sys/fs/cgroup/cpu.max", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            if (strncmp(buf, "max", 3) == 0) {
                info->cpu_limit = 0;  // 无限制
            } else {
                sscanf(buf, "%*u %lf", &info->cpu_limit);
            }
        }
        fclose(fp);
    }

    return 0;
}

/* 根据 cgroup 限制调整数据库参数 */
void adapt_to_cgroup_limits(void) {
    CgroupInfo info;
    if (detect_cgroup_limits(&info) < 0) return;

    if (info.memory_limit_mb > 0) {
        // shared_buffers = cgroup 内存限制 * 25%
        int suggested_buffers = info.memory_limit_mb / 4;

        if (shared_buffers > suggested_buffers) {
            log_warn("shared_buffers (%dMB) 超过建议值 (%dMB) "
                     "(cgroup 限制: %luMB)",
                     shared_buffers, suggested_buffers,
                     info.memory_limit_mb);

            // 自动降级
            shared_buffers = suggested_buffers;
            log_info("shared_buffers 已调整为 %dMB", shared_buffers);
        }
    }

    if (info.cpu_limit > 0 && info.cpu_limit < 1.0) {
        log_warn("CPU 限制 <1 核心 (%.2f CPU)，并行查询可能受限",
                 info.cpu_limit);
        max_parallel_workers = 0;  // 禁用并行
    }
}
```

### cgroup 限制与数据库配置

| cgroup 限制 | 数据库参数 | 调整策略 |
|-------------|-----------|---------|
| memory.max | shared_buffers | cgroup 的 25% |
| memory.max | work_mem | cgroup 的 1% |
| memory.max | effective_cache_size | cgroup 的 50% |
| cpu.max | max_parallel_workers | CPUs - 1 |
| io.max | checkpoint rate | I/O 限制对齐 |

### Docker/K8s 中的 cgroup

```yaml
# Kubernetes Pod 中的数据库资源限制
apiVersion: v1
kind: Pod
metadata:
  name: postgres-db
spec:
  containers:
  - name: postgres
    image: postgres:15
    resources:
      limits:
        memory: "2Gi"          # → memory.max = 2Gi
        cpu: "2000m"           # → cpu.max = 2
      requests:
        memory: "1Gi"          # → memory.min = 1Gi
        cpu: "1000m"           # → cpu.weight = 对应 1 CPU

    # init script 根据 cgroup 自动调整 PG 参数
    command:
      - sh
      - -c
      - |
        cgroup_mem=$(cat /sys/fs/cgroup/memory.max)
        pg_buffers=$((cgroup_mem / 4))
        echo "shared_buffers = ${pg_buffers}" >> postgresql.conf
        exec postgres -D /data
```

### cgroup 故障排查

```bash
# 1. 检查数据库是否被 OOM Killer 杀死
dmesg | grep -i "oom.*postgres"

# 2. 查看 cgroup OOM 计数
cat /sys/fs/cgroup/memory.events | grep oom

# 3. 查看 cgroup 压力（PSI）
cat /sys/fs/cgroup/cpu.pressure
cat /sys/fs/cgroup/memory.pressure
cat /sys/fs/cgroup/io.pressure

# 4. 容器故障（K8s 场景）
kubectl describe pod postgres-pod | grep -A10 "Events"
```

### 最佳实践

1. **内存限制对齐**：`shared_buffers` + `work_mem * max_connections` < cgroup 内存限制
2. **OOM 预留**：至少为 cgroup 内存限制留 20% 给 OS 缓存
3. **CPU 限制**：合理设置 CPU 限制避免被 throttle
4. **PSI 监控**：cgroup 压力计数可提前预警资源不足

### 性能影响

- 正确的 cgroup 限制：无额外开销
- 不合理的限制：OOM 或 CPU throttle 导致突增延迟
- 内存限制过低 → 频繁 OOM → 数据库不可用

### 扩展思考

- Kubernetes VPA（垂直自动伸缩）自动调整 cgroup 限制
- cgroup 压力感知的查询调度器
- 多租户数据库中，每个租户独立 cgroup 计费
