# CPU 亲和性 - 工程对照笔记

## 工程应用场景

### 网络处理优化

在高并发网络服务器中，CPU 亲和性是性能优化的重要手段。

**典型场景**：Nginx、Redis 等高性能服务器通常采用「工作线程绑定专用 CPU」的模式：

1. **主进程绑定 CPU 0**：处理信号、定时器等控制平面任务
2. **工作进程各绑定独立 CPU**：处理连接、协议解析、业务逻辑

这种绑定的收益在于：
- 减少进程切换带来的缓存失效（尤其 L1/L2 缓存）
- 让每个 CPU 核心的 L1/L2 缓存专属于特定线程，热点数据常驻
- 配合 SO_REUSEPORT 时，实现真正的「接收线程与 CPU 核心一一对应」

**代码模式示例**：

```c
void bind_to_cpu(int cpu_id)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
}
```

### 数据库连接池

数据库连接池（如 PostgreSQL libpq 连接池、MySQL Connector/CJ 连接池）在多线程环境下，可以使用 CPU 亲和性优化：

1. **连接会话亲和**：将同一会话的所有操作绑定到同一 CPU，保证缓存命中率
2. **DDL/DML 分离**：DDL 线程绑定到维护 CPU，DML 线程绑定到查询 CPU
3. **后台任务隔离**：VACUUM、CHECKPOINT 等后台任务绑定到低优先级 CPU

对于 OLTP 场景，连接池规模通常等于 CPU 核心数（每个核心一个长连接），此时 CPU 亲和性配合连接池可以实现「零争用」架构。

### 多线程场景结合

本项目的数据库存储引擎采用多线程架构：

- **WAL 写线程**：专门处理日志写入，绑定到独立 CPU 避免与其他线程争用
- **Buffer Pool 刷脏线程**：周期性将脏页刷盘，绑定到低优先级 CPU
- **查询执行器线程**：每个连接一个执行线程，通过亲和性绑定到固定 CPU

这种设计在 NUMA 环境下尤其重要：确保线程访问本地节点的 Buffer Pool，减少跨节点内存访问延迟（跨 NUMA 访问延迟通常是本地访问的 2-3 倍）。

## 关键参数

| 参数 | 说明 |
|------|------|
| `sched_setaffinity` | 进程级绑定，fork 后生效 |
| `pthread_setaffinity_np` | 线程级绑定，更细粒度控制 |
| `getcpu()` | 运行时获取当前 CPU，用于调试 |
| `CPU_SETSIZE` | 系统支持的最大 CPU 数 |

## 调试方法

查看当前进程的 CPU 亲和性：

```bash
# 查看进程的 CPU 亲和性掩码
taskset -p <pid>

# 查看线程的 CPU 亲和性
taskset -p -t <pid>
```

## 注意事项

1. 容器环境下 `/sys/devices/system/cpu/` 可能受限
2. 超线程（SMT）环境下，绑定到物理核心的相邻逻辑核心可能有干扰
3. 动态调整亲和性需要谨慎，避免热迁移开销
