# 工程对照笔记：Redis 主从复制

## engineering/src/redis/replica.c 对照

### 核心结构

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `ReplicationLink` | `client` (slave) | 从节点连接 |
| `ReplicationInfo` | `server.repl_*` | 复制状态 |
| `ReplicationState` | `repl_state` | 复制状态机 |

### 关键函数对照

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `demo_psync_full_sync()` | `replicationFeedSlaves()` | 复制命令 |
| `demo_psync_partial_sync()` | `masterTryPartialResync()` | PSYNC 增量 |

### 工程实现要点

1. **PSYNC 命令处理**
   ```c
   // replication.c
   void masterHandlePsync(client *c) {
       if (strcasecmp(c->argv[1]->ptr, "?") == 0) {
           // 全量同步
           addReplySds(c, sdscatprintf(sdsempty(),
               "+FULLRESYNC %s %lld\r\n",
               server.master_replid,
               server.master_repl_offset));
       }
   }
   ```

2. **复制积压缓冲区**
   ```c
   // server.repl_backlog (环形缓冲区)
   // repl_backlog_size 默认 1MB
   // 保存最近的写命令
   ```

3. **从节点命令接收**
   ```c
   // call() -> propagate()
   // 主节点执行写命令后，发送给所有从节点
   ```

### 扩展阅读

- `src/replication.c` - 复制核心实现
- `src/networking.c` - 网络 I/O
- `src/server.h` - 复制相关结构
- Sentinel: `sentinel.c`
