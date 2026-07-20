# NATS 学习资源与项目关联

## 学习目标

- 收集 NATS 学习的优质资源
- 分析 NATS 设计对项目的启发

## 官方资源

- **GitHub**：[nats-io/nats-server](https://github.com/nats-io/nats-server)
- **官方文档**：[docs.nats.io](https://docs.nats.io/)
- **JetStream 指南**：[docs.nats.io/nats-concepts/jetstream](https://docs.nats.io/nats-concepts/jetstream)
- **NATS 设计**：[nats.io/documentation](https://nats.io/documentation/)

## 源码阅读路径

```
nats-server/
├── server/           # 服务端核心
│   ├── nats.go       # 主逻辑
│   ├── client/       # 客户端连接管理
│   ├── jetstream/    # JetStream 实现
│   └── routes/       # 集群路由
├── internal/
│   ├── server/       # 内部服务器
│   └── msg/          # 消息处理
├── conf/             # 配置解析
├── logger/           # 日志
└── util/             # 工具
```

## 项目关联

NATS 对项目消息队列实现的启发：

| NATS 设计 | 项目借鉴 |
|-----------|---------|
| 主题层级 | 订阅匹配算法 |
| Queue Group | 负载均衡模型 |
| JetStream | 持久化消息 |
| Gossip 集群 | 集群管理 |

## 要点总结

- Go 实现，代码质量高
- 核心 NATS 极致轻量
- JetStream 提供完整消息队列能力
- 适合微服务和 IoT 场景