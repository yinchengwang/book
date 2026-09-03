# h5-vite-build

## Purpose

为 Reading Radar 2.0 提供基于 Vite 的 H5 端独立编译能力，与 Taro 微信小程序编译链并行运行。

## Requirements

### Requirement: 独立编译链

H5 端必须通过 Vite 编译运行，**不依赖 Taro 编译链**。

```bash
cd reading-radar-2/web
npm install
npm run dev      # 开发模式，HMR
npm run build    # 生产构建，产物在 web/dist/
```

#### Scenario: H5 端独立开发

- **WHEN** 开发者在 `web/` 目录下执行 `npm run dev`
- **THEN** Vite SHALL 启动开发服务器，监听 5173 端口，启用 HMR

#### Scenario: H5 端生产构建

- **WHEN** 开发者执行 `npm run build`
- **THEN** Vite SHALL 输出产物到 `web/dist/`，不依赖 Taro 编译链

### Requirement: 源码共享

`reading-radar-2/src/` 下 90% 的代码（H5 端运行时需要）必须**不修改**即可被 Vite 工程引用：

- `src/pages/` 19 个页面
- `src/components/` 通用组件
- `src/stores/` Zustand 状态
- `src/data/` 静态数据
- `src/utils/` 工具函数
- `src/styles/` SCSS 样式

#### Scenario: 业务页面被 Vite 引用

- **WHEN** Vite 启动并解析 `src/pages/quiz/Quiz.tsx`
- **THEN** 通过 Vite alias (`@/pages/...`) SHALL 直接引用根目录 `src/pages/quiz/Quiz.tsx`，无需任何文件修改

### Requirement: Taro 组件适配

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

#### Scenario: View 组件在 H5 渲染

- **WHEN** 页面使用 `<View className="container">`
- **THEN** H5 端 SHALL 渲染为 `<div class="container">`，样式正常生效

#### Scenario: 事件格式兼容

- **WHEN** 页面使用 `<Input onInput={(e) => ...}>` 形式（Taro 风格 `e.detail.value`）
- **THEN** H5 适配层 SHALL 将原生 `change` 事件转换为 `{ detail: { value } }` 格式

### Requirement: 路由系统

必须使用 `react-router-dom v6` 替代 Taro 路由：

- 19 个页面一对一映射到路由
- Sidebar 导航点击触发路由切换
- 支持浏览器前进/后退
- 路径形如 `/dashboard` `/quiz` `/mock-interview`

#### Scenario: 路由懒加载

- **WHEN** 用户首次访问 `/quiz`
- **THEN** 路由 SHALL 懒加载 `@/pages/quiz/Quiz` 模块，生成独立 chunk

#### Scenario: 浏览器后退

- **WHEN** 用户点击浏览器后退按钮
- **THEN** 路由 SHALL 切回上一个页面，组件状态不丢失

### Requirement: 状态管理

Zustand store 必须**直接复用**，不修改：

- `persist` 中间件使用 localStorage 持久化
- H5 端天然支持，零适配成本

#### Scenario: 状态持久化

- **WHEN** H5 端页面修改 quizStore 的 answers 数组
- **THEN** Zustand persist SHALL 自动写入 localStorage 的 `quiz-storage` key

#### Scenario: 状态跨页面同步

- **WHEN** 在 `/quiz` 页面答题后跳转到 `/dashboard`
- **THEN** 仪表盘 SHALL 显示刚才的答题数据

### Requirement: 样式系统

SCSS 样式通过 Vite sass 插件处理：

- 源码中的 `px` 单位**不转换**（H5 直接用 px）
- 玻璃拟态、动画、CSS 变量全部直接生效
- 暗黑主题作为默认

#### Scenario: px 单位直接生效

- **WHEN** SCSS 文件包含 `padding: 16px;`
- **THEN** H5 端 SHALL 直接渲染为 16 像素，无需 Taro 的 rpx 转换

### Requirement: 第三方库兼容

以下库在 Vite 端必须可用：

- `recharts` — 雷达图、趋势图
- `@xyflow/react` — 知识图谱
- `framer-motion` — 动画
- `lucide-react` — 图标
- `react-markdown` — Markdown 渲染
- `zustand` — 状态管理

#### Scenario: 图表组件加载

- **WHEN** `/radar` 页面使用 recharts 渲染雷达图
- **THEN** Vite SHALL 正确解析 ESM 模块，组件正常渲染

### Requirement: 微信小程序编译不受影响

Taro 对微信小程序的编译链（`npm run build:weapp`）必须**完全正常**：

- `config/` 目录不动
- Taro 相关依赖（`@tarojs/*`）保留
- 19 个页面在两种编译链下**都能编译**

#### Scenario: 小程序独立构建

- **WHEN** 开发者执行 `npm run build:weapp`
- **THEN** Taro SHALL 编译小程序产物到 `dist/`，不依赖 web 目录