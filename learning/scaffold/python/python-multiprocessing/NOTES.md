# python-multiprocessing 学习笔记

## 概念地图

multiprocessing 实现 Python 多进程并行：

- **Process**：独立进程，有独立 GIL
- **Pool**：进程池，复用进程执行任务
- **Queue/Pipe**：进程间通信
- **共享内存**：绕过进程隔离，高效数据共享
- **Manager**：共享 dict/list 等复杂数据结构

## multiprocessing vs threading

| 特性 | multiprocessing | threading |
|------|-----------------|-----------|
| 并行 | 真并行（多核） | GIL 限制 |
| 内存 | 独立内存空间 | 共享内存 |
| 创建开销 | 较大 | 较小 |
| 适用 | CPU 密集 | I/O 密集 |

## 踩坑记录

1. **pickle 序列化**：进程间通信需要可序列化对象
2. **fork vs spawn**：Windows 只有 spawn，Linux 默认 fork
3. **资源竞争**：共享内存需要锁保护

## 工程对照（≥100 字硬约束）

多进程在 Python 中用于 CPU 密集型任务：

1. **科学计算**：NumPy/SciPy + multiprocessing
2. **数据处理**：pandas 配合多进程加速
3. **图像处理**：PIL/OpenCV 多进程批处理
4. **爬虫**：多进程并发抓取
5. **ML 训练**：joblib 多核并行
6. **服务器**：多进程监听不同端口

学完本卡能动手的事：用 ProcessPoolExecutor 重构 CPU 密集的数据处理脚本。
