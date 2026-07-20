# Dragonfly 学习资源与项目关联

## 学习目标

- 收集 Dragonfly 学习的优质资源
- 分析 Dragonfly 设计对项目的启发

## 官方资源

- **GitHub**：[dragonflydb/dragonfly](https://github.com/dragonflydb/dragonfly)
- **官方文档**：[dragonflydb.io/docs](https://www.dragonflydb.io/docs)
- **性能报告**：[dragonflydb.io/benchmarks](https://www.dragonflydb.io/benchmarks)

## 源码阅读路径

```
src/
├── server/           # 服务端核心
├── core/             # 核心数据结构
├── io/               # IO 引擎
├── string/           # 字符串处理
├── uri/              # URI 解析
├── redis/            # Redis 协议支持
└── memcached/        # Memcached 协议支持
```

## 项目关联

Dragonfly 对项目存储引擎的启发：

| Dragonfly 设计 | 项目借鉴 |
|--------------|---------|
| Dash 哈希表 | 无锁哈希表设计 |
| 共享无锁架构 | 多线程并发访问 |
| 无 fork 快照 | 快照实现方案 |
| 透明大页 | 内存优化参考 |

## 要点总结

- 源码路径清晰，核心在 `src/server/` 和 `src/core/`
- 多线程架构突破 Redis 单线程瓶颈
- 无 fork 快照避免内存翻倍
- Dash 哈希表是无锁设计的关键