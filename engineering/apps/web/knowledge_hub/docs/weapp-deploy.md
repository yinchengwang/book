# 小程序端测试与发布指南

## 环境准备

### 1. 安装微信开发者工具

下载并安装 [微信开发者工具](https://developers.weixin.qq.com/miniprogram/dev/devtools/download.html)

### 2. 安装依赖

```bash
cd knowledge_hub
npm install
```

### 3. 配置小程序 AppID

编辑 `config/index.js`:

```js
weapp: {
  appid: 'YOUR_APPID',  // 替换为你的小程序 AppID
  compile: {}
}
```

## 本地开发

```bash
# 启动小程序开发服务器
npm run dev:weapp
```

用微信开发者工具导入 `knowledge_hub/dist/weapp` 目录

## 构建生产版本

```bash
# 构建小程序生产包
npm run build:weapp

# 输出目录: dist/weapp/
```

## 发布流程

### 1. 登录微信公众平台

访问 [微信公众平台](https://mp.weixin.qq.com/)，登录小程序账号

### 2. 上传代码

在微信开发者工具中点击「上传」按钮

### 3. 提交审核

1. 登录微信公众平台
2. 进入「版本管理」
3. 选择待审核版本
4. 填写版本说明
5. 提交审核

### 4. 版本发布

审核通过后，点击「发布」按钮

## CI/CD 自动化

### GitHub Actions

创建 `.github/workflows/weapp.yml`:

```yaml
name: Build Weapp

on:
  push:
    branches: [main]
  workflow_dispatch:

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: '20'
          cache: 'npm'
      - run: npm ci
      - run: npm run build:weapp
      - uses: actions/upload-artifact@v4
        with:
          name: weapp-dist
          path: dist/weapp
```

## 条件编译说明

项目使用 Taro 条件编译处理 H5 与小程序差异：

```tsx
// #ifdef H5
// 仅 H5 端编译
<div className="h5-only">H5 专属功能</div>
// #endif

// #ifdef MP-WEIXIN
// 仅小程序端编译
<view className="mp-only">小程序专属功能</view>
// #endif
```

## 常见问题

### 1. 用户未授权

处理方式：引导用户到设置页面开启授权

### 2. 域名校验失败

在「开发管理」→「开发设置」中配置合法域名

### 3. ES6 转 ES5

Taro 默认已处理，无需额外配置

### 4. 文件大小超限

- 检查并压缩图片资源
- 移除未使用的依赖
- 使用分包加载

## 性能优化

### 1. 分包加载

配置 `app.config.ts`:

```ts
export default defineAppConfig({
  pages: [...],
  subpackages: [
    {
      root: 'pages/sub',
      pages: ['list', 'detail']
    }
  ]
})
```

### 2. 图片优化

- 使用 WebP 格式
- 控制图片尺寸
- 懒加载图片

### 3. 代码分割

利用 Taro 的路由懒加载功能

## 审核注意事项

1. **功能完整性**：确保核心功能可用
2. **用户协议**：添加用户隐私协议页面
3. **类目选择**：选择正确的小程序类目
4. **截图准备**：准备各页面截图用于审核
