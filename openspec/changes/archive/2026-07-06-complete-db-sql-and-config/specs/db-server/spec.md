# 数据库服务器规格

## Overview

实现数据库服务器启动、连接监听和请求处理，支持简化的 PostgreSQL wire 协议。

## Server Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      PostgreSQL Server                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │  Listener    │───▶│  Conn Mgr    │───▶│   Executor   │  │
│  │  (TCP/Unix)  │    │              │    │              │  │
│  └──────────────┘    └──────────────┘    └──────────────┘  │
│                             │                   │           │
│                             ▼                   ▼           │
│                      ┌──────────────┐    ┌──────────────┐  │
│                      │  Auth        │    │  Catalog     │  │
│                      │  Handler     │    │  Manager     │  │
│                      └──────────────┘    └──────────────┘  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Component Overview

### 1. Listener

监听客户端连接请求，支持 TCP 和 Unix Socket：

```c
typedef struct Listener_s {
    int                 port;           /**< 监听端口 */
    int                 sock;           /**< 监听 socket */
    char               *unix_socket;    /**< Unix Socket 路径 */
    struct event_base  *base;           /**< 事件循环 */
    on_connect_cb       callback;       /**< 连接回调 */
} Listener;
```

### 2. Connection Manager

管理客户端连接：

```c
typedef struct Connection_s {
    int                 fd;             /**< 客户端 socket */
    Listener           *listener;       /**< 所属监听器 */
    struct event       *event;          /**< 事件 */
    char                peer_addr[64];  /**< 对端地址 */
    uint32_t            flags;          /**< 连接标志 */
} Connection;

typedef struct ConnManager_s {
    Connection        **connections;    /**< 连接数组 */
    size_t              max_conns;      /**< 最大连接数 */
    size_t              num_conns;      /**< 当前连接数 */
    ConnectionCB       *callbacks;      /**< 回调函数 */
} ConnManager;
```

### 3. Protocol Handler

处理 PostgreSQL wire 协议（简化版）：

```c
typedef struct ProtocolHandler_s {
    Connection         *conn;           /**< 所属连接 */
    sql_executor_t     *executor;       /**< 执行器 */
    uint32_t            state;          /**< 协议状态机 */
    char               *read_buf;       /**< 读缓冲区 */
    size_t              read_len;       /**< 已读数据长度 */
    char               *write_buf;      /**< 写缓冲区 */
    size_t              write_len;      /**< 待写数据长度 */
} ProtocolHandler;
```

## Protocol States

```
┌─────────┐  startup packet  ┌──────────┐  authenticate  ┌────────────┐
│  START  │─────────────────▶│  AUTH    │───────────────▶│   IDLE     │
└─────────┘                  └──────────┘                └─────┬──────┘
                                                                │
                                                                │ query
                                                                ▼
                                                        ┌──────────────┐
                                                        │   QUERYING   │
                                                        └──────┬───────┘
                                                               │ response
                                                               ▼
                                                        ┌──────────────┐
                                                        │  IDLE        │
                                                        └──────────────┘
```

### Startup Packet

客户端发送的启动包：

```c
typedef struct {
    uint32_t    proto_version;   // 0x00030000 for 3.0
    char        params[N];       // "user\0database\0options\0\0"
} StartupPacket;
```

### Simple Query Protocol

支持基本的 SQL 查询：

```
Client → Server: Query {string="SELECT 1"}
Server → Client: RowDescription {nfields=1, fields=[{name="?column?"}]}
Server → Client: DataRow {nvalues=1, values=["1"]}
Server → Client: CommandComplete {tag="SELECT 1"}
Server → Client: ReadyForQuery {status='I'}
```

## API

### Server Lifecycle

```c
/**
 * @brief 创建服务器
 * @param options 服务器选项
 * @return 服务器句柄
 */
Server *server_create(server_options_t *options);

/**
 * @brief 启动服务器
 * @param server 服务器句柄
 * @return 0 成功
 */
int server_start(Server *server);

/**
 * @brief 停止服务器
 * @param server 服务器句柄
 * @param sig 停止信号
 */
void server_stop(Server *server, int sig);

/**
 * @brief 销毁服务器
 * @param server 服务器句柄
 */
void server_destroy(Server *server);

/**
 * @brief 主循环
 * @param server 服务器句柄
 */
void server_run(Server *server);
```

### Server Options

```c
typedef struct server_options_s {
    const char  *data_directory;     /**< 数据目录 */
    const char  *listen_addr;        /**< 监听地址 */
    int          port;               /**< 监听端口 */
    const char  *unix_socket_dir;    /**< Unix Socket 目录 */
    size_t       max_connections;    /**< 最大连接数 */
    size_t       shared_buffers;     /**< 共享内存大小 */
    const char  *config_file;        /**< 配置文件路径 */
    bool         daemon_mode;        /**< 后台运行 */
} server_options_t;
```

### pg_ctl 工具

```bash
# 启动
pg_ctl start -D /path/to/data [-l logfile]

# 停止
pg_ctl stop -D /path/to/data [-m smart|fast|immediate]

# 重载配置
pg_ctl reload -D /path/to/data

# 状态
pg_ctl status -D /path/to/data
```

## Authentication

### Supported Methods

| Method | Description |
|--------|-------------|
| trust | 无条件信任（本地默认） |
| reject | 拒绝连接 |
| md5 | MD5 密码验证 |
| password | 明文密码（不推荐） |
| peer | OS 用户名匹配 |

### pg_hba.conf Format

```
# TYPE  DATABASE  USER  ADDRESS  METHOD

local   all       all                trust
host    all       all   127.0.0.1/32 md5
host    all       all   ::1/128      md5
```

## Error Handling

### Error Response

```
ErrorResponse {
    char    'E'                 // 消息类型
    string  S = "ERROR"         // 严重级别
    string  C = "42P01"         // SQLSTATE
    string  M = "undefined_table" // 消息
    string  F = "parse_relation.c" // 文件
    int     L = 1234            // 行号
    string  R = "parserOpenTable" // 例程
    char    '\0'                // 消息结束
    char    '\0'                // 消息体结束
}
```

### Notice Response

用于非致命信息（警告等）。

## Concurrency Model

### 每个连接一个线程/事件

```
Main Thread:
    listener.accept() → Connection
                          ↓
                    Event Loop (libevent/libuv)
                          ↓
              ┌──────────┼──────────┐
              ▼          ▼          ▼
         Query      Query      Query
         Exec       Exec       Exec
```

## Acceptance Criteria

- [ ] `server_start()` 绑定并监听端口
- [ ] 多个客户端可同时连接
- [ ] `psql -h localhost -p 5432` 能连接
- [ ] `SELECT 1` 返回正确结果
- [ ] `CREATE TABLE` 执行成功
- [ ] `INSERT/SELECT/UPDATE/DELETE` 正常工作
- [ ] 错误返回正确的 ErrorResponse
- [ ] `pg_ctl stop` 能优雅停止服务器
- [ ] `pg_ctl reload` 能重新加载配置
- [ ] 连接数超过 max_connections 时拒绝新连接
- [ ] 客户端断开后正确清理资源
