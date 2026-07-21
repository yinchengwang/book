# FerretDB 存储引擎

## 学习目标

- 理解 FerretDB 的存储架构和数据持久化机制
- 掌握 BSON → JSONB 类型映射的设计思想
- 了解文档存储与关系存储的融合策略
- 对比 FerretDB 与项目 storage/ 模块的异同

## 核心存储架构

FerretDB 采用**协议翻译 + 后端委托**的存储架构，核心思想是"不做存储，只做翻译"，将 MongoDB 的文档操作翻译为 PostgreSQL 的 SQL 操作。

```mermaid
graph TB
    subgraph "客户端层"
        MONGO_DRV["MongoDB Driver<br/>各语言驱动"]
        MONGOSH["mongosh<br/>MongoDB Shell"]
    end

    subgraph "FerretDB 协议层"
        WIRE["Wire Protocol Handler<br/>OpMsg/OpQuery 解析"]
        CMD["命令处理器<br/>find/insert/update/delete"]
    end

    subgraph "翻译层"
        BSON_CONV["BSON → JSONB<br/>类型转换器"]
        QUERY_TRANS["查询翻译器<br/>MongoDB 查询 → SQL"]
        INDEX_MAP["索引映射器<br/>MongoDB 索引 → PG 索引"]
    end

    subgraph "PostgreSQL 存储层"
        PG_DRV["pgx 驱动<br/>Go PostgreSQL 驱动"]
        JSONB_STORE["JSONB 存储<br/>文档存储为 JSONB 行"]
        PG_IDX["索引层<br/>B-tree/GIN 索引"]
        PG_TXN["事务层<br/>ACID 事务"]
    end

    MONGO_DRV --> WIRE
    MONGOSH --> WIRE
    WIRE --> CMD
    CMD --> BSON_CONV
    CMD --> QUERY_TRANS
    CMD --> INDEX_MAP
    BSON_CONV --> PG_DRV
    QUERY_TRANS --> PG_DRV
    INDEX_MAP --> PG_DRV
    PG_DRV --> JSONB_STORE
    JSONB_STORE --> PG_IDX
    PG_IDX --> PG_TXN
```

### 存储架构特点

| 特点 | 说明 | 优势 |
|------|------|------|
| 无自有存储 | 不实现存储引擎，完全委托 PostgreSQL | 复用成熟存储，降低开发复杂度 |
| JSONB 存储 | 文档存储为 PostgreSQL 的 JSONB 类型 | 保留文档灵活性，支持嵌套和数组 |
| 协议翻译 | MongoDB Wire Protocol → PostgreSQL SQL | 应用层无需修改，透明代理 |
| 事务委托 | 利用 PostgreSQL 的 ACID 事务 | 数据一致性有保障 |

## 数据结构：BSON → JSONB 映射

### 类型映射表

FerretDB 将 MongoDB 的 BSON 类型映射为 PostgreSQL 的 JSONB 类型：

```mermaid
graph LR
    subgraph "BSON 类型（MongoDB）"
        B_DOC["Document<br/>嵌套文档"]
        B_ARR["Array<br/>数组"]
        B_STR["String<br/>UTF-8 字符串"]
        B_INT32["Int32<br/>32位整数"]
        B_INT64["Int64<br/>64位整数"]
        B_DOUBLE["Double<br/>浮点数"]
        B_BOOL["Boolean<br/>布尔值"]
        B_NULL["Null<br/>空值"]
        B_OID["ObjectId<br/>12字节对象ID"]
        B_DATE["Date<br/>日期时间"]
        B_BIN["Binary<br/>二进制数据"]
        B_TS["Timestamp<br/>时间戳"]
        B_REGEX["Regex<br/>正则表达式"]
    end

    subgraph "JSONB 类型（PostgreSQL）"
        J_OBJ["JSONB Object<br/>JSON 对象"]
        J_ARR["JSONB Array<br/>JSON 数组"]
        J_STR["JSONB String<br/>JSON 字符串"]
        J_NUM["JSONB Number<br/>JSON 数字"]
        J_BOOL["JSONB Boolean<br/>JSON 布尔"]
        J_NULL["JSONB Null<br/>JSON 空值"]
        J_STR_OID["String (24位hex)<br/>ObjectId 字符串化"]
        J_STR_DATE["String (ISO8601)<br/>日期字符串化"]
        J_OBJ_BIN["Object<br/>二进制编码对象"]
        J_OBJ_TS["Object<br/>时间戳对象"]
        J_OBJ_REGEX["Object<br/>正则对象"]
    end

    B_DOC --> J_OBJ
    B_ARR --> J_ARR
    B_STR --> J_STR
    B_INT32 --> J_NUM
    B_INT64 --> J_NUM
    B_DOUBLE --> J_NUM
    B_BOOL --> J_BOOL
    B_NULL --> J_NULL
    B_OID --> J_STR_OID
    B_DATE --> J_STR_DATE
    B_BIN --> J_OBJ_BIN
    B_TS --> J_OBJ_TS
    B_REGEX --> J_OBJ_REGEX
```

### 详细映射规则

| BSON 类型 | JSONB 映射 | 存储示例 | 说明 |
|-----------|------------|----------|------|
| Document | JSONB Object | `{"name": "foo"}` | 直接映射，保留结构 |
| Array | JSONB Array | `[1, 2, 3]` | 直接映射，支持嵌套 |
| String | JSONB String | `"hello"` | UTF-8 编码 |
| Int32/Int64 | JSONB Number | `42` | 统一为数字类型 |
| Double | JSONB Number | `3.14` | 浮点数 |
| Boolean | JSONB Boolean | `true` / `false` | 直接映射 |
| Null | JSONB Null | `null` | 直接映射 |
| ObjectId | JSONB String | `"507f1f77bcf86cd799439011"` | 转为 24 位十六进制字符串 |
| Date | JSONB String | `"2024-01-15T10:30:00Z"` | ISO 8601 格式 |
| Binary | JSONB Object | `{"$binary": {"base64": "...", "subType": "..."}}` | 扩展格式 |
| Timestamp | JSONB Object | `{"$timestamp": {"t": 123, "i": 1}}` | 扩展格式 |
| Regex | JSONB Object | `{"$regex": "^foo", "$options": "i"}` | 扩展格式 |

### Schema 设计

FerretDB 在 PostgreSQL 中的 Schema 设计：

```sql
-- 每个 MongoDB 数据库对应一个 PostgreSQL Schema
CREATE SCHEMA IF NOT EXISTS "_ferretdb_database_name";

-- 每个集合对应一张表
CREATE TABLE "_ferretdb_database_name"."collection_name" (
    _id               JSONB NOT NULL,           -- MongoDB _id 字段
    _ferretdb_document JSONB NOT NULL,          -- 完整文档
    PRIMARY KEY ((_id->>'$oid'))                -- 主键
);

-- 索引元数据表
CREATE TABLE "_ferretdb_database_name"."_ferretdb_indexes" (
    id      SERIAL PRIMARY KEY,
    name    TEXT NOT NULL,
    key     JSONB NOT NULL,                    -- 索引字段定义
    unique  BOOLEAN DEFAULT FALSE,
    sparse  BOOLEAN DEFAULT FALSE,
    expire_after_seconds INTEGER               -- TTL 索引支持
);
```

### 文档存储示例

```mermaid
graph TB
    subgraph "MongoDB 文档"
        DOC["{<br/>  '_id': ObjectId('507f...'),<br/>  'name': 'Alice',<br/>  'age': 30,<br/>  'tags': ['dev', 'dba'],<br/>  'profile': {'city': 'Beijing'}<br/>}"]
    end

    subgraph "PostgreSQL 行存储"
        ROW["_id: {\"$oid\": \"507f...\"}<br/>_ferretdb_document: {\"_id\": {...}, \"name\": \"Alice\", ...}"]
    end

    DOC -->|"BSON → JSONB"| ROW
```

## 数据持久化机制

### 写入路径

```mermaid
sequenceDiagram
    participant C as 客户端
    participant F as FerretDB
    participant P as PostgreSQL
    participant D as 磁盘

    C->>F: insertOne 命令 (BSON 文档)
    F->>F: 解析 OpMsg，提取文档
    F->>F: BSON 文档 → JSONB
    F->>F: 生成 ObjectId（如未指定）
    F->>P: INSERT INTO collection VALUES (...)
    P->>P: 写入 WAL 日志
    P->>P: 更新 Buffer Pool
    P->>D: 异步刷脏页
    P-->>F: 插入成功
    F->>F: 构建响应 (insertedId)
    F-->>C: 返回结果
```

### 读取路径

```mermaid
sequenceDiagram
    participant C as 客户端
    participant F as FerretDB
    participant P as PostgreSQL
    participant B as Buffer Pool
    participant D as 磁盘

    C->>F: find 命令 (查询条件)
    F->>F: 解析查询条件
    F->>F: MongoDB 查询 → SQL WHERE
    F->>P: SELECT _ferretdb_document FROM collection WHERE ...
    
    alt 数据在 Buffer Pool
        P->>B: 查询 Buffer Pool
        B-->>P: 返回页面数据
    else 数据在磁盘
        P->>D: 读取页面
        D-->>P: 返回页面
        P->>B: 放入 Buffer Pool
    end
    
    P-->>F: 返回 JSONB 行
    F->>F: JSONB → BSON
    F->>F: 构建响应 (cursor)
    F-->>C: 返回结果集
```

### 事务处理

FerretDB 利用 PostgreSQL 的事务能力实现 MongoDB 风格的事务：

```mermaid
sequenceDiagram
    participant C as 客户端
    participant F as FerretDB
    participant P as PostgreSQL

    C->>F: startSession
    F->>F: 创建会话上下文
    F-->>C: 返回 sessionId

    C->>F: startTransaction
    F->>P: BEGIN TRANSACTION
    P-->>F: 事务开始

    C->>F: insert 操作
    F->>P: INSERT INTO ...
    P-->>F: 插入成功

    C->>F: update 操作
    F->>P: UPDATE ... WHERE ...
    P-->>F: 更新成功

    alt 提交事务
        C->>F: commitTransaction
        F->>P: COMMIT
        P-->>F: 事务提交成功
        F-->>C: 返回提交结果
    else 回滚事务
        C->>F: abortTransaction
        F->>P: ROLLBACK
        P-->>F: 事务已回滚
        F-->>C: 返回回滚结果
    end

    C->>F: endSession
    F->>F: 清理会话上下文
```

## 读写路径详解

### 写入路径流程图

```mermaid
graph TB
    subgraph "1. 协议接收"
        OPMSG["OpMsg 消息<br/>insert 命令"]
        PARSE["解析 BSON 文档"]
        VALID["验证文档结构"]
    end

    subgraph "2. 类型转换"
        BSON2JSONB["BSON → JSONB<br/>类型映射"]
        GENOID["生成 ObjectId<br/>（如未指定）"]
        ENCODE["JSON 编码"]
    end

    subgraph "3. SQL 生成"
        BUILD_SQL["构建 INSERT 语句"]
        PREPARE["准备语句<br/>参数绑定"]
    end

    subgraph "4. PostgreSQL 执行"
        SEND["发送到 PostgreSQL"]
        WAL["写入 WAL 日志"]
        BUFFER["更新 Buffer Pool"]
    end

    subgraph "5. 响应构建"
        RESULT["构建响应文档<br/>insertedId, acknowledged"]
        RESP["返回 OpMsg 响应"]
    end

    OPMSG --> PARSE --> VALID --> BSON2JSONB --> GENOID --> ENCODE
    ENCODE --> BUILD_SQL --> PREPARE --> SEND --> WAL --> BUFFER
    BUFFER --> RESULT --> RESP
```

### 查询路径流程图

```mermaid
graph TB
    subgraph "1. 协议接收"
        FIND["find 命令<br/>查询条件 + 选项"]
        PARSE_FILTER["解析 filter"]
        PARSE_OPTS["解析 projection/sort/limit"]
    end

    subgraph "2. 查询翻译"
        TRANS_WHERE["filter → WHERE 子句"]
        TRANS_SEL["projection → SELECT 子句"]
        TRANS_ORDER["sort → ORDER BY 子句"]
        TRANS_LIMIT["limit/skip → LIMIT/OFFSET"]
    end

    subgraph "3. SQL 执行"
        BUILD_SQL["构建完整 SELECT 语句"]
        EXEC["执行查询"]
        FETCH["获取结果集"]
    end

    subgraph "4. 结果转换"
        JSONB2BSON["JSONB → BSON<br/>每行转换"]
        BUILD_CURSOR["构建 cursor<br/>batch 返回"]
    end

    subgraph "5. 响应返回"
        RESP["返回 OpMsg 响应<br/>cursor 文档"]
    end

    FIND --> PARSE_FILTER --> TRANS_WHERE
    FIND --> PARSE_OPTS --> TRANS_SEL --> TRANS_ORDER --> TRANS_LIMIT
    TRANS_WHERE --> BUILD_SQL
    TRANS_LIMIT --> BUILD_SQL
    BUILD_SQL --> EXEC --> FETCH --> JSONB2BSON --> BUILD_CURSOR --> RESP
```

## 与项目 storage/ 模块的对比

### 架构对比

```mermaid
graph TB
    subgraph "FerretDB 存储架构"
        F_PROTO["Wire Protocol Handler"]
        F_TRANS["翻译层<br/>BSON→JSONB, 查询→SQL"]
        F_BACKEND["Backend 接口"]
        F_PG["PostgreSQL<br/>成熟存储引擎"]
    end

    subgraph "项目 storage/ 模块架构"
        P_API["统一 API 层"]
        P_ENGINE["storage_engine 接口"]
        P_MULTI["多引擎实现<br/>KV/Vector/Graph/..."]
        P_IMPL["自研存储<br/>Heap/BTree/WAL"]
    end

    F_PROTO --> F_TRANS --> F_BACKEND --> F_PG
    P_API --> P_ENGINE --> P_MULTI --> P_IMPL
```

### 核心差异对比

| 维度 | FerretDB | 项目 storage/ 模块 | 分析 |
|------|----------|-------------------|------|
| 存储引擎 | 委托 PostgreSQL | 自研多引擎 | FerretDB 复用成熟存储，项目自主可控 |
| 数据模型 | 文档模型（通过 JSONB） | 多模型（KV/向量/图/时序等） | 项目更丰富，FerretDB 专注文档 |
| 持久化 | PostgreSQL WAL + Buffer Pool | 自研 WAL + Buffer Pool | 架构相似，实现不同 |
| 索引 | 映射到 PG 索引（B-tree/GIN） | 自研 BTree/HNSW/R-Tree 等 | 项目索引类型更多样 |
| 事务 | 委托 PG 事务 | 自研事务管理 | FerretDB 复用成熟事务 |
| 多后端 | 可插拔 Backend 接口 | 可插拔 storage_engine 接口 | 设计理念相似 |

### 接口设计对比

```c
// FerretDB Backend 接口（Go 语言）
type Backend interface {
    // 数据库操作
    CreateCollection(ctx context.Context, db, collection string) error
    DropCollection(ctx context.Context, db, collection string) error
    ListCollections(ctx context.Context, db string) ([]string, error)
    
    // CRUD 操作
    Insert(ctx context.Context, db, collection string, docs []bson.D) error
    Find(ctx context.Context, db, collection string, filter bson.D) (cursor, error)
    Update(ctx context.Context, db, collection string, filter, update bson.D) (int, error)
    Delete(ctx context.Context, db, collection string, filter bson.D) (int, error)
    
    // 索引操作
    CreateIndex(ctx context.Context, db, collection string, spec IndexSpec) error
    DropIndex(ctx context.Context, db, collection, indexName string) error
}

// 项目 storage_engine 接口（C 语言）
typedef struct storage_ops_s {
    const char *name;
    DataModel model;
    
    // 生命周期
    int (*init)(const char *data_dir);
    int (*shutdown)(void);
    
    // 表操作
    int (*table_create)(const char *name, const storage_schema_t *schema);
    void *(*table_open)(const char *name, AccessMode mode);
    int (*table_close)(void *rel);
    int (*table_drop)(const char *name);
    
    // 元组操作
    int (*tuple_insert)(void *rel, const void *data, size_t len);
    int (*tuple_update)(void *rel, const void *old_data, size_t old_len,
                        const void *new_data, size_t new_len);
    int (*tuple_delete)(void *rel, const void *key, size_t key_len);
    
    // 扫描操作
    scan_desc_t *(*scan_begin)(void *rel, const scan_key_t *keys, int nkeys,
                               ScanDirection direction);
    int (*scan_next)(scan_desc_t *scan, void *out_data, size_t *out_len);
    int (*scan_end)(scan_desc_t *scan);
    
    // 索引操作
    int (*index_create)(const char *table, const index_desc_t *index);
    int (*index_drop)(const char *index_name);
} storage_ops_t;
```

### 可借鉴的设计点

| 借鉴点 | FerretDB 实现 | 项目可应用方向 |
|--------|---------------|----------------|
| 协议翻译层 | MongoDB Wire Protocol → SQL | 统一 API → 引擎特化查询 |
| 类型映射器 | BSON → JSONB | 定义项目统一的中间数据格式 |
| 后端抽象 | Backend interface | 增强 storage_engine_t 接口 |
| 配置驱动 | 运行时切换后端 | 编译时/运行时选择引擎 |
| 委托存储 | 复用成熟存储引擎 | 考虑支持 PostgreSQL 后端 |

## 要点总结

- **委托存储架构**：FerretDB 不实现存储引擎，完全委托 PostgreSQL，降低复杂度
- **BSON → JSONB 映射**：通过类型转换保留文档模型灵活性
- **协议翻译层**：Wire Protocol → SQL 的翻译实现 MongoDB 兼容
- **事务委托**：利用 PostgreSQL 的 ACID 能力保证数据一致性
- **与项目对比**：项目自研多引擎存储，FerretDB 委托单引擎，设计理念可互相借鉴

## 思考题

1. FerretDB 选择 JSONB 存储而非拆分为关系表的列，这种设计在什么场景下性能最优？什么场景下会有性能问题？

2. 如果要为项目的多模态存储引擎增加一个"PostgreSQL 后端"选项，需要实现哪些接口？如何处理项目特有的数据模型（如向量、图）？

3. FerretDB 的 Backend 接口设计与项目的 storage_ops_t 接口设计，哪个更灵活？哪个性能更好？为什么？
