# 工程对照笔记：Redis 持久化

## engineering/src/redis/persist.c 对照

### 核心结构

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `RdbInfo` | `server` (部分) | RDB 状态 |
| `AofInfo` | `server.aof_state` | AOF 状态 |
| `PersistMode` | `server.saveparams` | 持久化配置 |

### 关键函数对照

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `demo_rdb_save()` | `rdbSave()` | RDB 保存 |
| `demo_aof_write()` | `aofRewriteBufferWrite()` | AOF 追加 |
| `demo_aof_rewrite()` | `rewriteAppendOnlyFile()` | AOF 重写 |

### 工程实现要点

1. **RDB 保存**
   ```c
   // rdb.c
   int rdbSave(int fd, rdbSaveInfo *rsi) {
       // 写入 MAGIC "REDIS"
       // 遍历所有数据库
       // 保存 key-value
       // 写入 checksum
   }
   ```

2. **fork 机制**
   ```c
   // server.c
   pid = fork();
   if (pid == 0) {
       // 子进程: rdbSave()
       exit(0);
   } else {
       // 父进程: 继续服务
   }
   ```

3. **AOF 重写**
   ```c
   // aof.c
   int rewriteAppendOnlyFile(const char *target) {
       // 遍历内存数据
       // 生成 SET/HSET/ZADD 命令
       // 写入新文件
   }
   ```

### 扩展阅读

- `src/rdb.h` - RDB 格式定义
- `src/rdb.c` - RDB 实现
- `src/aof.c` - AOF 实现
- `src/server.c` - fork 调用
