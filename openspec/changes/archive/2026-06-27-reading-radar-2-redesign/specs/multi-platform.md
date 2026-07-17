# Multi-Platform 多端适配

## Overview

多端适配层使用 Taro 统一处理 H5 网页和微信小程序的差异。

## Features

### F11.1 项目脚手架

**描述**：Taro 项目初始化

**命令**：
```bash
npx create-taro@latest reading-radar-2
# 选择：React + TypeScript + TailwindCSS
```

**目录结构**：
```
reading-radar-2/
├── src/
│   ├── app.tsx           # 应用入口
│   ├── app.config.ts     # 全局配置
│   ├── components/       # 组件
│   ├── pages/           # 页面
│   ├── stores/          # 状态
│   ├── hooks/           # Hooks
│   ├── services/        # API 服务
│   ├── utils/          # 工具
│   └── styles/          # 样式
├── config/              # 构建配置
├── package.json
├── tsconfig.json
└── taro.config.ts
```

---

### F11.2 条件编译

**描述**：处理平台差异

**编译指令**：
```typescript
// 组件差异
// #ifdef MP-WEIXIN
import { useWechatShare } from '@/utils/wechat';
// #endif
// #ifndef MP-WEIXIN
import { useNativeShare } from '@/utils/share';
// #endif

// 样式差异
// 在样式文件中使用
/* #ifdef MP-WEIXIN */
.backdrop-blur { backdrop-filter: none; } /* 降级处理 */
/* #endif */

// 路由差异
// H5: /pages/index/index
// 小程序: pages/index/index
```

---

### F11.3 响应式布局

**描述**：统一响应式断点

**断点定义**：
| 断点 | 宽度 | 设备 |
|------|------|------|
| xs | < 320px | 小手机 |
| sm | 320-639px | 手机 |
| md | 640-1023px | 大手机/平板 |
| lg | 1024-1279px | 平板 |
| xl | >= 1280px | 桌面 |

**Tailwind 配置**：
```javascript
// tailwind.config.js
module.exports = {
  screens: {
    'xs': '320px',
    'sm': '640px',
    'md': '768px',
    'lg': '1024px',
    'xl': '1280px',
  },
}
```

---

### F11.4 小程序适配

**描述**：小程序特殊处理

**TabBar 配置**：
```typescript
// app.config.ts
export default defineAppConfig({
  tabBar: {
    color: '#94A3B8',
    selectedColor: '#6366F1',
    backgroundColor: '#000000',
    borderStyle: 'white',
    list: [
      { pagePath: 'pages/index/index', text: '首页', iconPath: 'assets/home.png' },
      { pagePath: 'pages/radar/index', text: '雷达', iconPath: 'assets/radar.png' },
      { pagePath: 'pages/kanban/index', text: '看板', iconPath: 'assets/kanban.png' },
      { pagePath: 'pages/quiz/index', text: '测验', iconPath: 'assets/quiz.png' },
      { pagePath: 'pages/mine/index', text: '我的', iconPath: 'assets/mine.png' },
    ]
  }
})
```

**分包配置**（可选）：
```typescript
// 减少主包体积
subPackages: [
  {
    root: 'pages/graph',
    pages: ['index']
  }
]
```

---

### F11.5 H5 适配

**描述**：H5 端特殊处理

**路由配置**：
```typescript
// 路由类型
type RouteConfig = {
  path: string;
  component: React.ComponentType;
  meta?: {
    title?: string;
    keepAlive?: boolean;
  }
};
```

**浏览器兼容**：
- 目标：Chrome 80+, Safari 13+, Firefox 75+
- polyfills：按需引入
- CSS：autoprefixer

---

### F11.6 跨平台组件

**描述**：抽象平台差异的组件

**SafeArea**：
```typescript
// 适配刘海屏、安全区域
<SafeArea position="top">导航栏</SafeArea>
<SafeArea position="bottom">TabBar</SafeArea>
```

**WebView**：
```typescript
// H5: iframe
// 小程序: web-view
<HybridView src={url} />
```

**Image**：
```typescript
// 统一图片处理
// 支持：网络图、本地图片、base64
// 小程序：需在 image 标签添加 mode
<HybridImage src={src} mode="aspectFill" />
```

## Technical Notes

- 条件编译：Taro 内置 `#ifdef` 语法
- 样式适配：使用 rpx / rem 单位
- 图片适配：小程序使用 CDN + 压缩
- 性能优化：分包加载、懒加载
