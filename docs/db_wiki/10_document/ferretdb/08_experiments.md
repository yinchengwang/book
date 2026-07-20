# FerretDB 动手实验

## 学习目标

- 掌握 FerretDB 的 Docker 部署方法
- 学会使用 mongosh 执行 CRUD 操作
- 理解索引创建和查询性能验证
- 掌握聚合管道的使用方法

## 实验一：Docker 部署 FerretDB + PostgreSQL

### 环境准备

创建 `docker-compose.yml` 文件：

```yaml
version: "3.8"

services:
  postgres:
    image: postgres:16
    container_name: ferretdb-postgres
    environment:
      POSTGRES_USER: ferretdb
      POSTGRES_PASSWORD: ferretdb
      POSTGRES_DB: ferretdb
    volumes:
      - postgres_data:/var/lib/postgresql/data
    ports:
      - "5432:5432"
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U ferretdb"]
      interval: 5s
      timeout: 5s
      retries: 5

  ferretdb:
    image: ghcr.io/ferretdb/ferretdb:latest
    container_name: ferretdb
    depends_on:
      postgres:
        condition: service_healthy
    environment:
      FERRETDB_POSTGRESQL_URL: "postgres://ferretdb:ferretdb@postgres:5432/ferretdb"
    ports:
      - "27017:27017"

volumes:
  postgres_data:
```

### 启动服务

```bash
# 启动服务
docker compose up -d

# 查看日志
docker compose logs -f ferretdb

# 验证 PostgreSQL 连接
docker exec -it ferretdb-postgres psql -U ferretdb -d ferretdb -c "SELECT version();"

# 验证 FerretDB 运行状态
docker compose ps
```

### 架构示意

```mermaid
graph TB
    subgraph "Docker 容器"
        FERRET["FerretDB<br/>27017 端口"]
        PG["PostgreSQL 16<br/>5432 端口"]
        VOLUME["postgres_data<br/>数据持久化"]
    end

    CLIENT["mongosh 客户端"] -->|"MongoDB 协议"| FERRET
    FERRET -->|"pgx 驱动"| PG
    PG --> VOLUME
```

## 实验二：使用 mongosh 执行 CRUD

### 连接 FerretDB

```bash
# 安装 mongosh（如未安装）
# macOS: brew install mongosh
# Windows: winget install MongoDB.Shell
# Linux: 参考 https://www.mongodb.com/docs/mongodb-shell/install/

# 连接 FerretDB
mongosh "mongodb://ferretdb:ferretdb@localhost:27017/?authMechanism=PLAIN"
```

### 插入操作

```javascript
// 切换到测试数据库
use test_db

// 插入单条文档
db.users.insertOne({
    name: "张三",
    email: "zhangsan@example.com",
    age: 28,
    status: "active",
    tags: ["developer", "mongodb"],
    created_at: new Date()
})

// 批量插入
db.users.insertMany([
    {
        name: "李四",
        email: "lisi@example.com",
        age: 32,
        status: "active",
        tags: ["manager", "postgresql"]
    },
    {
        name: "王五",
        email: "wangwu@example.com",
        age: 25,
        status: "inactive",
        tags: ["tester"]
    },
    {
        name: "赵六",
        email: "zhaoliu@example.com",
        age: 35,
        status: "active",
        tags: ["architect", "postgresql", "mongodb"]
    }
])
```

### 查询操作

```javascript
// 查询所有文档
db.users.find()

// 条件查询
db.users.find({ status: "active" })

// 范围查询
db.users.find({ age: { $gte: 30 } })

// 组合查询
db.users.find({
    status: "active",
    age: { $gte: 25, $lte: 35 }
})

// 数组查询
db.users.find({ tags: "postgresql" })

// 投影查询
db.users.find(
    { status: "active" },
    { name: 1, email: 1, _id: 0 }
)

// 排序和分页
db.users.find()
    .sort({ age: -1 })
    .limit(2)
    .skip(1)
```

### 更新操作

```javascript
// 更新单个字段
db.users.updateOne(
    { name: "张三" },
    { $set: { status: "inactive" } }
)

// 更新多个字段
db.users.updateOne(
    { name: "李四" },
    {
        $set: { age: 33, updated_at: new Date() },
        $push: { tags: "leader" }
    }
)

// 批量更新
db.users.updateMany(
    { status: "active" },
    { $inc: { age: 1 } }
)

// 替换整个文档
db.users.replaceOne(
    { name: "王五" },
    {
        name: "王五",
        email: "wangwu_new@example.com",
        age: 26,
        status: "active"
    }
)
```

### 删除操作

```javascript
// 删除单个文档
db.users.deleteOne({ name: "王五" })

// 批量删除
db.users.deleteMany({ status: "inactive" })

// 删除所有文档
db.users.deleteMany({})
```

## 实验三：创建索引并验证查询性能

### 准备测试数据

```javascript
// 批量插入测试数据
const batch_size = 1000
for (let i = 0; i < 10000; i++) {
    const docs = []
    for (let j = 0; j < batch_size && i * batch_size + j < 100000; j++) {
        docs.push({
            user_id: i * batch_size + j,
            name: `用户${i * batch_size + j}`,
            email: `user${i * batch_size + j}@example.com`,
            age: Math.floor(Math.random() * 50) + 18,
            status: Math.random() > 0.3 ? "active" : "inactive",
            score: Math.random() * 100,
            created_at: new Date(Date.now() - Math.random() * 365 * 24 * 60 * 60 * 1000)
        })
    }
    if (docs.length > 0) {
        db.test_data.insertMany(docs)
    }
    if (i % 10 === 0) {
        print(`已插入 ${i * batch_size} 条数据`)
    }
}
```

### 索引创建

```javascript
// 查看现有索引
db.test_data.getIndexes()

// 创建单字段索引
db.test_data.createIndex({ user_id: 1 })

// 创建复合索引
db.test_data.createIndex({ status: 1, age: -1 })

// 创建唯一索引
db.test_data.createIndex({ email: 1 }, { unique: true })

// 查看索引创建进度
db.currentOp({
    $all: true,
    $or: [
        { msg: "Index Build" },
        { msg: "topo" }
    ]
})
```

### 性能对比

```javascript
// 无索引查询（全表扫描）
db.test_data.find({ user_id: 50000 }).explain("executionStats")

// 有索引查询
db.test_data.find({ user_id: 50000 }).hint({ user_id: 1 }).explain("executionStats")

// 范围查询性能
db.test_data.find({
    status: "active",
    age: { $gte: 30, $lte: 40 }
}).sort({ age: -1 }).explain("executionStats")
```

### 执行计划分析

```mermaid
graph LR
    subgraph "无索引"
        SCAN1["COLLSCAN<br/>全表扫描"]
        FILTER1["过滤条件<br/>user_id = 50000"]
        TIME1["耗时: ~500ms"]
    end

    subgraph "有索引"
        SCAN2["IXSCAN<br/>索引扫描"]
        FETCH["回表获取文档"]
        TIME2["耗时: ~5ms"]
    end

    SCAN1 --> FILTER1 --> TIME1
    SCAN2 --> FETCH --> TIME2
```

## 实验四：聚合管道操作

### 基本聚合

```javascript
// 统计各状态用户数量
db.test_data.aggregate([
    { $group: {
        _id: "$status",
        count: { $sum: 1 },
        avg_age: { $avg: "$age" },
        avg_score: { $avg: "$score" }
    }},
    { $sort: { count: -1 } }
])

// 年龄分布统计
db.test_data.aggregate([
    { $bucket: {
        groupBy: "$age",
        boundaries: [18, 25, 30, 35, 40, 45, 50, 68],
        default: "other",
        output: {
            count: { $sum: 1 },
            users: { $push: "$name" }
        }
    }}
])
```

### 多阶段聚合

```javascript
// 复杂聚合：活跃用户的月度注册趋势
db.test_data.aggregate([
    // 阶段1：筛选活跃用户
    { $match: { status: "active" } },
    
    // 阶段2：提取年月
    { $project: {
        name: 1,
        year: { $year: "$created_at" },
        month: { $month: "$created_at" },
        age: 1,
        score: 1
    }},
    
    // 阶段3：按年月分组
    { $group: {
        _id: { year: "$year", month: "$month" },
        user_count: { $sum: 1 },
        avg_age: { $avg: "$age" },
        avg_score: { $avg: "$score" }
    }},
    
    // 阶段4：排序
    { $sort: { "_id.year": -1, "_id.month": -1 } },
    
    // 阶段5：限制输出
    { $limit: 12 }
])
```

### 聚合管道流程

```mermaid
graph LR
    DATA["测试数据<br/>100000 条"] --> MATCH["$match<br/>status=active"]
    MATCH --> PROJECT["$project<br/>提取年月"]
    PROJECT --> GROUP["$group<br/>按月统计"]
    GROUP --> SORT["$sort<br/>时间排序"]
    SORT --> LIMIT["$limit<br/>限制12条"]
    LIMIT --> RESULT["聚合结果"]
```

### $lookup 关联查询

```javascript
// 创建订单集合
db.orders.insertMany([
    { order_id: 1, user_id: 100, amount: 500, status: "paid" },
    { order_id: 2, user_id: 200, amount: 300, status: "pending" },
    { order_id: 3, user_id: 100, amount: 200, status: "paid" }
])

// 创建用户集合
db.customers.insertMany([
    { user_id: 100, name: "张三", level: "gold" },
    { user_id: 200, name: "李四", level: "silver" }
])

// 关联查询
db.orders.aggregate([
    { $lookup: {
        from: "customers",
        localField: "user_id",
        foreignField: "user_id",
        as: "customer_info"
    }},
    { $unwind: "$customer_info" },
    { $project: {
        order_id: 1,
        amount: 1,
        customer_name: "$customer_info.name",
        customer_level: "$customer_info.level"
    }}
])
```

## 实验清理

```bash
# 停止并清理容器
docker compose down

# 清理数据卷（如需要）
docker compose down -v
```

## 要点总结

- **部署**：使用 Docker Compose 一键部署，PostgreSQL 作为存储后端
- **CRUD**：mongosh 操作与 MongoDB 完全一致，无需学习新语法
- **索引**：创建索引可显著提升查询性能，执行计划分析验证效果
- **聚合**：支持 $match/$group/$sort/$limit/$lookup 等常用管道阶段

## 思考题

1. 在实验三中，复合索引 `{ status: 1, age: -1 }` 对哪些查询有效？设计一个无法利用该索引的查询示例。
2. 使用 `explain("executionStats")` 分析聚合管道时，哪个阶段最耗时？如何优化？
3. $lookup 在 FerretDB 中的实现方式是什么？与 MongoDB 原生的 $lookup 有什么性能差异？