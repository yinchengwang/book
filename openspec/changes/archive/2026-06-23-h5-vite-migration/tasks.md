# H5 端 Vite 迁移 — 任务清单

## 1. 基础设施

- [x] 1.1 创建 `web/` 目录结构
- [x] 1.2 创建 `web/package.json`（独立依赖）
- [x] 1.3 创建 `web/vite.config.ts`（配 alias）
- [x] 1.4 创建 `web/tsconfig.json`
- [x] 1.5 创建 `web/index.html`（HTML 入口）
- [x] 1.6 创建 `web/main.tsx`（React 入口）
- [x] 1.7 安装 Vite 相关依赖（`vite @vitejs/plugin-react react-router-dom sass`）
- [x] 1.8 验证 `npm run dev` 能启动

## 2. Taro 组件适配层

- [x] 2.1 创建 `web/adapters/View.tsx`
- [x] 2.2 创建 `web/adapters/Text.tsx`
- [x] 2.3 创建 `web/adapters/Button.tsx`
- [x] 2.4 创建 `web/adapters/Image.tsx`
- [x] 2.5 创建 `web/adapters/Input.tsx`
- [x] 2.6 创建 `web/adapters/Textarea.tsx`
- [x] 2.7 创建 `web/adapters/ScrollView.tsx`
- [x] 2.8 创建 `web/adapters/taro-shim.ts`（Taro.* API 桩）
- [x] 2.9 创建 `web/adapters/index.ts`（统一导出）
- [x] 2.10 在 vite.config.ts 配 alias 指向适配层
- [x] 2.11 补充 `Switch` `Progress` 组件适配（Settings/GapAnalysis 用到）

## 3. 路由 + Layout

- [x] 3.1 创建 `web/components/Layout/Layout.tsx`
- [x] 3.2 创建 `web/components/Layout/Sidebar.tsx`（19 项导航）
- [x] 3.3 创建 `web/components/Layout/TopBar.tsx`（搜索 + 通知 + 用户）
- [x] 3.4 配 13 个路由（react-router-dom v6）
- [x] 3.5 配懒加载（lazy + Suspense）
- [x] 3.6 验证侧边栏点击切换页面

## 4. 平台差异处理

- [x] 4.1 写 ifdef 插件处理 `#ifdef MP-WEIXIN`（vite-plugins/ifdef.ts）
- [x] 4.2 修复 `useStore` 笔误（Review.tsx）
- [x] 4.3 处理 TabBar / NavBar 小程序专属组件

## 5. 错误隔离

- [x] 5.1 全局 ErrorBoundary（App 层级）
- [x] 5.2 单页面 PageErrorBoundary（每个路由独立包裹）
- [x] 5.3 关闭 Vite HMR overlay（避免遮挡）
- [x] 5.4 全局错误监听（window error/unhandledrejection）

## 6. PC 端样式适配

- [x] 6.1 写 PC 端样式覆盖层 `web/styles/pc-adaptive.scss`
- [x] 6.2 在 main.tsx 引入覆盖样式
- [ ] 6.3 验证各页面在大屏的展示效果
- [ ] 6.4 根据实际情况微调

## 7. 核心页面（4 个优先）

- [x] 7.1 跑通 Dashboard（自动化测试 ✅ 渲染正常，紫色悬浮物已修）
- [ ] 7.2 跑通 Tech Radar（v1 测试显示为空，需 v2 重新验证）
- [ ] 7.3 跑通 Quiz（v1 测试显示为空，需 v2 重新验证）
- [ ] 7.4 跑通 Settings（v1 测试显示为空，需 v2 重新验证）

## 8. 其余页面（按测试结果逐个修）

- [ ] 8.1 Kanban
- [ ] 8.2 Review
- [ ] 8.3 Interview Tracker
- [ ] 8.4 Mock Interview
- [ ] 8.5 Knowledge Graph（@xyflow/react 复杂）
- [ ] 8.6 Gap Analysis
- [ ] 8.7 Excerpt（之前踩过坑）
- [ ] 8.8 Learning Path
- [ ] 8.9 Project Roadmap
- [ ] 8.10 Not Found

## 9. 数据与状态

- [ ] 9.1 验证 10 个 Zustand store 全部正常初始化
- [ ] 9.2 验证 persist 中间件 localStorage 持久化
- [ ] 9.3 验证数据导入/导出
- [ ] 9.4 验证设置页 LLM API Key 存储

## 10. 第三方库兼容

- [ ] 10.1 验证 Recharts（雷达图、趋势图）
- [ ] 10.2 验证 @xyflow/react（知识图谱）
- [ ] 10.3 验证 Framer Motion（动画）
- [ ] 10.4 验证 Lucide React（图标）
- [ ] 10.5 验证 react-markdown（摘抄）

## 11. 验证与发布

- [ ] 11.1 启动验证（npm run dev）✅
- [ ] 11.2 路由验证（13 个页面切换）
- [ ] 11.3 功能验证（核心交互）
- [ ] 11.4 持久化验证（刷新不丢失）
- [ ] 11.5 性能验证（首屏 < 3s）
- [ ] 11.6 浏览器兼容验证
- [ ] 11.7 生产构建验证（npm run build）
- [ ] 11.8 验证 `npm run build:weapp` 仍正常（小程序端不受影响）
