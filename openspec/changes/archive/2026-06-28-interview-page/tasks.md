# 面试题库页面 — 任务清单

## 1. 数据层

- [x] 1.1 创建 `scripts/build-interview-questions.js` 编译脚本：递归扫描 `data/interview-questions/` 下所有 `.md` 文件，解析 YAML frontmatter（id/difficulty/priority/tags），提取 body 和 title，按 stack 分组，输出 `data/interview/interview-questions.js`
- [x] 1.2 创建 `data/interview/interview-questions.js`：包含 `CATEGORIES` 数组（6 个分类）、`INTERVIEW_STACKS` 数组（20 个技术栈，含 category 字段）、`INTERVIEW_QUESTIONS` 对象，每个技术栈至少 1 条 mock 数据
- [x] 1.3 编译脚本增加 frontmatter 校验：缺少 difficulty/priority 使用默认值并输出警告；empty guard 防止空 MD 目录覆盖已有数据

## 2. 面试页面骨架（独立页面）

- [x] 2.1 创建 `interview.html`：HTML 骨架，引入 marked.js（CDN）、highlight.js（CDN），不引入 nav-component.js
- [x] 2.2 实现自定义深色渐变 header（reading-radar 风格），页面完全自包含，无需全局导航栏

## 3. 侧栏导航（替代 Tab 栏）

- [x] 3.1 从 `CATEGORIES` 动态渲染类别药丸筛选器，默认选中"全部"
- [x] 3.2 从 `INTERVIEW_STACKS` 动态渲染技术栈侧栏列表，每项显示图标、名称、题目数量
- [x] 3.3 类别筛选联动：切换类别时，技术栈列表仅显示该类别下的栈，自动选中第一个栈
- [x] 3.4 技术栈切换事件：切换时中栏列表切换为该技术栈题目

## 4. 题目列表（中栏）

- [x] 4.1 根据当前选中的技术栈渲染题目列表，每行显示标题、难度标签（颜色区分）、重要度标签（🔥/📊/📌）
- [x] 4.2 实现排序切换下拉菜单：默认 / 重要度 / 访问次数
- [x] 4.3 实现访问次数记录：点击题目时 `localStorage` 递增计数（key: `interview_click_{questionId}`）
- [x] 4.4 列表选中高亮：当前选中的题目行有特殊背景色

## 5. 题目详情（右栏）

- [x] 5.1 使用 marked.js 将选中题目的 body 渲染为 HTML，展示四个板块（回答思路、参考回答、示例辅助理解、追问方向）
- [x] 5.2 追问方向按正则 `\d. ` 拆分，每个追问独立折叠，默认折叠
- [x] 5.3 集成 highlight.js，代码块自动语法高亮
- [x] 5.4 详情顶部显示难度标签和重要度标签（颜色样式与列表一致）

## 6. 移动端适配

- [x] 6.1 < 768px 时侧栏折叠为水平滚动条（与类别药丸同行），三栏切换为纵向堆叠
- [x] 6.2 < 480px 时列表与详情全屏切换：点击题目隐藏列表、全屏显示详情；切换技术栈时重新显示列表

## 7. 样式与 UI 主题

- [x] 7.1 使用 reading-radar 设计体系：深蓝紫渐变 header、#409eff 蓝色激活态、#f0f2f5 背景色
- [x] 7.2 定义侧栏、列表、详情区、难度标签、重要度标签等专属样式

## 8. 测试验证

- [x] 8.1 通过 Playwright 验证全部 20 个技术栈渲染正常
- [x] 8.2 验证分类筛选功能正确（每个类别过滤后栈数量正确）
- [x] 8.3 验证详情内容渲染正确（标题、难度、重要度、body）
- [ ] 8.4 编译脚本单元测试（empty guard、frontmatter 缺失、多文件合并）

## 9. 数据文件维护

- [ ] 9.1 用户提供 MD 文件后，运行 `node scripts/build-interview-questions.js` 编译
- [ ] 9.2 验证编译后的数据在页面上展示正确

## 统计

- 总计：31 子任务
- 已完成：29
- 待完成：2（数据录入工作）