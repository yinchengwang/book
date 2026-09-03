# MiniVecDB Docker 部署指南

## 快速开始

### 1. 构建镜像

```bash
# 构建 Docker 镜像
docker build -t minivecdb:latest .

# 或者使用 docker-compose
docker-compose up -d
```

### 2. 运行容器

```bash
# 后台运行
docker run -d \
  --name minivecdb \
  -p 8080:8080 \
  -v minivecdb_data:/data \
  minivecdb:latest

# 前台运行（查看日志）
docker run -it --rm \
  -p 8080:8080 \
  -v $(pwd)/data:/data \
  minivecdb:latest
```

### 3. 验证部署

```bash
# 健康检查
curl http://localhost:8080/health

# 创建集合
curl -X POST http://localhost:8080/collections \
  -H "Content-Type: application/json" \
  -d '{"name":"test","dimension":128}'

# 列出集合
curl http://localhost:8080/collections
```

## 配置

### 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `MINIVECDB_DATA_DIR` | `/data` | 数据存储目录 |
| `MINIVECDB_LOG_LEVEL` | `info` | 日志级别 (debug/info/warn/error) |
| `MINIVECDB_HOST` | `0.0.0.0` | 监听地址 |
| `MINIVECDB_PORT` | `8080` | 监听端口 |

### docker-compose.yml 示例

```yaml
version: '3.8'
services:
  minivecdb:
    image: minivecdb:latest
    container_name: minivecdb
    restart: unless-stopped
    ports:
      - "8080:8080"
    volumes:
      - minivecdb_data:/data
    environment:
      - MINIVECDB_LOG_LEVEL=info
    healthcheck:
      test: ["CMD", "wget", "--spider", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
volumes:
  minivecdb_data:
```

## 数据持久化

数据存储在 Docker 卷 `minivecdb_data` 中：

```bash
# 查看卷位置
docker volume inspect minivecdb_data

# 备份数据
docker run --rm -v minivecdb_data:/data -v $(pwd):/backup alpine tar czf /backup/minivecdb_backup.tar.gz -C /data .

# 恢复数据
docker run --rm -v minivecdb_data:/data -v $(pwd):/backup alpine tar xzf /backup/minivecdb_backup.tar.gz -C /data
```

## 健康检查

容器内健康检查：

```bash
# 就绪检查
curl http://localhost:8080/ready

# 活跃检查
curl http://localhost:8080/live

# Prometheus 指标
curl http://localhost:8080/metrics
```

## 日志

查看容器日志：

```bash
# 实时日志
docker logs -f minivecdb

# 最近 100 行
docker logs --tail 100 minivecdb

# 错误日志
docker logs minivecdb 2>&1 | grep ERROR
```

## 资源限制

建议生产环境配置：

```yaml
deploy:
  resources:
    limits:
      memory: 1G
      cpus: '1'
    reservations:
      memory: 256M
```

## 停止和清理

```bash
# 停止容器
docker stop minivecdb

# 删除容器
docker rm minivecdb

# 删除镜像
docker rmi minivecdb:latest

# 删除数据卷（谨慎！）
docker volume rm minivecdb_data
```
