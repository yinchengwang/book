# ebpf_intro — eBPF 入门

## 简介

演示 Linux eBPF（extended Berkeley Packet Filter）技术基础，展示 BCC 工具使用方法和 eBPF 探针机制。

## 编译

```bash
gcc -std=c11 -Wall -o ebpf_intro main.c
```

## 运行

```bash
./ebpf_intro
```

## 前提条件

```bash
# 安装 BCC/BPF 工具链
sudo apt install bpfcc-tools linux-headers-$(uname -r)
sudo apt install bpftrace

# 验证支持
sudo bpftrace -e 'BEGIN { printf("hello eBPF!\n"); exit(); }'
```

## 核心概念

### eBPF 工作流程

```
用户空间                 内核空间
  BCC/Python 程序  →  eBPF 字节码  →  验证器  →  JIT 编译  →  执行
       ↓                                        ↓
  BPF Maps ← ← ← ← ← ← ← ← ← ← ← ←  内核事件/追踪点
```

### 追踪方式

- **kprobe/kretprobe**：内核函数探针
- **uprobe/uretprobe**：用户态函数探针
- **tracepoint**：内核静态追踪点（推荐）
- **USDT**：用户空间静态追踪点

## 常见工具

- `bpftrace`：类 AWK 脚本语言
- `bcc/tools/*`：Python 封装的高层工具
- `perf`：perf_event 集成 eBPF

## 扩展

- 阅读 `man bpftrace` 了解完整语法
- 尝试 `/usr/share/bcc/tools/` 中的数十个工具
