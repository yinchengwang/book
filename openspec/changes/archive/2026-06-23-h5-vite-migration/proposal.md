# H5 端 Vite 迁移提案

## Why

`reading-radar-2-redesign` 变更已完成 164/164 任务，H5 端的页面、store、组件、数据全部就绪。但 Taro 3.6 + Node 24 存在依赖链冲突：

1. `@pmmmwh/react-refresh-webpack-plugin` 要 webpack 5，Taro 3.6 锁 webpack 4
2. `@tarojs/plugin-framework-react` 的 `webpack.h5.ts` 硬 require 上述插件
3. babel-loader / sass-loader / postcss-loader 在 `--legacy-peer-deps` 安装后部分缺失
4. 条件编译指令 `#ifdef H5` / `#ifdef MP-WEIXIN` 在 babel-loader 直接编译时未剥离，触发 `Identifier 'TabBar' has already been declared` 错误

这些冲突**仅影响 H5 端编译**。Taro 对微信小程序的编译链（`build:weapp`）完全独立、运行良好。

**机会**：保留 Taro 编译链发小程序，新增 Vite 工程独立跑 H5 端。源码 90% 复用，开发体验和性能都更好。

## What Changes

### 架构调整

在 `reading-radar-2/web/` 下新建 Vite + React + TypeScript 工程，**与 Taro 编译链并行运行**：

```
reading-radar-2/
├── src/                # 共享源码（90% 复用）
├── web/                # 🆕 H5 端 Vite 工程
│   ├── adapters/       # Taro 组件适配层
│   ├── App.tsx         # 路由 + Layout
│   ├── main.tsx
│   ├── index.html
│   ├── vite.config.ts
│   └── tsconfig.json
└── config/             # Taro 配置（保留，小程序编译用）
```

### 关键技术决策

1. **Taro 组件适配**：写 6 个 shim 文件把 `<View>` `<Text>` 等映射到原生 HTML 标签
2. **路由**：用 `react-router-dom` 替代 Taro 路由，19 个页面一对一映射
3. **状态管理**：Zustand + persist 直接复用，无需修改
4. **样式**：SCSS 直接用 Vite 处理（Taro 编译时 rpx→px 这步跳过，因为源码用 px）
5. **第三方库**：Recharts、@xyflow/react、Framer Motion、Lucide React 全部直接用
6. **条件编译**：源码中 `#ifdef MP-WEIXIN` 包裹的代码（H5 端无效）在 Vite 端**会执行**，需适配（如 `wx.switchTab` → `navigate()`）

### 影响范围

| 模块 | 现状 | 变更 |
|------|------|------|
| 19 个页面 | 通过 Taro 编译运行 H5 | 通过 Vite 编译运行 H5 |
| 16 个 store | 直接可复用 | 直接可复用 |
| 组件库 | 依赖 `@tarojs/components` | 通过 alias 映射到适配层 |
| 样式 | 走 Taro 后处理 | 走 Vite + sass |
| 微信小程序编译 | `npm run build:weapp` | **完全不动** |
| 源码目录 `src/` | Taro 工程结构 | **完全不动** |

## Capabilities

### New Capabilities

- `h5-vite-build`: H5 端 Vite 编译能力 — 独立编译链，与 Taro 小程序编译并存

### Modified Capabilities

- `multi-platform`: 多端适配 — H5 端编译从 Taro 改为 Vite，小程序端保持 Taro 不变

## Impact

### 新增文件

```
web/
├── index.html
├── main.tsx
├── App.tsx
├── vite.config.ts
├── tsconfig.json
├── package.json         (可选独立依赖声明)
├── adapters/
│   ├── View.tsx
│   ├── Text.tsx
│   ├── Button.tsx
│   ├── Image.tsx
│   ├── Input.tsx
│   ├── Textarea.tsx
│   ├── ScrollView.tsx
│   ├── taro-shim.ts     (Taro.navigateTo 等 API 桩)
│   └── index.ts
├── components/
│   └── Layout/          (PC 端 Sidebar + TopBar 布局)
└── styles/
    └── reset.scss       (浏览器样式重置)
```

### 不变的部分

- `src/pages/`、`src/components/`、`src/stores/`、`src/data/`、`src/utils/`、`src/styles/` 全部不动
- `config/` Taro 配置不动
- `package.json` 新增 Vite 相关依赖，不移除任何 Taro 依赖

### 平台目标

- **H5 端**：现代浏览器（Chrome 90+, Edge 90+, Safari 14+, Firefox 88+）
- **微信小程序**：保持现状（Taro 编译）

### 性能与体验

- Vite HMR 启动 < 1s
- 生产构建产物 < 500KB gzip（不含 Recharts/xyflow）
- 与 Taro H5 编译产物功能等价
