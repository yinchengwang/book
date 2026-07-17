# python-multiprocessing scaffold

Python multiprocessing 多进程演示——Process + Pool + Queue + Pipe。

## 复现命令

```bash
cd learning/scaffold/python/python-multiprocessing
python3 main.py
```

## 预期输出

```
[1] 基础多进程:
    4 个进程并行完成，耗时: ~0.5s

[2] ProcessPoolExecutor:
    20 个任务完成，耗时: ~0.6s（4并发）
    结果前5个: [0, 1, 4, 9, 16]

[3] Pool.map/starmap:
    map 结果: [1, 4, 9, 16, 25]
    starmap 结果: [2, 4, 8, 9]

[4] Queue 通信:
    [生产] 1
    [消费] 1
    ...

[6] 共享内存:
    预期值: 40000
    实际值: 40000
    线程安全: ✓

=== PASS ===
```

## 关键点

- **Process**：创建独立进程，绕过 GIL
- **Pool**：进程池管理批量任务
- **Queue**：进程安全队列
- **Pipe**：双工管道通信
- **共享内存**：mp.Value/mp.Array 高效共享

详见 NOTES.md。
