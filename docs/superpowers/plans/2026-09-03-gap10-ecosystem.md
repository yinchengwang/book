# Gap#10 生态接口系统实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现完整的生态接口系统（REST API + Drivers + Plugins + gRPC）

**Architecture:** 四个独立子模块：REST/Drivers/Plugins/gRPC

**Tech Stack:** C 语言（核心）+ Python/Java/Go（驱动）+ protobuf（gRPC）

## Global Constraints

- 遵循现有代码风格 (extern "C"、命名下划线分隔)
- REST API 和 Plugins 使用 C 实现
- Drivers 使用各语言原生实现
- gRPC 使用 protobuf

---

### Task 1: REST API

**Files:**
- Create: `engineering/include/db/ecosystem/rest_server.h`
- Create: `engineering/src/db/ecosystem/rest_server.c`
- Create: `engineering/src/db/ecosystem/CMakeLists.txt`

- [ ] **Step 1: 创建 REST API**

```c
// rest_server.h
typedef struct rest_server rest_server_t;

rest_server_t *rest_server_create(int port);
int rest_server_start(rest_server_t *srv);
void rest_server_stop(rest_server_t *srv);
void rest_server_destroy(rest_server_t *srv);
```

- [ ] **Step 2: 提交**

---

### Task 2: Python Driver

**Files:**
- Create: `drivers/python/mmdb/__init__.py`
- Create: `drivers/python/mmdb/driver.py`

- [ ] **Step 1: 创建 Python Driver**

```python
class MMDBClient:
    def __init__(self, host: str, port: int = 8080):
        self.host = host
        self.port = port
    
    def query(self, sql: str) -> list:
        pass
    
    def execute(self, sql: str) -> int:
        pass
```

- [ ] **Step 2: 提交**

---

### Task 3: Java Driver

**Files:**
- Create: `drivers/java/src/main/java/com/mmdb/Driver.java`
- Create: `drivers/java/src/main/java/com/mmdb/Connection.java`

- [ ] **Step 1: 创建 Java Driver**

```java
public class MMDBDriver implements Driver {
    public Connection connect(String url, Properties info) {
        // Parse URL and create connection
    }
}
```

- [ ] **Step 2: 提交**

---

### Task 4: Go Driver

**Files:**
- Create: `drivers/go/driver.go`
- Create: `drivers/go/conn.go`

- [ ] **Step 1: 创建 Go Driver**

```go
type Driver struct{}
func (d *Driver) Open(name string) (*Conn, error)
type Conn struct{}
func (c *Conn) Query(query string) (*Rows, error)
```

- [ ] **Step 2: 提交**

---

### Task 5: Plugin System

**Files:**
- Create: `engineering/include/db/ecosystem/plugin_manager.h`
- Create: `engineering/src/db/ecosystem/plugin_manager.c`

- [ ] **Step 1: 创建 Plugin API**

```c
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
    int (*init)(void);
    int (*start)(void);
    void (*stop)(void);
} plugin_t;

plugin_manager_t *plugin_manager_create(const char *plugin_dir);
int plugin_manager_load(plugin_manager_t *mgr, const char *name);
int plugin_manager_unload(plugin_manager_t *mgr, const char *name);
```

- [ ] **Step 2: 提交**

---

### Task 6: gRPC

**Files:**
- Create: `proto/mmdb.proto`
- Create: `engineering/src/db/ecosystem/grpc_server.c`

- [ ] **Step 1: 创建 Proto 定义**

```protobuf
syntax = "proto3";
package mmdb;
service MMDBService {
    rpc Query(QueryRequest) returns (QueryResponse);
    rpc Execute(ExecuteRequest) returns (ExecuteResponse);
}
```

- [ ] **Step 2: 提交**

---

## 任务依赖关系

```
Task 1: REST API        ← 无
Task 2: Python Driver   ← 无
Task 3: Java Driver     ← 无
Task 4: Go Driver       ← 无
Task 5: Plugins        ← 无
Task 6: gRPC           ← 无
```

## 成功标准

- [ ] Task 1: REST API HTTP 接口
- [ ] Task 2: Python 驱动
- [ ] Task 3: Java 驱动
- [ ] Task 4: Go 驱动
- [ ] Task 5: 插件系统
- [ ] Task 6: gRPC 接口
