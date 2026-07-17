# H5 端 Vite 迁移 — 设计文档

## 1. 背景与目标

### 1.1 现状

`reading-radar-2-redesign` 变更已完成 164/164 任务，H5 端页面、组件、store、数据全部就绪。但 Taro 3.6 + Node 24 存在依赖链冲突，H5 端无法启动。

### 1.2 目标

新增 Vite 工程作为 H5 端独立编译链：

- ✅ H5 端能在 PC 浏览器运行
- ✅ 微信小程序端继续通过 Taro 编译
- ✅ 90% 源码共享
- ✅ 双端最终通过后端 API 互通数据

## 2. 架构设计

### 2.1 目录结构

```
reading-radar-2/
├── src/                          # 共享源码
│   ├── pages/                    # 19 个页面
│   ├── components/               # 通用组件
│   ├── stores/                   # 16 个 Zustand store
│   ├── data/                     # 静态数据
│   ├── utils/                    # 工具函数
│   ├── hooks/                    # 自定义 Hooks
│   └── styles/                   # SCSS 样式
│
├── web/                          # 🆕 H5 端 Vite 工程
│   ├── adapters/                 # Taro 组件适配层
│   │   ├── View.tsx
│   │   ├── Text.tsx
│   │   ├── Button.tsx
│   │   ├── Image.tsx
│   │   ├── Input.tsx
│   │   ├── Textarea.tsx
│   │   ├── ScrollView.tsx
│   │   ├── taro-shim.ts          # Taro.* API 桩
│   │   └── index.ts              # 统一导出
│   ├── components/               # H5 端专属组件
│   │   └── Layout/               # Sidebar + TopBar
│   │       ├── Sidebar.tsx
│   │       ├── TopBar.tsx
│   │       └── Layout.tsx
│   ├── pages/                    # H5 端路由级组件（懒加载包装）
│   │   ├── Dashboard.tsx
│   │   ├── Quiz.tsx
│   │   └── ...
│   ├── styles/
│   │   ├── reset.scss            # 浏览器样式重置
│   │   └── global.scss           # 全局变量、字体
│   ├── App.tsx                   # 路由配置
│   ├── main.tsx                  # React 入口
│   ├── index.html                # HTML 入口
│   ├── vite.config.ts            # Vite 配置
│   ├── tsconfig.json             # TS 配置
│   └── package.json              # 独立依赖声明
│
├── config/                       # Taro 配置（保留）
├── package.json                  # 主 package.json（新增 Vite 依赖）
└── tsconfig.json                 # 主 TS 配置
```

### 2.2 编译流程

```
H5 端：
src/pages/* ─┐
src/components/* ─┤
src/stores/* ─┤──→ Vite ──→ web/dist/* (静态资源)
src/styles/* ─┤
web/App.tsx ──┤
web/main.tsx ─┘

小程序端：
src/pages/* ─┐
src/components/* ─┤
src/stores/* ─┤──→ Taro ──→ dist/* (小程序代码)
src/styles/* ─┤
config/* ─────┘
```

两套流程**互不依赖**。

## 3. 关键技术决策

### 3.1 Taro 组件适配

**问题**：源码用 `<View>` `<Text>` 等 Taro 组件，小程序端正常，H5 端需要替换为原生 HTML。

**方案**：通过 Vite alias 拦截 `@tarojs/components` 的 import。

```ts
// web/vite.config.ts
resolve: {
  alias: {
    '@tarojs/components': path.resolve(__dirname, 'adapters/index.ts'),
    '@tarojs/taro': path.resolve(__dirname, 'adapters/taro-shim.ts')
  }
}
```

**适配层**：

```tsx
// web/adapters/View.tsx
import { HTMLAttributes, ReactNode } from 'react'
type ViewProps = HTMLAttributes<HTMLDivElement> & {
  hoverClass?: string
  children?: ReactNode
}
export const View = (props: ViewProps) => {
  const { hoverClass, ...rest } = props
  return <div {...rest} />
}
```

```tsx
// web/adapters/index.ts
export { View } from './View'
export { Text } from './Text'
export { Button } from './Button'
export { Image } from './Image'
export { Input } from './Input'
export { Textarea } from './Textarea'
export { ScrollView } from './ScrollView'
```

### 3.2 Taro API 桩

**问题**：源码可能调用 `Taro.navigateTo` `Taro.showToast` 等。

**方案**：

```ts
// web/adapters/taro-shim.ts
export const Taro = {
  navigateTo: ({ url }: { url: string }) => {
    history.pushState(null, '', url)
    window.dispatchEvent(new PopStateEvent('popstate'))
  },
  redirectTo: ({ url }: { url: string }) => {
    history.replaceState(null, '', url)
    window.dispatchEvent(new PopStateEvent('popstate'))
  },
  switchTab: ({ url }: { url: string }) => {
    history.pushState(null, '', url)
    window.dispatchEvent(new PopStateEvent('popstate'))
  },
  showToast: ({ title, icon = 'none' }: any) => {
    // 简易 toast 实现
    const div = document.createElement('div')
    div.textContent = title
    div.style.cssText = `
      position:fixed; top:50%; left:50%; transform:translate(-50%,-50%);
      background:rgba(0,0,0,0.8); color:#fff; padding:12px 20px;
      border-radius:8px; z-index:9999; font-size:14px;
    `
    document.body.appendChild(div)
    setTimeout(() => div.remove(), 2000)
  },
  getStorageSync: (key: string) => localStorage.getItem(key),
  setStorageSync: (key: string, value: string) => localStorage.setItem(key, value),
  removeStorageSync: (key: string) => localStorage.removeItem(key),
  getStorageInfoSync: () => ({ keys: Object.keys(localStorage) })
}
```

### 3.3 路由系统

**问题**：Taro 用 `Taro.navigateTo`，H5 端用 `react-router-dom`。

**方案**：

```tsx
// web/App.tsx
import { lazy, Suspense } from 'react'
import { BrowserRouter, Routes, Route } from 'react-router-dom'
import Layout from './components/Layout/Layout'

const Dashboard = lazy(() => import('../src/pages/dashboard/Dashboard'))
const Quiz = lazy(() => import('../src/pages/quiz/Quiz'))
// ... 19 个 lazy import

export default function App() {
  return (
    <BrowserRouter>
      <Layout>
        <Suspense fallback={<div>Loading...</div>}>
          <Routes>
            <Route path="/" element={<Dashboard />} />
            <Route path="/dashboard" element={<Dashboard />} />
            <Route path="/quiz" element={<Quiz />} />
            <Route path="/mock-interview" element={<MockInterview />} />
            {/* ... 19 个路由 */}
          </Routes>
        </Suspense>
      </Layout>
    </BrowserRouter>
  )
}
```

### 3.4 路径别名

源码中使用了 `@/components` `@/pages` 等别名（`config/index.js` 已配）。Vite 端需要同步配：

```ts
// web/vite.config.ts
resolve: {
  alias: {
    '@': path.resolve(__dirname, '../src'),
    '@/components': path.resolve(__dirname, '../src/components'),
    '@/pages': path.resolve(__dirname, '../src/pages'),
    '@/stores': path.resolve(__dirname, '../src/stores'),
    '@/data': path.resolve(__dirname, '../src/data'),
    '@/utils': path.resolve(__dirname, '../src/utils'),
    '@/styles': path.resolve(__dirname, '../src/styles'),
    '@/hooks': path.resolve(__dirname, '../src/hooks')
  }
}
```

### 3.5 平台条件编译处理

**问题**：源码有 `#ifdef H5` 和 `#ifdef MP-WEIXIN` 指令。Vite 端不识别这些指令。

**方案**：Vite 端**保留所有代码**，但用运行时判断确保小程序专属代码不出错：

```tsx
// 例：TabBar.tsx 源码
// #ifdef MP-WEIXIN
class TabBar extends Component { ... }
// #endif

// #ifdef H5
const TabBar = () => null
// #endif
```

Vite 编译时两个分支都会保留，触发 `Identifier 'TabBar' has already been declared` 错误。

**解决方案**：
1. 方案 A：写一个 babel 插件处理 `#ifdef` 指令
2. 方案 B：在源码中**手工把 H5 端的 `const TabBar` 改名**为 `TabBarH5`，通过 alias 解决
3. 方案 C：把 H5 端的 `TabBar` 移到 `web/components/TabBar.tsx`，通过 alias 覆盖

**采用方案 C**（最简单）：

```ts
// web/vite.config.ts
resolve: {
  alias: [
    // 全局 alias
    { find: '@tarojs/components', replacement: path.resolve(__dirname, 'adapters/index.ts') },
    // 文件级 alias - 覆盖 H5 端专属组件
    { find: '@/components/TabBar/TabBar', replacement: path.resolve(__dirname, 'components/TabBarH5/TabBar.tsx') }
  ]
}
```

但这种文件级 alias 维护成本高。**最终采用方案 A**：

写一个简单的 babel 插件，扫描源码中的 `#ifdef XXX` / `#endif` 注释，根据环境变量移除对应代码块。

```ts
// web/vite.config.ts
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [
    react({
      babel: {
        plugins: ['./babel-plugin-ifdef.js']
      }
    })
  ]
})
```

```js
// web/babel-plugin-ifdef.js
module.exports = function ({ types: t }) {
  return {
    visitor: {
      Program(path, state) {
        // 实现 #ifdef H5 / #ifdef MP-WEIXIN 处理
      }
    }
  }
}
```

**简化决策**：v2.0 阶段**先不动源码**，遇到 H5 端报错再单独修。预估 5-10 处。

### 3.6 样式系统

**问题**：源码 SCSS 中使用 px，Taro 编译时转 rpx。

**方案**：Vite 端**直接用 px**。源码 SCSS 不改。

Vite 默认支持 SCSS（需安装 `sass` 依赖）。

## 4. 实施步骤

### 阶段 1: 基础设施（1-2 小时）

1. 创建 `web/` 目录
2. 创建 `web/index.html` `main.tsx` `App.tsx`
3. 创建 `web/vite.config.ts`，配 alias
4. 安装依赖：`vite @vitejs/plugin-react react react-dom react-router-dom sass @types/node`

### 阶段 2: 适配层（1-2 小时）

1. 写 7 个适配组件（View/Text/Button/Image/Input/Textarea/ScrollView）
2. 写 `taro-shim.ts`（Taro.* API 桩）
3. 写 `web/adapters/index.ts` 统一导出

### 阶段 3: 路由 + Layout（2-3 小时）

1. 写 `Layout.tsx`（Sidebar + TopBar）
2. 写 `Sidebar.tsx` 19 个导航项
3. 写 `TopBar.tsx` 搜索 + 通知 + 用户
4. 配 19 个路由

### 阶段 4: 跑通核心页面（2-3 小时）

1. Dashboard / Radar / Quiz / Settings 4 个核心页面跑通
2. 修复 `Taro.*` API 调用问题
3. 修复样式兼容问题

### 阶段 5: 跑通全部页面（4-6 小时）

1. Interview Tracker / Mock Interview / Review / Kanban
2. Knowledge Graph（@xyflow/react 复杂页面）
3. Excerpt（之前踩过坑）
4. Settings / Not Found

### 阶段 6: 边角修复（2-4 小时）

1. 移动端响应式
2. 加载状态
3. 错误边界
4. 浏览器兼容

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| `@xyflow/react` 在 Vite 下报错 | 中 | 知识图谱页不可用 | 单独调试，优先保证其他页面 |
| 某些页面用了 Taro 独有 API | 中 | 部分页面挂掉 | 适配层兜底，少数单独修 |
| `react-markdown` SSR 兼容 | 低 | 构建失败 | 客户端渲染 |
| SCSS 嵌套编译错误 | 低 | 样式丢失 | 改用 CSS Modules 兜底 |
| localStorage 跨域访问 | 低 | 持久化失败 | 检查 origin |

## 6. 验证标准

### 6.1 启动验证

```bash
cd web
npm install
npm run dev
# 浏览器自动打开 http://localhost:5173
```

✅ 在 Chrome 中能看到 Dashboard 页面，左侧 Sidebar 可见。

### 6.2 路由验证

- ✅ 点击 Sidebar 各导航项能切换页面
- ✅ 浏览器前进/后退按钮工作
- ✅ URL 显示正确的路径

### 6.3 功能验证

- ✅ Dashboard：统计卡 + 进度条 + 快捷入口
- ✅ Quiz：题目展示 + 答题 + AI 评估 mock
- ✅ Mock Interview：配置 + 答题 + AI 追问
- ✅ Settings：LLM API 配置 + 主题切换 + 数据导出
- ✅ Interview Tracker：增删公司 + 状态切换 + 详情查看

### 6.4 持久化验证

- ✅ 新建面试公司后刷新页面，数据还在
- ✅ 答题进度在 localStorage 中
- ✅ 设置（主题、API Key）持久化

### 6.5 性能验证

- ✅ 首次加载 < 3s（开发模式）
- ✅ 路由切换 < 500ms
- ✅ HMR 修改 < 1s 反映

### 6.6 兼容性验证

- ✅ Chrome 90+ 正常
- ✅ Edge 90+ 正常
- ✅ Firefox 88+ 正常
- ✅ Safari 14+ 正常（如有 Mac）

## 7. 后续步骤

完成 H5 端 Vite 迁移后，下一步：

1. **后端 API 接入**：FastAPI + SQLite，H5 和小程序两端调用同一 API
2. **用户系统**：JWT 登录，两端数据合并
3. **PWA 支持**：离线使用、桌面安装
4. **生产部署**：Vercel / Netlify 自动部署

## 8. 工期

| 阶段 | 工期 |
|------|------|
| 阶段 1: 基础设施 | 1-2 小时 |
| 阶段 2: 适配层 | 1-2 小时 |
| 阶段 3: 路由 + Layout | 2-3 小时 |
| 阶段 4: 核心页面 | 2-3 小时 |
| 阶段 5: 全部页面 | 4-6 小时 |
| 阶段 6: 边角修复 | 2-4 小时 |
| **合计** | **12-20 小时（1-3 天）** |
