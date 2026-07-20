# Garnet 学习资源与项目关联

## 学习目标

- 收集 Garnet 学习的优质资源
- 分析 Garnet 设计对项目的启发

## 官方资源

- **GitHub**：[microsoft/garnet](https://github.com/microsoft/garnet)
- **官方文档**：[github.com/microsoft/garnet/wiki](https://github.com/microsoft/garnet/wiki)
- **论文**：[Garnet: A High-Performance Caching Store](https://arxiv.org/abs/1234)

## 源码阅读路径

```
Garnet/
├── main/GarnetServer/    # 服务端入口
├── libs/
│   ├── host/             # 主机层
│   ├── server/           # 服务器核心
│   ├── storage/          # Tsavorite 存储
│   ├── cluster/          # 集群支持
│   └── common/           # 公共库
├── benchmark/            # 基准测试
├── test/                 # 测试
├── metrics/              # 指标
└── modules/              # 模块系统
```

## 项目关联

Garnet 对项目存储引擎的启发：

| Garnet 设计 | 项目借鉴 |
|------------|---------|
| Tsavorite 日志结构 | LSM 风格存储引擎 |
| 无锁读路径 | 并发优化 |
| .NET 零分配 | 内存池优化 |
| Span/Memory | 内存视图抽象 |

## 要点总结

- 微软研究院出品，C# 实现
- Tsavorite 是日志结构存储引擎
- 无锁读路径实现高并发
- 对 .NET 生态有特殊意义