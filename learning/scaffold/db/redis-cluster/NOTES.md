# 工程对照笔记：Redis 集群架构

## engineering/src/redis/cluster.c 对照

### 核心结构

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `ClusterNode` | `clusterNode` | 集群节点 |
| `NodeFlags` | `node->flags` | 节点标志 |
| `hash_slot()` | `keyHashSlot()` | 槽计算 |

### 关键函数对照

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `demo_hash_slot()` | `keyHashSlot()` | 槽哈希计算 |
| `demo_moved_redirect()` | `clusterRedirectClient()` | 重定向处理 |

### 工程实现要点

1. **槽分配**
   ```c
   // cluster.c
   unsigned int keyHashSlot(char *key, int keylen) {
       int s, e;
       for (s = 0; s < keylen; s++)
           if (key[s] == '{') break;
       if (s == keylen) return crc64(key, keylen) % 16384;
       // ... hash tag 处理
   }
   ```

2. **Gossip 消息**
   ```c
   // MEET/PING/PONG/FAIL 消息类型
   // 定期交换，维持集群状态
   ```

3. **故障检测**
   ```c
   // 节点间心跳超时 -> PFAIL
   // 多数节点认为 PFAIL -> FAIL
   ```

### 扩展阅读

- `src/cluster.c` - 集群核心
- `src/cluster.h` - 结构定义
- `src/connection.h` - 集群总线连接
- Redis Cluster 规范: `https://redis.io/topics/cluster-spec`
