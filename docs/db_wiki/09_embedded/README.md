# 嵌入式数据库横向对比

## 分类概述

嵌入式数据库是无需独立服务器进程，直接集成到应用程序中的轻量级数据库。它们以内嵌库形式存在，适合移动应用、桌面软件、IoT 设备、浏览器等场景，提供可靠的本地存储能力，零运维是最大优势。

## 库一览

- **RocksDB** - Facebook 高性能嵌入式 KV 存储，LSM-Tree，企业级
- **LevelDB** - Google 轻量级嵌入式 KV，LSM-Tree，经典实现
- **Badger** - Dgraph 高性能嵌入式 KV，纯 Go 实现，LSM + Value Log

## 功能对比表

| 维度 | RocksDB | LevelDB | Badger |
|------|---------|---------|--------|
| 编程语言 | C++ | C++ | Go |
| 开源协议 | Apache 2.0 | BSD | Apache 2.0 |
| 首次发布 | 2012 | 2011 | 2017 |
| GitHub Stars | 30k+ | 36k+ | 14k+ |
| LSM 变种 | 原生 LSM | 经典 LSM | LSM + Value Log |
| 并发支持 | 原生支持 | 有限(单写) | 原生支持 |
| 压缩 | LZ4/Snappy/ZSTD | Snappy | ZSTD |
| 迭代器 | 支持 | 支持 | 支持 |
| Go 生态 | CGO 绑定 | CGO 绑定 | 原生支持 |
| C++ 生态 | 原生 | 原生 | 有限 |
| 性能特点 | 高吞吐写入 | 简单高效 | 读优化 |
| 一句话定位 | 企业级嵌入式 KV 存储 | 经典轻量 KV 引擎 | Go 原生高性能 KV |

## 选型指南

- **生产级嵌入式存储**：推荐 RocksDB（Facebook 生产验证）
- **简单 Go 项目**：推荐 Badger（原生 Go、生态好）
- **学习 LSM-Tree 原理**：推荐 LevelDB（代码简洁）
- **C++ 项目高性能存储**：推荐 RocksDB（性能最优）

## 学习路径

1. **先学 LevelDB** — LSM-Tree 经典实现，代码量小
2. **再学 Badger** — 理解 LSM + Value Log 优化
3. **最后学 RocksDB** — 企业级实现，参数丰富
4. **理解 LSM-Tree 原理** — compaction、写入路径、读取路径

## 关联项目

- `db/storage/` — 项目存储引擎，可参考 RocksDB 架构
- `reference/open-source/` — RocksDB/LevelDB 源码参考
