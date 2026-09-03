# 数学算法 — 工程对照笔记

## 现实应用

1. **质数判定与筛法**：RSA 加密需要大质数（2048+ bit），Miller-Rabin 概率检测 + 筛法预生成小质数表是工程标准做法。Redis 中哈希槽（hash slot）的 CRC16 运算也用到质数取模。
2. **随机数生成**：蒙特卡洛模拟（如期权定价、渲染抗锯齿）、AB 测试分组（Hash + 随机种子一致性）、UUID v4 生成。注意 `rand()` 周期仅 2^31，生产用 `arc4random` 或 `xoshiro256**`。
3. **蓄水池采样**：大数据场景下在线等概率采样——MapReduce 中的 Top-K 采样、Nginx 日志负载均衡（random two choices）、ClickHouse 中的 approximate 聚合函数。只需 O(k) 空间即可从未知大小的流中采样。
