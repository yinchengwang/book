# python-threading scaffold

Python threading 多线程演示——Thread + Lock + Event + ThreadPoolExecutor。

## 复现命令

```bash
cd learning/scaffold/python/python-threading
python3 main.py
```

## 预期输出

```
[1] 基础多线程:
    [Worker-0] 开始工作...
    [Worker-1] 开始工作...
    [Worker-2] 开始工作...
    [Worker-2] 完成（耗时 0.3s）
    ...

[2] Lock 互斥锁:
    预期值: 40000
    实际值: 40000
    线程安全: ✓

[4] Event 交替打印:
    Ping 1
    Pong 1
    ...

[5] 生产者-消费者:
    生产: 0
    消费: 0
    ...

=== PASS ===
```

## 关键点

- **GIL**：Python GIL 限制多线程 CPU 密集型性能
- **threading.Lock**：互斥锁保护共享状态
- **threading.RLock**：可重入锁，同一线程可多次获取
- **Event**：线程间信号通知
- **ThreadPoolExecutor**：线程池管理

详见 NOTES.md。
