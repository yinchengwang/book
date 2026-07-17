# 工程对照笔记：Redis 事件循环

## engineering/src/redis/event.c 对照

### 核心结构

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `AE_EventLoop` | `aeEventLoop` | 事件循环主结构 |
| `AE_FileEvent` | `aeFileEvent` | 文件事件 |
| `AE_TimerNode` | `aeTimeEvent` | 定时器节点 |
| `timer_heap` | `aeApiState` (epoll) | 底层复用机制 |

### 关键函数对照

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `event_loop_create()` | `aeCreateEventLoop()` | 创建事件循环 |
| `ae_create_file_event()` | `aeCreateFileEvent()` | 注册文件事件 |
| `ae_create_time_event()` | `aeCreateTimeEvent()` | 注册定时器 |
| `ae_process_events()` | `aeProcessEvents()` | 处理所有事件 |
| `get_current_ms()` | `ustime()` / `mstime()` | 获取当前时间 |

### 工程实现要点

1. **epoll 模型**（Linux）
   ```c
   // event.c 中的 ae Api 层
   static int aeApiCreate(aeEventLoop *eventLoop) {
       aeApiState *state = zmalloc(sizeof(aeApiState));
       state->epfd = epoll_create(1024);
       eventLoop->apidata = state;
   }
   ```

2. **时间获取优化**
   ```c
   // 使用 clock_gettime(CLOCK_MONOTONIC) 避免 NTP 调整
   // Redis 内部用 ustime() 返回微秒
   ```

3. **定时器实现**
   ```c
   // Redis 定时器基于最小堆 + 链表
   // 每次事件循环检查堆顶是否到期
   ```

4. **beforeSleep 与 afterSleep**
   ```c
   // 事件处理前后钩子
   aeSetBeforeSleepProc(loop, beforeSleep);
   aeSetAfterSleepProc(loop, afterSleep);
   ```

### 行数对比

| 组件 | 学习代码 | 工程代码（约）|
|------|---------|--------------|
| 事件循环核心 | ~200 行 | ~500 行 |
| epoll 封装 | ~50 行 | ~200 行 |
| 定时器管理 | ~80 行 | ~300 行 |

### 扩展阅读

- Redis 源码：`src/server.c` 中的 `aeMain()` 主循环
- `ae.c`：事件循环主体
- `ae_epoll.c`：epoll 实现
- `ae_evport.c`：Solaris event ports
- `ae_kqueue.c`：macOS/FreeBSD kqueue
- `ae_select.c`：select 降级方案
