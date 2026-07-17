# Capability: h5-vite-build

## Purpose

为 Reading Radar 2.0 提供基于 Vite 的 H5 端独立编译能力，与 Taro 微信小程序编译链并行运行。

## Requirements

### R1: 独立编译链

H5 端必须通过 Vite 编译运行，**不依赖 Taro 编译链**。

```bash
cd reading-radar-2/web
npm install
npm run dev      # 开发模式，HMR
npm run build    # 生产构建，产物在 web/dist/
```

### R2: 源码共享

`reading-radar-2/src/` 下 90% 的代码（H5 端运行时需要）必须**不修改**即可被 Vite 工程引用：

- `src/pages/` 19 个页面
- `src/components/` 通用组件
- `src/stores/` Zustand 状态
- `src/data/` 静态数据
- `src/utils/` 工具函数
- `src/styles/` SCSS 样式

### R3: Taro 组件适配

`@tarojs/components` 的导入必须通过 Vite alias 映射到 web 端适配层，**源码不修改**：

| Taro 组件 | H5 映射 |
|----------|---------|
| `<View>` | `<div>` |
| `<Text>` | `<span>` |
| `<Button>` | `<button>` |
| `<Image>` | `<img>` |
| `<Input>` | `<input>` |
| `<Textarea>` | `<textarea>` |
| `<ScrollView>` | `<div style="overflow:auto">` |

### R4: 路由系统

必须使用 `react-router-dom v6` 替代 Taro 路由：

- 19 个页面一对一映射到路由
- Sidebar 导航点击触发路由切换
- 支持浏览器前进/后退
- 路径形如 `/dashboard` `/quiz` `/mock-interview`

### R5: 状态管理

Zustand store 必须**直接复用**，不修改：

- `persist` 中间件使用 localStorage 持久化
- H5 端天然支持，零适配成本

### R6: 样式系统

SCSS 样式通过 Vite sass 插件处理：

- 源码中的 `px` 单位**不转换**（H5 直接用 px）
- 玻璃拟态、动画、CSS 变量全部直接生效
- 暗黑主题作为默认

### R7: 第三方库兼容

以下库在 Vite 端必须可用：

- `recharts` — 雷达图、趋势图
- `@xyflow/react` — 知识图谱
- `framer-motion` — 动画
- `lucide-react` — 图标
- `react-markdown` — Markdown 渲染
- `zustand` — 状态管理

### R8: 微信小程序编译不受影响

Taro 对微信小程序的编译链（`npm run build:weapp`）必须**完全正常**：

- `config/` 目录不动
- Taro 相关依赖（`@tarojs/*`）保留
- 19 个页面在两种编译链下**都能编译**

## Out of Scope

- 后端 API 接入（下一个变更）
- 用户系统（下一个变更）
- 移动端适配（小程序的领域）
- SSR / SSG（H5 端 MVP 不需要）
