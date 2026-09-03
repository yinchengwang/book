# Gap#10 生态接口系统设计

> **日期:** 2026-09-03
> **状态:** 待批准

## 1. 目标

实现完整的生态接口系统，支持：
- REST API (HTTP/REST 接口)
- Drivers (Python/Java/Go 客户端驱动)
- Plugins (可扩展插件框架)
- gRPC (高性能 RPC 接口)

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                    Ecosystem Interface                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ REST API │  │ Drivers  │  │ Plugins  │  │  gRPC   │  │
│  │ HTTP接口 │  │ SDK客户端│  │ 插件系统 │  │ 高性能RPC│  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## 3. Phase 1: REST API

### 3.1 API 端点

```
POST   /api/v1/query          - 执行查询
GET    /api/v1/tables         - 列出表
GET    /api/v1/tables/{name}  - 获取表信息
POST   /api/v1/tables         - 创建表
DELETE /api/v1/tables/{name}  - 删除表
GET    /api/v1/indexes        - 列出索引
POST   /api/v1/indexes        - 创建索引
GET    /api/v1/shards         - 分片状态
GET    /api/v1/health         - 健康检查
```

### 3.2 接口定义

```c
typedef struct rest_server rest_server_t;
typedef struct rest_request rest_request_t;
typedef struct rest_response rest_response_t;

rest_server_t *rest_server_create(int port);
int rest_server_start(rest_server_t *srv);
void rest_server_stop(rest_server_t *srv);
void rest_server_destroy(rest_server_t *srv);

/**
 * @brief 注册路由处理函数
 */
int rest_register_handler(rest_server_t *srv, const char *method,
                        const char *path, rest_handler_t handler, void *arg);

/**
 * @brief 处理请求
 */
rest_response_t *rest_handle(rest_server_t *srv, const rest_request_t *req);
```

## 4. Phase 2: Drivers (SDK 客户端)

### 4.1 Python Driver

```python
# python/mmdb/driver.py
class MMDBClient:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
    
    def query(self, sql: str) -> list:
        """执行 SQL 查询"""
        
    def execute(self, sql: str) -> int:
        """执行 SQL 语句"""
    
    def get_tables(self) -> list:
        """获取表列表"""
```

### 4.2 Java Driver

```java
// java/src/main/java/com/mmdb/Driver.java
public class MMDBDriver implements Driver {
    public Connection connect(String url, Properties info);
    public boolean acceptsURL(String url);
}
```

### 4.3 Go Driver

```go
// go/driver.go
type Driver struct{}
func (d *Driver) Open(name string) (*Conn, error)
type Conn struct{}
func (c *Conn) Query(query string) (*Rows, error)
```

## 5. Phase 3: Plugins (插件系统)

### 5.1 插件接口

```c
#define PLUGIN_API_VERSION 1

typedef enum {
    PLUGIN_TYPE_STORAGE = 0,
    PLUGIN_TYPE_INDEX,
    PLUGIN_TYPE_AUTH,
    PLUGIN_TYPE_UDF
} plugin_type_t;

typedef struct plugin {
    int api_version;
    plugin_type_t type;
    const char *name;
    const char *version;
    
    /* 生命周期 */
    int (*init)(void);
    int (*start)(void);
    void (*stop)(void);
    void (*destroy)(void);
    
    /* 插件特定数据 */
    void *data;
} plugin_t;

typedef struct plugin_manager {
    plugin_t **plugins;
    int count;
    char plugin_dir[256];
} plugin_manager_t;
```

### 5.2 插件 API

```c
plugin_manager_t *plugin_manager_create(const char *plugin_dir);
void plugin_manager_destroy(plugin_manager_t *mgr);

int plugin_manager_load(plugin_manager_t *mgr, const char *name);
int plugin_manager_unload(plugin_manager_t *mgr, const char *name);
int plugin_manager_list(plugin_manager_t *mgr, plugin_t **results, int max_count);
```

## 6. Phase 4: gRPC

### 6.1 Proto 定义

```protobuf
syntax = "proto3";

package mmdb;

service MMDBService {
    rpc Query(QueryRequest) returns (QueryResponse);
    rpc Execute(ExecuteRequest) returns (ExecuteResponse);
    rpc GetTables(Empty) returns (TableList);
    rpc CreateTable(CreateTableRequest) returns (StatusResponse);
}

message QueryRequest {
    string sql = 1;
    map<string, string> params = 2;
}

message QueryResponse {
    repeated ColumnMeta columns = 1;
    repeated Row rows = 2;
}
```

### 6.2 C gRPC 服务器

```c
typedef struct grpc_server grpc_server_t;

grpc_server_t *grpc_server_create(int port);
int grpc_server_start(grpc_server_t *srv);
void grpc_server_stop(grpc_server_t *srv);
void grpc_server_destroy(grpc_server_t *srv);
```

## 7. 文件结构

```
engineering/
├── include/db/ecosystem/
│   ├── rest_server.h           # REST API
│   ├── plugin_manager.h        # 插件系统
│   └── grpc_server.h          # gRPC
├── src/db/ecosystem/
│   ├── rest_server.c
│   ├── plugin_manager.c
│   └── grpc_server.c
├── drivers/
│   ├── python/                 # Python driver
│   ├── java/                  # Java driver
│   └── go/                    # Go driver
├── proto/
│   └── mmdb.proto            # gRPC proto
└── test/db/ecosystem/
    └── ecosystem_test.cpp
```

## 8. 实现顺序

| Phase | 内容 | 依赖 |
|-------|------|------|
| 1 | REST API | 无 |
| 2 | Drivers | 无 |
| 3 | Plugins | 无 |
| 4 | gRPC | 无 |

## 9. 成功标准

- [ ] REST API: HTTP 接口可工作
- [ ] Drivers: Python/Java/Go 驱动可连接
- [ ] Plugins: 插件可加载/卸载
- [ ] gRPC: 高性能 RPC 接口可工作
- [ ] 单元测试覆盖
