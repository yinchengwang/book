# python-threading 学习笔记

## 概念地图

Python threading 模块提供多线程支持：

- **GIL（全局解释器锁）**：CPython 单进程同一时刻只允许一个线程执行字节码
- **适用场景**：I/O 密集型（网络请求、文件读写）
- **不适用场景**：CPU 密集型（计算密集），用 multiprocessing

## 同步原语

| 原语 | 用途 |
|------|------|
| Lock | 互斥锁，一次只能一个线程持有 |
| RLock | 可重入锁，同一线程可多次获取 |
| Event | 事件通知，一个线程通知其他线程 |
| Condition | 条件变量，等待特定条件 |
| Semaphore | 信号量，限制并发数 |
| Queue | 线程安全队列 |

## 踩坑记录

1. **死锁**：多个锁的获取顺序不一致导致死锁
2. **GIL**：CPU 密集型任务用 multiprocessing
3. **共享状态**：尽量避免，用消息队列通信

## 工程对照（≥100 字硬约束）

多线程在 Python 中有广泛实践：

1. **I/O 密集任务**：requests + threading 并发抓取
2. **守护线程**：后台监控、心跳等不需要等待的任务
3. **线程池**：ThreadPoolExecutor 管理线程生命周期
4. **Queue**：生产者-消费者模式解耦线程
5. **threading.Lock**：保护共享状态
6. **concurrent.futures**：简化线程/进程池管理

学完本卡能动手的事：编写并发文件下载器，使用线程池限制并发数。
