# python-docker scaffold

Python 应用容器化演示——Dockerfile + docker-compose。

## 复现命令

```bash
cd learning/scaffold/python/python-docker

# 构建镜像
make build

# 启动服务
make up

# 查看日志
make logs

# 停止服务
make down
```

## 包含内容

| 文件 | 说明 |
|------|------|
| Dockerfile | 多阶段构建，生产级镜像 |
| docker-compose.yml | Web + Redis + PostgreSQL + Nginx |
| Makefile | 常用命令封装 |

## 关键点

- **多阶段构建**：减小镜像体积
- **非 root 用户**：安全最佳实践
- **健康检查**：容器健康监控
- **环境变量**：配置管理

详见 NOTES.md。
