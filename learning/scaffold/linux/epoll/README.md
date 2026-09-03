# epoll 事件驱动编程

## 概述

本卡演示 Linux epoll 高性能事件驱动编程，包括 LT/ET 模式和 Reactor 模式。

## epoll vs select/poll

| 特性 | select | poll | epoll |
|------|--------|------|-------|
| 最大 fd 数 | FD_SETSIZE (通常 1024) | 无限制 | 无限制 |
| 时间复杂度 | O(n) 遍历所有 fd | O(n) 遍历所有 fd | O(1) 只返回就绪 fd |
| 内存拷贝 | 每次都要拷贝 fd 集合 | 每次都要拷贝数组 | 仅拷贝一次 |
| 水平触发 | 支持 | 支持 | 支持 |
| 边缘触发 | 不支持 | 不支持 | 支持 |
| 适用场景 | fd 少 | fd 中等 | fd 非常多 |

## 核心 API

### 创建 epoll 实例

```c
int epfd = epoll_create1(0);  // Linux 2.6.27+
```

### 注册事件

```c
struct epoll_event ev;
ev.events = EPOLLIN;        // 可读事件
ev.data.fd = fd;            // 用户数据（通常是 fd）
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
```

### 等待事件

```c
struct epoll_event events[MAX_EVENTS];
int n = epoll_wait(epfd, events, MAX_EVENTS, timeout_ms);
```

## LT vs ET 模式

### Level-Triggered (LT)

- 条件满足时持续触发
- 可以只读取部分数据
- 编程简单，类似 select/poll

### Edge-Triggered (ET)

- 只在状态变化时触发一次
- 必须一次性读取所有数据（循环直到 EAGAIN）
- 必须配合非阻塞 socket
- 性能更高，但编程复杂

## 编译运行

```bash
make              # 编译
make run          # 运行完整演示
make run-et       # 仅运行 ET 模式
```

## Reactor 模式

```
┌─────────────────────────────────────────────────────┐
│                    Event Loop                        │
│  ┌─────────────┐    ┌─────────────┐                │
│  │ epoll_wait  │ -> │ handle_event │                │
│  └─────────────┘    └──────┬──────┘                │
│                            │                        │
│         ┌──────────────────┼──────────────────┐    │
│         ▼                  ▼                  ▼    │
│   ┌──────────┐       ┌──────────┐       ┌──────────┐│
│   │ accept   │       │  read    │       │  write   ││
│   └──────────┘       └──────────┘       └──────────┘│
└─────────────────────────────────────────────────────┘
```

## 学习要点

1. **epoll_create1** 创建 epoll 实例
2. **epoll_ctl** 注册/修改/删除监控的 fd
3. **epoll_wait** 等待事件发生
4. **LT 模式**：简单，容许部分读取
5. **ET 模式**：高性能，必须配合非阻塞 + 全量读取
