# Appwrite 关键特性

## 学习目标

- 掌握 Appwrite 的核心功能特性
- 理解各特性的使用场景和实现原理

## 特性总览

```mermaid
graph TD
    A[Appwrite 关键特性] --> B[实时订阅]
    A --> C[文件存储]
    A --> D[认证系统]
    A --> E[函数计算]
    A --> F[数据库 API]
    A --> G[团队权限]
```

## 实时订阅（Realtime）

```mermaid
sequenceDiagram
    participant C as 客户端
    participant WS as WebSocket 服务
    participant DB as Database
    participant EVENT as Event Bus

    C->>WS: 订阅集合 users
    WS-->>C: 订阅成功
    loop 数据变更
        DB->>EVENT: 触发 document.changed
        EVENT->>WS: 推送事件
        WS->>C: 推送变更数据
    end
```

### 实时订阅示例

```javascript
// Web SDK 订阅文档变更
import { Client, Databases, Realtime } from 'appwrite';

const client = new Client()
    .setEndpoint('https://example.com/v1')
    .setProject('project-id');

const databases = new Databases(client);

// 订阅集合变更
client.subscribe('documents', response => {
    console.log('文档变更:', response);
});
```

## 文件存储（Storage）

| 特性 | 说明 |
|------|------|
| S3 兼容 | 支持 AWS S3、MinIO 等 |
| 图片处理 | 支持裁剪、缩放、格式转换 |
| 预览链接 | 生成临时访问 URL |
| 批量上传 | 支持多文件批量操作 |
| 权限控制 | 文件级别 ACL |

### 文件上传示例

```javascript
import { Storage } from 'appwrite';

const storage = new Storage(client);

// 上传文件
const file = await storage.createFile(
    'bucket-id',
    'unique-file-id',
    fileObject,
    ['read("any")']  // 权限设置
);

// 获取预览链接
const preview = storage.getFilePreview('bucket-id', 'file-id');
```

## 认证系统（Auth）

```mermaid
graph LR
    A[认证方式] --> B[Email/Password]
    A --> C[OAuth 2.0]
    A --> D[Phone/SMS]
    A --> E[Magic URL]
    A --> F[Anonymous]

    C --> G[Google]
    C --> H[GitHub]
    C --> I[Apple]
    C --> J[Facebook]
```

### OAuth 认证流程

```mermaid
sequenceDiagram
    participant U as 用户
    participant APP as 应用
    participant AUTH as Auth Service
    participant PROVIDER as OAuth Provider

    U->>APP: 点击 Google 登录
    APP->>AUTH: 请求 OAuth URL
    AUTH-->>APP: 返回授权 URL
    APP->>PROVIDER: 重定向到 Provider
    PROVIDER->>U: 用户授权
    U->>PROVIDER: 确认授权
    PROVIDER->>AUTH: 回调带 Code
    AUTH->>PROVIDER: 用 Code 换 Token
    PROVIDER-->>AUTH: 返回用户信息
    AUTH-->>APP: 创建 Session
```

## 函数计算（Functions）

```mermaid
graph TD
    A[触发方式] --> B[HTTP 请求]
    A --> C[定时任务]
    A --> D[事件触发]
    A --> E[手动调用]

    F[运行时] --> G[Node.js]
    F --> H[Deno]
    F --> I[Ruby]
    F --> J[Python]
    F --> K[Go]
```

### 函数示例

```javascript
// Node.js 函数示例
export default async function({ request, response }) {
    const body = await request.json();
    
    // 业务逻辑处理
    const result = processData(body);
    
    return response.json({
        success: true,
        data: result
    });
}
```

## 数据库 API

| 操作 | 方法 | 说明 |
|------|------|------|
| 创建文档 | POST /databases/{id}/collections/{id}/documents | 插入新文档 |
| 获取文档 | GET /databases/{id}/collections/{id}/documents/{id} | 查询单个 |
| 列表查询 | GET /databases/{id}/collections/{id}/documents | 条件查询 |
| 更新文档 | PATCH /databases/{id}/collections/{id}/documents/{id} | 部分更新 |
| 删除文档 | DELETE /databases/{id}/collections/{id}/documents/{id} | 删除 |

## 要点总结

- **实时订阅**：基于 WebSocket，支持文档和集合级别订阅
- **文件存储**：S3 兼容，支持图片处理和权限控制
- **认证系统**：多种认证方式，OAuth 2.0 完整支持
- **函数计算**：多运行时支持，事件驱动触发

## 思考题

1. Realtime 如何处理大量订阅连接？
2. Functions 的冷启动如何优化？