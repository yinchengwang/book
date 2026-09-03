# MiniVecDB 运维手册

## 概述

MiniVecDB 是一款轻量级高性能向量数据库，适用于 RAG、智能问答、推荐系统等场景。

## 部署要求

### 硬件要求

| 规格 | 最低配置 | 推荐配置 |
|------|----------|----------|
| CPU | 2 核 | 4+ 核 |
| 内存 | 1 GB | 4+ GB |
| 磁盘 | 10 GB | 50+ GB SSD |
| 网络 | 100 Mbps | 1 Gbps |

### 软件要求

- Docker 20.10+
- Docker Compose 2.0+（可选）

## 日常运维

### 启动服务

```bash
# 使用 Docker
docker run -d -p 8080:8080 minivecdb:latest

# 使用 docker-compose
docker-compose up -d
```

### 停止服务

```bash
# 优雅停止（推荐）
docker stop minivecdb

# 强制停止
docker kill minivecdb
```

### 查看状态

```bash
# 查看容器状态
docker ps | grep minivecdb

# 查看健康状态
curl http://localhost:8080/ready

# 查看指标
curl http://localhost:8080/metrics
```

### 查看日志

```bash
# 实时查看
docker logs -f minivecdb

# 最近 100 行
docker logs --tail 100 minivecdb

# 按时间过滤
docker logs --since "2024-01-01" minivecdb
```

## 性能调优

### 内存配置

```yaml
# docker-compose.yml
deploy:
  resources:
    limits:
      memory: 2G
```

### 并发配置

默认配置可处理 100 并发连接。

### 索引配置

- 维度：建议 128-1536
- 索引类型：HNSW（默认）、IVF

## 备份恢复

### 备份

```bash
# 创建备份
docker run --rm -v minivecdb_data:/data -v $(pwd):/backup \
  alpine tar czf /backup/backup_$(date +%Y%m%d).tar.gz -C /data .
```

### 恢复

```bash
# 停止服务
docker stop minivecdb

# 恢复数据
docker run --rm -v minivecdb_data:/data -v $(pwd):/backup \
  alpine tar xzf /backup/backup_20240101.tar.gz -C /data

# 重启服务
docker start minivecdb
```

## 监控告警

### Prometheus 指标

| 指标名 | 类型 | 说明 |
|--------|------|------|
| `minivecdb_vectors_total` | Gauge | 向量总数 |
| `minivecdb_collections_total` | Gauge | 集合数 |
| `minivecdb_search_duration_seconds` | Histogram | 搜索延迟 |
| `minivecdb_request_total` | Counter | 请求计数 |

### 告警规则示例

```yaml
groups:
  - name: minivecdb
    rules:
      - alert: HighSearchLatency
        expr: histogram_quantile(0.95, rate(minivecdb_search_duration_seconds_bucket[5m])) > 1
        for: 5m
        labels:
          severity: warning
```

## 故障排查

### 服务无响应

1. 检查容器状态：`docker ps`
2. 检查日志：`docker logs minivecdb`
3. 检查端口：`netstat -tlnp | grep 8080`

### 数据丢失

1. 检查数据卷：`docker volume inspect minivecdb_data`
2. 检查磁盘空间：`df -h`
3. 尝试恢复备份

### 性能下降

1. 检查资源使用：`docker stats`
2. 查看慢查询日志
3. 调整内存限制

## 安全建议

1. **网络隔离**：使用防火墙限制访问
2. **数据加密**：生产环境启用 TLS
3. **访问控制**：实施 IP 白名单
4. **定期备份**：每日自动备份

## 升级指南

```bash
# 1. 备份数据
./backup.sh

# 2. 拉取新镜像
docker pull minivecdb:latest

# 3. 停止旧容器
docker stop minivecdb

# 4. 启动新容器
docker run -d -p 8080:8080 -v minivecdb_data:/data minivecdb:latest
```

## 联系支持

如遇问题，请提交 Issue：https://github.com/your-repo/issues
