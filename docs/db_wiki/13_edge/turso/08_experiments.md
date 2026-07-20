# Turso 实验手册

## 学习目标
- 掌握 Turso CLI 的安装和配置
- 学会创建和管理数据库
- 理解嵌入式副本的使用方法
- 掌握分支操作流程

## 环境准备

### 1. 安装 Turso CLI

```bash
# macOS / Linux
curl -sSfL https://get.turso.io | bash

# Windows（通过 npm）
npm install -g turso

# 验证安装
turso --version

# 登录（需要 GitHub 账号）
turso auth login

# 验证登录
turso auth whoami
```

### 2. 配置认证

```bash
# 查看当前 Token
turso auth token

# 设置环境变量
export TURSO_AUTH_TOKEN=$(turso auth token)
```

## 数据库操作

### 1. 创建数据库

```bash
# 创建数据库
turso db create my-first-db

# 指定区域
turso db create my-db --location us-east

# 查看可用区域
turso db locations
```

### 2. 查看和管理数据库

```bash
# 列出所有数据库
turso db list

# 查看数据库详情
turso db show my-first-db

# 删除数据库
turso db destroy my-first-db
```

### 3. 数据库 Shell

```bash
# 打开交互式 Shell
turso db shell my-first-db

# 创建表
turso db shell my-first-db "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)"

# 插入数据
turso db shell my-first-db "INSERT INTO users VALUES (1, 'Alice', 'alice@example.com')"

# 查询数据
turso db shell my-first-db "SELECT * FROM users"
```### 4. CRUD 操作

```sql
-- 创建表
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    price REAL,
    stock INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 插入数据
INSERT INTO products (name, price, stock) VALUES ('Widget', 9.99, 100);
INSERT INTO products (name, price, stock) VALUES ('Gadget', 19.99, 50);
INSERT INTO products (name, price, stock) VALUES ('Doohickey', 4.99, 200);

-- 查询
SELECT * FROM products WHERE price > 10;
SELECT COUNT(*) as total, AVG(price) as avg_price FROM products;

-- 更新
UPDATE products SET stock = stock + 10 WHERE name = 'Widget';

-- 删除
DELETE FROM products WHERE stock = 0;

-- 批量插入
INSERT INTO products (name, price, stock) VALUES
    ('Item A', 1.99, 100),
    ('Item B', 2.99, 200),
    ('Item C', 3.99, 300);
```

### 5. 向量操作

```sql
-- 创建向量表
CREATE TABLE embeddings (
    id INTEGER PRIMARY KEY,
    text TEXT,
    embedding VECTOR(3)
);

-- 插入向量
INSERT INTO embeddings (text, embedding) VALUES
    ('hello', vector('[0.1, 0.2, 0.3]')),
    ('world', vector('[0.4, 0.5, 0.6]')),
    ('turso', vector('[0.7, 0.8, 0.9]'));

-- 向量搜索
SELECT text, vector_distance(embedding, vector('[0.15, 0.25, 0.35]')) as distance
FROM embeddings
ORDER BY distance
LIMIT 5;
```

## 嵌入式副本实验

### 1. 配置嵌入式副本

```javascript
import { createClient } from '@libsql/client';

const db = createClient({
  url: 'file:local.db',
  syncUrl: 'libsql://my-db.turso.io',
  authToken: 'your-token-here',
  syncInterval: 30
});

// 初始化同步
await db.sync();
console.log('初始同步完成');
```

### 2. 同步状态监控

```javascript
const status = await db.syncStatus();
console.log('当前帧号:', status.currentFrame);
console.log('已应用帧号:', status.appliedFrame);
console.log('落后帧数:', status.currentFrame - status.appliedFrame);

// 手动触发同步
await db.sync();
```

### 3. 离线写入测试

```javascript
async function testOfflineWrite() {
  const db = createClient({
    url: 'file:local.db',
    syncUrl: 'libsql://my-db.turso.io',
    authToken: 'your-token-here'
  });

  // 初始同步
  await db.sync();

  // 离线写入本地
  console.log('离线写入...');
  await db.execute('INSERT INTO users (name) VALUES (?)', ['Offline User']);

  // 恢复网络后同步
  console.log('恢复网络...');
  const result = await db.sync();
  console.log('同步完成');
  return result;
}
```

## 分支操作

### 1. 创建和管理分支

```bash
# 创建数据库
turso db create prod-db

# 创建开发分支
turso db branch create prod-db dev-branch

# 列出所有分支
turso db branch list prod-db

# 查看分支详情
turso db branch show prod-db dev-branch
```

### 2. 分支实验

```bash
# 在开发分支上实验
turso db shell prod-db dev-branch
> CREATE TABLE test_table (id INTEGER PRIMARY KEY, name TEXT);
> INSERT INTO test_table VALUES (1, 'Test Data');

# 切换回主分支确认隔离
turso db shell prod-db main
> SELECT * FROM test_table;
-- Error: no such table: test_table
```

### 3. 分支合并

```bash
# 合并到主分支
turso db branch merge prod-db dev-branch

# 验证合并
turso db shell prod-db main
> SELECT * FROM test_table;
```

## HTTP 协议测试

### 1. 获取连接信息

```bash
# 获取数据库 URL 和 Token
turso db show my-db
turso db tokens create my-db
```

### 2. HTTP 查询

```bash
curl -X POST "https://my-db.turso.io/v2/pipeline" \
  -H "Authorization: Bearer $TURSO_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "requests": [
      {
        "type": "execute",
        "stmt": {
          "sql": "SELECT 1 as test"
        }
      },
      {
        "type": "close"
      }
    ]
  }'
```

## Serverless 集成

### 1. Cloudflare Worker

```bash
npm create cloudflare@latest turso-worker
cd turso-worker
npm install @libsql/client/web
```

```javascript
// src/index.js
import { createClient } from '@libsql/client/web';

export default {
  async fetch(request, env, ctx) {
    const db = createClient({
      url: env.TURSO_URL,
      authToken: env.TURSO_AUTH_TOKEN
    });

    const result = await db.execute('SELECT * FROM users');
    return Response.json({ users: result.rows });
  }
};
```

### 2. Vercel Edge Function

```bash
npm create vercel@latest turso-vercel
cd turso-vercel
npm install @libsql/client/web
```

```javascript
// api/users.js
import { createClient } from '@libsql/client/web';

export const config = { runtime: 'edge' };

export default async function handler(req) {
  const db = createClient({
    url: process.env.TURSO_URL,
    authToken: process.env.TURSO_AUTH_TOKEN
  });

  const users = await db.execute('SELECT * FROM users');
  return new Response(JSON.stringify(users.rows), {
    headers: { 'Content-Type': 'application/json' }
  });
}
```

## 实验检查清单

- [ ] Turso CLI 安装成功
- [ ] 创建数据库
- [ ] CRUD 操作
- [ ] 向量操作
- [ ] 嵌入式副本同步
- [ ] 分支创建和合并
- [ ] HTTP 协议查询
- [ ] Cloudflare Worker 集成

## 要点总结

- **Turso CLI** 是管理和操作数据库的主要工具
- **嵌入式副本**通过本地文件 + 远程同步实现离线优先
- **分支系统**支持开发隔离和实验
- **HTTP 协议**无需传统数据库驱动
- **Serverless 集成**支持 Cloudflare Worker / Vercel Edge

## 思考题

1. 嵌入式副本的同步间隔如何影响应用性能和一致性？
2. 分支合并出现冲突时，Turso 如何处理？
3. HTTP 协议相比原生 TCP 协议在查询性能上有多少额外开销？