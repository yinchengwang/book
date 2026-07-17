# H5 端测试与发布指南

## 环境准备

```bash
# 安装依赖
cd knowledge_hub
npm install

# 安装 Playwright（用于 E2E 测试）
npx playwright install --with-deps
```

## 本地开发

```bash
# 启动 H5 开发服务器（默认端口 10086）
npm run dev:h5
```

访问 http://localhost:10086

## 构建生产版本

```bash
# 构建 H5 生产包
npm run build:h5

# 输出目录: dist/h5/
```

## 部署选项

### 1. Nginx 部署

```nginx
server {
    listen 80;
    server_name your-domain.com;
    root /path/to/knowledge_hub/dist/h5;
    index index.html;

    # SPA 重定向
    location / {
        try_files $uri $uri/ /index.html;
    }

    # 静态资源缓存
    location /static/ {
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
}
```

### 2. Vercel 部署

创建 `vercel.json`:

```json
{
  "buildCommand": "npm run build:h5",
  "outputDirectory": "dist/h5",
  "rewrites": [
    { "source": "/(.*)", "destination": "/index.html" }
  ]
}
```

### 3. Docker 部署

```dockerfile
FROM nginx:alpine
COPY dist/h5 /usr/share/nginx/html
COPY nginx.conf /etc/nginx/conf.d/default.conf
EXPOSE 80
```

### 4. GitHub Pages 部署

修改 `config/index.js`:

```js
h5: {
  publicPath: '/knowledge_hub/',
  // ...
}
```

## CI/CD 自动化

### GitHub Actions

创建 `.github/workflows/deploy.yml`:

```yaml
name: Deploy H5

on:
  push:
    branches: [main]

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: '20'
      - run: npm ci
      - run: npm run build:h5
      - uses: peaceiris/actions-gh-pages@v3
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: ./dist/h5
```

## 验证部署

```bash
# E2E 测试
npm run test:e2e

# UI 模式测试
npm run test:e2e:ui
```

## 性能指标

构建后检查:

- 首屏加载 < 3s (3G 网络)
- Lighthouse Performance Score > 80
- 无 JavaScript 错误

## 常见问题

### 1. 跨域问题

确保 API 服务正确配置 CORS 头。

### 2. 微信授权（可选）

如需微信登录，需配置微信公众号授权回调域名。

### 3. 静态资源 404

检查 `publicPath` 配置是否与部署路径一致。
