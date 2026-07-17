# XDP 快速路径

## 概述

XDP (eXpress Data Path) 是 Linux 4.8+ 引入的高性能数据包处理框架，在驱动层执行 eBPF 程序，实现极早期数据包处理。

## XDP vs iptables

| 指标 | XDP | iptables |
|------|-----|----------|
| 位置 | 驱动层 | netfilter |
| 性能 | 10M+ pps | ~1M pps |
| 延迟 | 极低 | 较高 |

## 核心概念

### XDP 动作

- `XDP_PASS` - 放行到普通网络栈
- `XDP_DROP` - 丢弃数据包
- `XDP_TX` - 直接发回
- `XDP_REDIRECT` - 重定向到其他接口

### BPF 映射

内核与用户空间共享数据的机制，支持 Hash/Array 等类型。

## 编译运行

```bash
make              # 编译
make run          # 运行演示
```
