# python-concurrency 学习笔记

## 并发方式对比

| 方式 | 适用场景 | GIL | 内存 | 创建开销 |
|------|----------|-----|------|----------|
| threading | I/O 密集 | 限制 | 共享 | 小 |
| multiprocessing | CPU 密集 | 无 | 独立 | 大 |
| asyncio | 高并发 I/O | 无 | 共享 | 小 |

## 选择指南

1. **网络请求/文件 I/O** → threading 或 asyncio
2. **科学计算/图像处理** → multiprocessing
3. **Web 服务器** → asyncio（FastAPI）
4. **爬虫** → threading 或 asyncio

## 工程对照（≥100 字硬约束）

并发编程在实际项目中常见场景：

1. **Web 爬虫**：threading/asyncio 并发抓取
2. **数据处理**：multiprocessing 并行计算
3. **API 服务器**：asyncio 处理高并发
4. **批量任务**：concurrent.futures 简化管理
5. **后台任务**：threading 执行定时任务
6. **实时系统**：asyncio + websockets

学完本卡能动手的事：根据任务类型选择合适的并发方式。
