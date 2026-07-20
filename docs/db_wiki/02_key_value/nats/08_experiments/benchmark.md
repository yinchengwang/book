# NATS 动手实验

## 学习目标

- 掌握 NATS 的部署和基本操作
- 通过实验验证 NATS 的 Pub/Sub 和 JetStream 特性

## 环境准备

```bash
# Docker 启动 NATS
docker run -d --name nats-test \
  -p 4222:4222 -p 8222:8222 \
  nats:latest

# 安装 NATS CLI
# Windows: 下载 nats.exe
# Linux: curl -sf https://bin.nats.dev/nats | bash

# 验证连接
nats server info
```

## 实验 1：Pub/Sub 基本模式

```bash
# 终端 1：订阅
nats sub "greet.>"

# 终端 2：发布
nats pub greet.hello "Hello NATS!"
nats pub greet.world "Hello World!"

# 终端 1 应收到两条消息

# 请求-回复
# 终端 1：回复
nats reply "help" "I can help you"

# 终端 2：请求
nats request "help" "I need help"
```

## 实验 2：Queue Group

```bash
# 启动 3 个消费者
nats queue sub "tasks" "workers"
nats queue sub "tasks" "workers"
nats queue sub "tasks" "workers"

# 发布任务
nats pub tasks "task1"
nats pub tasks "task2"
nats pub tasks "task3"

# 观察：每个消费者收到不同消息
# 负载均衡效果
```

## 实验 3：JetStream 持久化

```bash
# 创建 Stream
nats stream add MESSAGES \
  --subjects "msg.>" \
  --storage file \
  --retention limits \
  --max-msgs 1000 \
  --max-age 1h

# 发布消息
nats pub msg.hello "persistent message 1"
nats pub msg.hello "persistent message 2"

# 创建 Consumer
nats consumer add MESSAGES READER \
  --ack explicit \
  --deliver all \
  --max-deliver 3

# 拉取消息
nats consumer next MESSAGES READER

# 查看 Stream 状态
nats stream info MESSAGES
```

## 实验 4：监控

```bash
# 访问 HTTP 监控
# 浏览器打开 http://localhost:8222

# 查看路由
nats server list

# 查看连接
nats server connections

# 查看 Stream 统计
nats stream report
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| Pub/Sub | 订阅者收到消息 | |
| Queue Group | 消息负载均衡 | |
| JetStream | 重启后消息持久化 | |
| 监控 | 页面显示实时状态 | |

## 要点总结

- Docker 一键部署，监控端口 8222
- Queue Group 天然负载均衡
- JetStream 持久化消息
- CLI 工具完整，操作直观