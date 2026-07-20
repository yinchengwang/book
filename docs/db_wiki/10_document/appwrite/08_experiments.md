# Appwrite 动手实验

## 学习目标

- 掌握 Appwrite 的 Docker 部署方法
- 通过实验验证核心功能

## 实验环境要求

- Docker 20.10+
- Docker Compose 2.0+
- 内存：至少 4GB
- 端口：80、443 可用

## 实验一：Docker 部署 Appwrite

### 部署命令

```bash
# 拉取 Appwrite 镜像并启动
docker run -it --rm \
    --volume /var/run/docker.sock:/var/run/docker.sock \
    --volume appwrite:/usr/src/code/appwrite:rw \
    --volume appwrite-config:/mnt:rw \
    --env _APP_ENV=production \
    --publish 80:80 \
    --publish 443:443 \
    appwrite/appwrite:latest install
```

### 验证部署

```bash
# 检查容器状态
docker ps | grep appwrite

# 查看日志
docker logs appwrite
```

### 访问控制台

- 打开浏览器访问：`http://localhost`
- 创建管理员账号
- 记录项目 ID 和 API Endpoint

## 实验二：集合与文档 CRUD

### 创建集合

```bash
# 使用 curl 创建集合
curl -X POST "http://localhost/v1/databases/default/collections" \
    -H "Content-Type: application/json" \
    -H "X-Appwrite-Project: your-project-id" \
    -H "X-Appwrite-Key: your-api-key" \
    -d '{
        "collectionId": "users",
        "name": "Users",
        "permissions": ["read(\"any\")", "write(\"any\")"]
    }'
```

### 创建文档

```bash
# 插入文档
curl -X POST "http://localhost/v1/databases/default/collections/users/documents" \
    -H "Content-Type: application/json" \
    -H "X-Appwrite-Project: your-project-id" \
    -H "X-Appwrite-Key: your-api-key" \
    -d '{
        "documentId": "unique()",
        "data": {
            "name": "张三",
            "email": "zhangsan@example.com",
            "age": 25
        }
    }'
```

### 查询文档

```bash
# 获取文档列表
curl -X GET "http://localhost/v1/databases/default/collections/users/documents" \
    -H "X-Appwrite-Project: your-project-id" \
    -H "X-Appwrite-Key: your-api-key"

# 查询单个文档
curl -X GET "http://localhost/v1/databases/default/collections/users/documents/{document-id}" \
    -H "X-Appwrite-Project: your-project-id"
```

## 实验三：文件上传与预览

### 创建存储桶

```bash
# 创建存储桶
curl -X POST "http://localhost/v1/storage/buckets" \
    -H "Content-Type: application/json" \
    -H "X-Appwrite-Project: your-project-id" \
    -H "X-Appwrite-Key: your-api-key" \
    -d '{
        "bucketId": "images",
        "name": "Images",
        "permissions": ["read(\"any\")", "write(\"users\")"]
    }'
```

### 上传文件

```bash
# 上传文件
curl -X POST "http://localhost/v1/storage/buckets/images/files" \
    -H "X-Appwrite-Project: your-project-id" \
    -F "fileId=unique()" \
    -F "file=@/path/to/image.png"
```

### 获取预览链接

```bash
# 获取图片预览
curl -X GET "http://localhost/v1/storage/buckets/images/files/{file-id}/view" \
    -H "X-Appwrite-Project: your-project-id"
```

## 实验四：创建和执行 Functions

### 创建函数

```javascript
// index.js - Node.js 函数
export default async function({ request, response }) {
    const body = await request.json();
    
    return response.json({
        message: 'Hello from Appwrite Functions!',
        input: body
    });
}
```

### 部署函数

```bash
# 使用 CLI 部署
appwrite function create \
    --function-id hello \
    --name "Hello Function" \
    --runtime node-18

appwrite function createTag \
    --function-id hello \
    --entrypoint "index.js" \
    --code "./function-code"
```

### 执行函数

```bash
# 调用函数
curl -X POST "http://localhost/v1/functions/hello/executions" \
    -H "Content-Type: application/json" \
    -H "X-Appwrite-Project: your-project-id" \
    -d '{"name": "World"}'
```

## 实验对比

| 实验 | 工具 | 预期结果 |
|------|------|----------|
| 部署 | Docker | 控制台可访问 |
| CRUD | curl | 文档正常读写 |
| 文件 | curl | 文件上传成功 |
| 函数 | CLI | 函数执行成功 |

## 要点总结

- Docker 部署是最快捷的方式
- REST API 完整覆盖所有功能
- Functions 支持多运行时

## 思考题

1. 如何配置 HTTPS？
2. 如何设置邮件发送服务？