# 工程对照笔记：Redis RESP 协议

## engineering/src/redis/resp.c 对照

### 核心结构

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `RespValue` | `redisObject` | RESP 值对象 |
| `RespParser` | `client` (部分) | 解析器上下文 |
| `resp_type` | `type` | 值类型 |

### 关键函数对照

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `parse_simple_string()` | `processInlineBuffer()` | 解析内联命令 |
| `parse_bulk_string()` | `readQueryFromClient()` | 解析 RESP |
| `encode_command()` | `sendCommandToSubstrate()` | 发送命令 |
| `resp_create_value()` | `createObject()` | 创建对象 |

### 工程实现要点

1. **解析器状态机**
   ```c
   // resp.c 中的核心解析
   typedef struct redisClient {
       int fd;                  // 客户端 fd
       sds querybuf;            // 查询缓冲区
       int argc;                // 参数个数
       robj **argv;             // 参数数组
       // ...
   } client;
   ```

2. **Bulk String 处理**
   ```c
   // 读取 bulk string 长度
   if (c->querybuf[0] == '$') {
       // 解析长度前缀
       n = strtol(c->querybuf+1, NULL, 10);
   }
   ```

3. **命令执行流程**
   ```
   readQueryFromClient()
   -> processInputBuffer()
   -> processCommand()
   -> call()
   -> addReply()
   ```

4. **客户端状态**
   ```c
   typedef struct client {
       uint64_t id;             // 唯一 ID
       int fd;                   // 文件描述符
       sds querybuf;             // 查询缓冲区
       int argc;                 // 参数个数
       robj **argv;              // 参数
       int cmd;                  // 命令类型
   } client;
   ```

### 行数对比

| 组件 | 学习代码 | 工程代码（约）|
|------|---------|--------------|
| 协议定义 | ~100 行 | ~200 行 |
| 解析器 | ~150 行 | ~400 行 |
| 编码器 | ~50 行 | ~150 行 |

### 扩展阅读

- Redis 源码：`src/networking.c` - 网络 I/O
- `src/server.h` - client 结构定义
- RESP2 vs RESP3：后者支持更多数据类型
- Redis 协议规范：`https://redis.io/topics/protocol`
