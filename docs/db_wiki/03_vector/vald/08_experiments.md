# 动手实验: Vald 部署与使用

## 学习目标

- 能够在 K8s 环境中部署 Vald
- 掌握使用 gRPC 客户端进行检索

## K8s 部署（Helm Chart）

Vald 提供 Helm Chart 一键部署：

```bash
# 添加 Vald Helm 仓库
helm repo add vald https://vald.vda.ai/helm
helm repo update

# 部署 Vald 集群
helm install vald-cluster vald/vald-cluster \
  --set gateway.replicas=2 \
  --set agent.replicas=3 \
  --set agent.ngt.dimension=128 \
  --set agent.ngt.distance_type=l2 \
  --set index-manager.auto_indexing=true

# 检查部署状态
kubectl get pods
kubectl get valdrelease  # CRD 资源
```

**验证部署**：

```bash
# 端口转发
kubectl port-forward svc/vald-cluster-gateway 8081:8081

# 查看日志
kubectl logs -l app=vald-agent
```

## 部署 Vald Agent/Gateway（手动配置）

```yaml
# vald-agent.yaml
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: vald-agent
spec:
  replicas: 3
  selector:
    matchLabels:
      app: vald-agent
  serviceName: vald-agent
  template:
    metadata:
      labels:
        app: vald-agent
    spec:
      containers:
        - name: agent
          image: vdaas/vald-agent-ngt:latest
          ports:
            - containerPort: 8081
          env:
            - name: VALD_NGT_DIMENSION
              value: "128"
            - name: VALD_NGT_DISTANCE_TYPE
              value: "L2"
            - name: VALD_NGT_OBJECT_TYPE
              value: "Float"
          volumeMounts:
            - name: index-storage
              mountPath: /var/vald/index
  volumeClaimTemplates:
    - metadata:
        name: index-storage
      spec:
        accessModes: ["ReadWriteOnce"]
        resources:
          requests:
            storage: 10Gi
```

```yaml
# vald-gateway.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: vald-gateway
spec:
  replicas: 2
  selector:
    matchLabels:
      app: vald-gateway
  template:
    metadata:
      labels:
        app: vald-gateway
    spec:
      containers:
        - name: gateway
          image: vdaas/vald-gateway:latest
          ports:
            - containerPort: 8081
          env:
            - name: VALD_GATEWAY_AGENT_ADDRESSES
              value: "vald-agent-0.vald-agent:8081,vald-agent-1.vald-agent:8081"
```

```bash
# 应用配置
kubectl apply -f vald-agent.yaml
kubectl apply -f vald-gateway.yaml

# 暴露服务
kubectl expose deployment vald-gateway --port=8081 --target-port=8081
```

## 使用 gRPC 客户端进行检索

Vald 提供多种语言的 gRPC SDK：

```bash
# Python SDK
pip install vald-client-python
```

```python
import numpy as np
from vald_client_python import Client, InsertRequest, SearchRequest, ObjectID

# 1. 连接
client = Client("localhost:8081", insecure=True)

# 2. 插入向量
for i in range(1000):
    vec = np.random.rand(128).astype(np.float32).tolist()
    req = InsertRequest(
        vector=ObjectID(
            id=str(i),
            vector=vec
        )
    )
    client.insert(req)

# 3. 搜索
query = np.random.rand(128).astype(np.float32).tolist()
search_req = SearchRequest(
    vector=query,
    config=SearchConfig(
        num=10,           # Top-K
        radius=-1,        # 搜索半径（-1 表示不限）
        epsilon=0.1       # 精度控制
    )
)

results = client.search(search_req)

# 4. 输出结果
for result in results.results:
    print(f"ID: {result.id}, Distance: {result.distance}")

# 5. 删除向量
client.remove(ObjectID(id="0"))

# 6. 获取索引统计信息
stats = client.index_info()
print(f"Index size: {stats.index_size}")
print(f"Vector count: {stats.vector_count}")
```

## 自动备份实验

```yaml
# 配置自动备份
apiVersion: vald.vda.ai/v1
kind: BackupConfig
metadata:
  name: vald-backup
spec:
  enabled: true
  schedule: "*/30 * * * *"  # 每 30 分钟
  storage:
    type: S3
    endpoint: s3.amazonaws.com
    bucket: vald-backups
    path: /vald-cluster
  retention:
    keep_latest: 10
    max_age: "72h"
```

```bash
# 应用备份配置
kubectl apply -f backup-config.yaml

# 手动触发备份
kubectl exec -it vald-cluster-backup-manager -- backup

# 查看备份列表
kubectl exec -it vald-cluster-backup-manager -- list-backups

# 从备份恢复
kubectl exec -it vald-cluster-backup-manager -- restore --backup-id=20250720-103000
```

## 索引管理操作

```python
# 查看索引状态
index_info = client.index_info()
print(f"Total vectors: {index_info.total_vector_count}")
print(f"Indexed vectors: {index_info.indexed_vector_count}")
print(f"Unindexed vectors: {index_info.unindexed_vector_count}")

# 手动触发索引构建
client.create_index()

# 保存索引
client.save_index()

# 获取 Agent 状态
agent_stats = client.agent_stats()
for agent in agent_stats.agents:
    print(f"Agent: {agent.name}, Status: {agent.status}")
```

## 要点总结

- Helm Chart 一键部署 Vald 到 K8s 集群
- StatefulSet 管理有状态 Agent，Deployment 管理无状态 Gateway
- gRPC 客户端支持插入、搜索、删除等基本操作
- 自动备份通过 CronJob 实现，支持 S3 对象存储
- 索引管理支持手动触发构建和保存

## 思考题

1. 实验中插入 1000 条数据和 100 万条数据，搜索延迟的差异有多大？
2. 如果 Agent 从 3 个缩容到 2 个，索引数据如何迁移？
3. 备份到 S3 和备份到本地 PVC，在恢复速度和可靠性上有什么区别？