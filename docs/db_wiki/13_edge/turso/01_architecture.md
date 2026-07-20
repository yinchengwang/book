# Turso 架构

## 学习目标
- 理解 libSQL 分支与 SQLite 的关系
- 掌握嵌入式副本（Embedded Replica）的工作原理
- 理解 sqld 服务器的架构设计
- 了解主从同步机制

## 核心架构概览

```mermaid
graph TB
    subgraph "边缘节点"
        ER[嵌入式副本<br/>libSQL Embedded Replica]
        ER --> |本地读取| APP[应用程序]
        ER --> |写入转发| SQLD
    end
    
    subgraph "服务端"
        SQLD[sqld 服务器<br/>HTTP/WebSocket]
        SQLD --> PRIMARY[(主库 Primary)]
        SQLD --> REPLICA[(副本 Replica)]
    end
    
    subgraph "客户端"
        CLI[Turso CLI]
        SDK[Serverless SDK<br/>JS/Rust/Go/Python]
    end
    
    CLI --> SQLD
    SDK --> SQLD
    SDK --> ER
    
    style ER fill:#e1f5fe
    style SQLD fill:#fff3e0
    style PRIMARY fill:#e8f5e9
```

## libSQL 分支架构

### 1. libSQL 与 SQLite 的关系

| 特性 | SQLite | libSQL |
|------|--------|--------|
| 存储格式 | 单文件 | 单文件 + WAL |
| 网络协议 | 无 | HTTP/WebSocket |
| 向量扩展 | 需手动编译 | 内置 vector 类型 |
| ALTER TABLE | 有限 | 增强版 |
| JSON 支持 | JSON1 扩展 | 内置 JSON 模式 |
| 复制 | 无 | 嵌入式副本 |

### 2. libSQL 扩展架构

```mermaid
graph LR
    subgraph "libSQL 核心扩展"
        VEC[向量类型<br/>vector32/vector64]
        ALTER[增强 ALTER TABLE]
        JSON[JSON 模式验证]
        HTTP[HTTP 协议层]
    end
    
    subgraph "存储层"
        BTREE[B-Tree 页面]
        WAL[Write-Ahead Log]
        PAGE[页面缓存]
    end
    
    VEC --> BTREE
    ALTER --> BTREE
    JSON --> BTREE
    HTTP --> WAL
    
    style VEC fill:#f3e5f5
    style ALTER fill:#e8f5e9
    style JSON fill:#fff3e0
    style HTTP fill:#e1f5fe
```

## 嵌入式副本（Embedded Replica）

### 1. 工作原理

```mermaid
sequenceDiagram
    participant App as 应用程序
    participant ER as 嵌入式副本
    participant SQLD as sqld 服务器
    
    Note over App,SQLD: 启动阶段
    App->>ER: 初始化副本
    ER->>SQLD: 请求全量同步
    SQLD-->>ER: 返回数据库文件
    ER->>ER: 写入本地文件
    
    Note over App,SQLD: 运行阶段 - 读取
    App->>ER: SELECT 查询
    ER-->>App: 本地返回结果
    
    Note over App,SQLD: 运行阶段 - 写入
    App->>ER: INSERT/UPDATE
    ER->>SQLD: 转发写入请求
    SQLD-->>ER: 确认 + 新位置
    ER->>ER: 异步应用更新
    
    Note over App,SQLD: 后台同步
    SQLD->>ER: 推送新事务
    ER->>ER: 应用到本地
```

### 2. 同步策略

```c
// 嵌入式副本同步模式
typedef enum {
    SYNC_MODE_REALTIME,   // 实时同步（WebSocket）
    SYNC_MODE_PERIODIC,   // 周期同步（可配置间隔）
    SYNC_MODE_MANUAL      // 手动触发
} sync_mode_t;

// 副本状态
typedef struct embedded_replica {
    char *db_path;           // 本地数据库路径
    char *remote_url;        // 远程服务器 URL
    uint64_t last_frame;     // 最后同步的帧号
    sync_mode_t sync_mode;   // 同步模式
    uint32_t sync_interval;  // 同步间隔（毫秒）
    bool is_syncing;         // 正在同步标志
} embedded_replica_t;
```

## sqld 服务器架构

### 1. 服务组件

```mermaid
graph TB
    subgraph "sqld 服务器"
        HTTP[HTTP 端点<br/>:8080]
        WS[WebSocket 端点<br/>:8080/ws]
        AUTH[认证层<br/>JWT Token]
        ROUTER[请求路由]
        
        subgraph "执行引擎"
            PARSE[SQL 解析]
            OPT[查询优化]
            EXEC[执行器]
        end
        
        STORE[存储管理器]
        REPL[复制管理器]
    end
    
    HTTP --> AUTH
    WS --> AUTH
    AUTH --> ROUTER
    ROUTER --> PARSE
    PARSE --> OPT
    OPT --> EXEC
    EXEC --> STORE
    EXEC --> REPL
    
    style HTTP fill:#e1f5fe
    style WS fill:#e1f5fe
    style STORE fill:#e8f5e9
    style REPL fill:#fff3e0
```

### 2. HTTP 协议

```bash
# 执行 SQL
curl -X POST https://db.example.com/sql \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "statements": [
      "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)",
      "INSERT INTO users VALUES (1, '\''Alice'\'')"
    ]
  }'

# 查询结果
{
  "results": [
    {"rows_affected": 0},
    {"last_insert_rowid": 1}
  ]
}
```

### 3. WebSocket 实时订阅

```javascript
// WebSocket 连接
const ws = new WebSocket('wss://db.example.com/ws?token=xxx');

ws.onopen = () => {
  // 订阅表变更
  ws.send(JSON.stringify({
    type: 'subscribe',
    table: 'users'
  }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('表变更:', data);
  // { type: 'change', table: 'users', operation: 'INSERT', row: {...} }
};
```

## 主从架构

### 1. Primary-Replica 复制

```mermaid
graph LR
    subgraph "Primary 节点"
        PWRITE[写入请求]
        PWAL[WAL 日志]
        PDATA[(数据库文件)]
    end
    
    subgraph "Replica 节点"
        RWAL[WAL 日志]
        RDATA[(数据库文件)]
    end
    
    PWRITE --> PWAL
    PWAL --> PDATA
    
    PWAL --> |异步复制| RWAL
    RWAL --> RDATA
    
    style PWRITE fill:#e8f5e9
    style RWAL fill:#fff3e0
```

### 2. 复制流程

```c
// 复制帧结构
typedef struct replication_frame {
    uint32_t frame_no;       // 帧序号
    uint32_t page_no;        // 页面号
    uint32_t db_size;        // 数据库大小
    uint8_t page_data[4096]; // 页面数据（4KB）
} replication_frame_t;

// 复制状态
typedef struct replica_state {
    uint64_t current_frame;  // 当前帧号
    uint64_t applied_frame;  // 已应用帧号
    uint32_t lag_ms;         // 延迟毫秒
    bool is_syncing;         // 同步中
} replica_state_t;
```

## 分支系统（Branching）

### 1. 分支架构

```mermaid
gitGraph
    commit id: "初始数据"
    commit id: "迁移 V1"
    branch feature/user-table
    commit id: "添加 users 表"
    commit id: "插入测试数据"
    checkout main
    commit id: "生产更新"
    merge feature/user-table id: "合并功能"
```

### 2. 分支操作

```bash
# 创建分支
turso db branch create my-db feature-branch

# 切换分支
turso db branch switch my-db feature-branch

# 查看分支
turso db branch list my-db

# 合并分支（合并到 main）
turso db branch merge my-db feature-branch

# 删除分支
turso db branch delete my-db feature-branch
```

## 客户端架构

### 1. Serverless SDK 架构

```mermaid
graph TB
    subgraph "SDK 层"
        JS[JavaScript SDK]
        RS[Rust SDK]
        GO[Go SDK]
        PY[Python SDK]
    end
    
    subgraph "传输层"
        HTTP[HTTP Client]
        WS[WebSocket Client]
    end
    
    subgraph "本地优化"
        CACHE[结果缓存]
        BATCH[批量执行]
    end
    
    JS --> HTTP
    JS --> WS
    RS --> HTTP
    GO --> HTTP
    PY --> HTTP
    
    HTTP --> CACHE
    WS --> CACHE
    CACHE --> BATCH
    
    style JS fill:#f3e5f5
    style RS fill:#e8f5e9
    style GO fill:#fff3e0
    style PY fill:#e1f5fe
```

### 2. SDK 使用示例

```javascript
import { createClient } from '@libsql/client';

const db = createClient({
  url: 'libsql://my-db.turso.io',
  authToken: 'xxx'
});

// 执行查询
const result = await db.execute({
  sql: 'SELECT * FROM users WHERE id = ?',
  args: [1]
});

// 批量执行
await db.batch([
  'CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT)',
  'INSERT INTO products VALUES (1, '\''Widget'\'')',
  'INSERT INTO products VALUES (2, '\''Gadget'\'')'
]);
```

## 要点总结

- **libSQL 是 SQLite 的增强分支**：添加向量类型、增强 ALTER TABLE、HTTP 协议支持
- **嵌入式副本实现边缘分发**：本地读取 + 写入转发 + 后台同步
- **sqld 提供 HTTP/WebSocket 接口**：无驱动访问 SQLite
- **主从架构支持高可用**：WAL 异步复制
- **分支系统类似 Git**：数据库版本隔离与合并

## 思考题

1. 嵌入式副本与传统的数据库副本有何不同？离线优先场景如何保证数据一致性？
2. libSQL 的向量扩展与专用向量数据库（如 Milvus）相比，优势和劣势分别是什么？
3. Turso 的分支系统与 Dolt 的版本控制有何本质区别？