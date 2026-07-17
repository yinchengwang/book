# python-docker 学习笔记

## 概念地图

Python 容器化最佳实践：

| 阶段 | 关注点 |
|------|--------|
| 构建 | 多阶段构建、依赖缓存 |
| 运行 | 非 root、健康检查 |
| 配置 | 环境变量、密钥管理 |
| 网络 | 服务发现、负载均衡 |

## Docker 最佳实践

1. **镜像大小**：使用 alpine 变体，用多阶段构建
2. **安全性**：非 root 用户、最小权限
3. **调试**：.dockerignore、ENTRYPOINT vs CMD
4. **性能**：卷挂载、资源限制

## docker-compose 场景

| 场景 | 工具 |
|------|------|
| 开发环境 | docker-compose |
| 生产环境 | Kubernetes/Docker Swarm |
| CI/CD | Docker in Docker |

## 工程对照（≥100 字硬约束）

容器化在现代 Python 项目中不可或缺：

1. **一致性**：开发/生产环境一致
2. **隔离性**：服务间相互隔离
3. **可扩展**：容器编排支持扩缩容
4. **CI/CD**：Dockerfile 纳入流水线
5. **监控**：健康检查集成
6. **日志**：容器日志聚合

学完本卡能动手的事：将自己的 Python 项目容器化，编写 Dockerfile。
