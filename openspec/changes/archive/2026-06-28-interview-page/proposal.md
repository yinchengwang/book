## Why

读书雷达项目目前覆盖了技术栈学习地图（雷达图）、知识测评（测验）、学习内容、仪表盘等功能，但缺少一个集中式的面试题库浏览页面。面试准备是程序员持续学习的核心场景之一，增加面试页面可以让用户在同一个平台内完成"学 → 测 → 面"的闭环。

## What Changes

- 新增 `interview.html` 独立页面，作为面试题库的浏览和学习界面
- 新增 `data/interview/interview-questions.js` 作为面试题数据文件（由 MD 源文件编译生成）
- 新增 `scripts/build-interview-questions.js` 编译脚本，将 MD 源文件转为 JS 数据文件
- 新增 `data/interview-questions/` 目录，按技术栈存放 MD 源文件（第二阶段由用户提供）
- 不修改 nav-component.js，面试页面为独立页面（不含 reading-radar 全局导航栏）

## Capabilities

### New Capabilities
- `interview-page-browse`: 面试题库浏览，支持分类筛选 + 技术栈侧栏切换、按重要度和访问次数排序、左侧导航中栏列表右栏详情的三栏布局
- `interview-question-detail`: 面试题详情展示，包括回答思路、参考回答、示例辅助理解（代码块高亮）、追问方向（可折叠），使用 marked.js 渲染 MD 内容
- `interview-questions-data`: 面试题数据管理，以 MD 文件为源、编译脚本生成 JS 数据文件的发布流水线

### Modified Capabilities
（无。面试页面为独立页面，不修改 reading-radar 现有导航栏）

## Impact

- `project/reading-radar/interview.html` — 新增页面（独立页面，不含全局导航栏）
- `project/reading-radar/data/interview/interview-questions.js` — 新增数据文件
- `project/reading-radar/scripts/build-interview-questions.js` — 新增编译脚本
- `project/reading-radar/data/interview-questions/` — 新增目录（MD 源文件）
