# Reading Radar 2.0 实现任务清单

## 1. 项目初始化

- [x] 1.1 创建 Taro 项目 `npx create-taro@latest reading-radar-2`
- [x] 1.2 选择 React + TypeScript + TailwindCSS 模板
- [x] 1.3 配置 ESLint + Prettier + Husky
- [x] 1.4 配置 TypeScript strict mode
- [x] 1.5 安装核心依赖：Recharts, @xyflow/react, Framer Motion, Zustand, Lucide React, react-markdown
- [x] 1.6 配置 Tailwind CSS（参考 Coolearn 的 tailwind.config.js）
- [x] 1.7 创建组件目录结构
- [ ] 1.8 初始化 Git 仓库

## 2. 设计系统组件

- [x] 2.1 创建 Button 组件（primary/secondary/ghost variants）
- [x] 2.2 创建 Card 组件（glass/blur 效果）
- [x] 2.3 创建 Input 组件（text/textarea/search）
- [x] 2.4 创建 Badge/Tag 组件
- [x] 2.5 创建 Modal/Drawer 组件
- [x] 2.6 创建 Select/Dropdown 组件
- [x] 2.7 创建 Tabs 组件
- [x] 2.8 创建 Progress 组件
- [x] 2.9 创建 Avatar 组件
- [x] 2.10 创建 Empty 状态组件
- [x] 2.11 创建 Loading/Spinner 组件
- [x] 2.12 创建 Tooltip 组件

## 3. 布局组件

- [x] 3.1 创建 Layout 根组件
- [x] 3.2 创建 Sidebar 组件（可折叠）
- [x] 3.3 创建 Header 组件
- [x] 3.4 创建 TabBar 组件（小程序）
- [x] 3.5 创建 PageContainer 组件
- [x] 3.6 创建 Responsive 断点工具
- [x] 3.7 创建 SafeArea 组件

## 4. 路由与导航

- [x] 4.1 配置 React Router（Web）
- [x] 4.2 配置 Taro 路由（小程序）
- [x] 4.3 创建路由配置文件
- [x] 4.4 实现 404 页面
- [x] 4.5 实现页面守卫（未完成功能提示）

## 5. 状态管理

- [x] 5.1 配置 Zustand store
- [x] 5.2 创建 quizStatsStore（测验数据）
- [x] 5.3 创建 knowledgeStore（知识数据）
- [x] 5.4 创建 userStore（用户设置）
- [x] 5.5 创建 reviewStore（复习数据）
- [x] 5.6 配置 persist 中间件（localStorage）
- [x] 5.7 实现 store 初始化逻辑

## 6. 数据层

- [x] 6.1 创建 KNOWLEDGE_DATA 数据结构（迁移 items-registry.js）
- [x] 6.2 创建 QUIZ_QUESTIONS 题库
- [x] 6.3 创建 DOMAIN_CONFIG 领域配置
- [x] 6.4 创建 BOOK_DATA 读书数据
- [x] 6.5 创建 DataService 服务层
- [x] 6.6 实现数据导出功能（JSON/Markdown/CSV）
- [ ] 6.7 预留 CloudService 接口（云端同步）

## 7. 仪表盘页面 (Dashboard)

- [x] 7.1 创建 Dashboard 页面组件
- [x] 7.2 实现全局统计卡片
- [x] 7.3 实现技术栈进度条
- [x] 7.4 实现成长趋势图（Recharts）
- [x] 7.5 实现热力图日历
- [x] 7.6 实现快捷入口
- [x] 7.7 实现数字递增动画

## 8. 技术雷达页面 (TechRadar)

- [x] 8.1 创建 TechRadar 页面组件
- [x] 8.2 实现雷达图（Recharts RadarChart）
- [x] 8.3 实现技术栈切换 Tab
- [x] 8.4 实现知识详情弹窗
- [x] 8.5 实现 Legend 图例
- [x] 8.6 实现响应式适配

## 9. 知识看板页面 (Kanban)

- [x] 9.1 创建 Kanban 页面组件
- [x] 9.2 实现四象限看板布局
- [x] 9.3 创建 KanbanCard 组件
- [x] 9.4 实现拖拽功能（@dnd-kit）
- [x] 9.5 实现看板分组切换
- [x] 9.6 实现卡片操作菜单
- [x] 9.7 实现新增卡片表单
- [x] 9.8 实现空状态提示

## 10. 测验系统页面 (Quiz)

- [x] 10.1 创建 Quiz 页面组件
- [x] 10.2 实现题库浏览列表
- [x] 10.3 实现筛选功能（分类/领域/难度）
- [x] 10.4 实现测验答题界面
- [x] 10.5 实现语音输入功能
- [x] 10.6 实现 AI 批改接口（预留）
- [x] 10.7 实现统计面板
- [x] 10.8 实现错题本
- [x] 10.9 实现掌握度追踪

## 11. 复习系统页面 (Review)

- [x] 11.1 创建 Review 页面组件
- [x] 11.2 实现 SM-2 算法
- [x] 11.3 创建复习卡片组件
- [x] 11.4 实现复习队列管理
- [x] 11.5 实现复习统计
- [x] 11.6 实现复习设置
- [x] 11.7 实现复习历史

## 12. 知识图谱页面 (KnowledgeGraph)

> ⚠️ 仅 H5 端，小程序降级为列表视图

- [x] 12.1 创建 KnowledgeGraph 页面组件（#ifndef MP-WEIXIN）
- [x] 12.2 创建 KnowledgeList 页面组件（#ifdef MP-WEIXIN）
- [x] 12.3 配置 @xyflow/react
- [x] 12.4 实现图谱节点渲染
- [x] 12.5 实现图谱边渲染
- [x] 12.6 实现节点交互（点击/拖拽）
- [x] 12.7 实现图谱布局算法
- [x] 12.8 实现筛选控制
- [x] 12.9 实现缩放/平移功能

## 13. 差距分析页面 (GapAnalysis)

- [x] 13.1 创建 GapAnalysis 页面组件
- [x] 13.2 实现领域掌握度分析
- [x] 13.3 实现薄弱点列表
- [x] 13.4 实现智能推荐算法
- [x] 13.5 实现诊断自测
- [x] 13.6 实现学习目标设定
- [x] 13.7 实现差距可视化

## 14. 摘抄管理页面 (Excerpt)

- [x] 14.1 创建 Excerpt 页面组件
- [x] 14.2 实现摘抄列表（书籍/标签/时间视图）
- [x] 14.3 实现摘抄详情弹窗
- [x] 14.4 实现 Markdown 编辑器
- [x] 14.5 实现 Obsidian 双向链接解析
- [x] 14.6 实现书籍管理
- [x] 14.7 实现标签系统
- [x] 14.8 实现搜索功能

## 15. 学习路径页面 (LearningPath)

- [x] 15.1 创建 LearningPath 页面组件
- [x] 15.2 实现路径可视化
- [x] 15.3 实现步骤详情面板
- [x] 15.4 实现路径配置
- [x] 15.5 实现学习推荐
- [x] 15.6 实现路径统计

## 16. 面试追踪页面 (InterviewTracker)

- [x] 16.1 创建 InterviewTracker 页面组件
- [x] 16.2 实现公司列表
- [x] 16.3 实现投递详情
- [x] 16.4 实现面试状态机
- [x] 16.5 实现面试记录
- [x] 16.6 实现面经分享
- [x] 16.7 实现统计面板

## 17. 模拟面试页面 (MockInterview) - 预埋

- [x] 17.1 创建 MockInterview 页面组件
- [x] 17.2 实现面试配置
- [x] 17.3 实现面试流程
- [x] 17.4 预留 AI 追问接口
- [x] 17.5 预留面试评估接口
- [x] 17.6 实现题库选择
- [x] 17.7 实现面试记录

## 18. 项目路线页面 (ProjectRoadmap) - 预埋

- [x] 18.1 创建 ProjectRoadmap 页面组件
- [x] 18.2 实现项目列表
- [x] 18.3 实现项目详情
- [x] 18.4 实现项目卡片
- [x] 18.5 实现项目创建
- [x] 18.6 实现关联知识

## 19. 设置页面 (Settings)

- [x] 19.1 创建 Settings 页面组件
- [x] 19.2 实现 LLM API 配置
- [x] 19.3 实现主题切换（深色/浅色）
- [x] 19.4 实现数据管理（导出/导入/清空）
- [x] 19.5 实现通知设置
- [x] 19.6 实现关于页面

## 20. 多端适配

- [x] 20.1 配置 Taro tabBar（小程序）
- [x] 20.2 实现条件编译（平台差异）
- [x] 20.3 适配刘海屏/安全区域
- [x] 20.4 优化 H5 端样式
- [x] 20.5 优化小程序端样式
- [x] 20.6 实现 WebView 组件
- [x] 20.7 实现图片跨端处理

## 21. 数据迁移

- [x] 21.1 创建数据迁移脚本
- [x] 21.2 迁移 items-registry.js → knowledgeData.ts
- [x] 21.3 迁移题库 → quizQuestions.ts
- [x] 21.4 迁移读书数据 → bookData.ts
- [x] 21.5 迁移 server.js API → service 层
- [x] 21.6 验证迁移完整性

## 22. 性能优化

- [x] 22.1 实现路由懒加载
- [x] 22.2 实现组件懒加载
- [x] 22.3 配置 Tree Shaking
- [x] 22.4 压缩图片资源
- [x] 22.5 优化首屏加载
- [x] 22.6 优化动画帧率

## 23. 测试与发布

- [x] 23.1 编写核心组件单元测试
- [x] 23.2 编写页面 E2E 测试（Playwright）
- [x] 23.3 H5 端测试与发布
- [x] 23.4 小程序端测试与发布
- [x] 23.5 编写使用文档
- [x] 23.6 清理老项目文件（已迁移的摘抄 .txt 文件已在之前提交中清理）
