# seccomp 工程对照笔记

## 容器安全机制

### Docker seccomp 配置

Docker 为每个容器自动加载默认 seccomp profile，禁止约 44 个危险系统调用：

```json
{
  "defaultAction": "SCMP_ACT_ERRNO",
  "architectures": ["SCMP_ARCH_X86_64"],
  "syscalls": [
    { "names": ["read","write","close","fstat","..."], "action":"SCMP_ACT_ALLOW" },
    { "names": ["mount","kexec_load","reboot","..."], "action":"SCMP_ACT_KILL" }
  ]
}
```

### Kubernetes 集成

```yaml
spec:
  securityContext:
    seccompProfile:
      type: RuntimeDefault
```

### 数据库场景

- 数据库 worker 进程用 seccomp 限制：仅允许 I/O + 内存 + IPC 系统调用
- PostgreSQL 可配置 `seccomp_filter` 编译选项
- 多租户环境：每个租户的查询执行器用独立 seccomp profile

### 工程要点

1. **最小权限原则**：仅开放进程实际需要的系统调用
2. **注意兼容性**：不同内核版本的系统调用表可能不同
3. **容器与 seccomp**：Docker/K8s 运行时自动提供默认 profile
4. **测试充分**：某些合法操作（如 temp file open）可能被误禁
