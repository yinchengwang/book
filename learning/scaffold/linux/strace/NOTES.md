# strace 工程对照笔记

## 调试工具实现原理

### ptrace 核心机制

strace 通过 `ptrace()` 系统调用实现用户态系统调用追踪：

```
用户进程（被追踪）
    │
    ├→ syscall entry → 内核通知 tracer（SIGTRAP）
    │                  → tracer 读取寄存器（rax = 系统调用号）
    │
    ├→ syscall exit  → 内核再次通知 tracer
    │                  → tracer 读取返回值（rax）
    │
    └→ 继续执行 ← tracer 调用 ptrace(PTRACE_SYSCALL)
```

### 工程中的调试工具

1. **strace**：系统调用级别追踪
2. **ltrace**：库函数调用追踪（PLT/GOT hook）
3. **perf trace**：基于 perf_event 的低开销追踪
4. **bpftrace**：eBPF 动态追踪（开销最低）

### 性能开销分析

- **ptrace**：每个系统调用 2 次上下文切换 + 2 次信号传递 → 20-50% 性能影响
- **perf trace**：使用 perf_event 采样 → 5-10% 开销
- **eBPF bpftrace**：内核态 JIT 编译 → 1-5% 开销
- **数据库场景**：不要用 strace 实时追踪，先 perf 采样定位后再用 strace
