# 面试题库浏览

## Purpose

提供按类别和技术栈分类的面试题列表浏览功能，用户可切换类别过滤技术栈、从侧栏选择技术栈、切换排序方式，从列表中选择题目查看详情。

## ADDED Requirements

### Requirement: 类别筛选

侧栏顶部 SHALL 显示类别药丸（全部/语言/数据库/计算机基础/AI/其他），默认选中"全部"。选中某类别时，下方技术栈列表 SHALL 仅显示该类别下的技术栈。切换类别时自动选中该类别第一个技术栈。

#### Scenario: 切换类别

- **WHEN** 用户点击"数据库"类别
- **THEN** 技术栈列表 SHALL 仅显示 Redis、MySQL、Elasticsearch、MongoDB
- **THEN** 自动选中"Redis"技术栈

### Requirement: 技术栈侧栏导航

页面左侧 SHALL 显示垂直技术栈列表。每个技术栈项显示图标、名称和题目数量。默认选中第一个技术栈，切换技术栈时更新题目列表。桌面端侧栏固定宽度 190px，移动端转为水平滚动布局。

#### Scenario: 切换技术栈

- **WHEN** 用户点击侧栏中的"Redis"
- **THEN** 中栏列表 SHALL 只显示 Redis 分类下的题目
- **THEN** 该技术栈项 SHALL 处于 active 状态

### Requirement: 题目列表展示

中栏 SHALL 按行展示题目，每行包含：
- 题目标题
- 难度标签（入门/进阶/挑战，使用不同颜色）
- 重要度标签（🔥高频 / 📊中频 / 📌低频）

#### Scenario: 列表渲染

- **WHEN** 用户选择一个技术栈
- **THEN** 中栏列表 SHALL 渲染该技术栈下所有题目
- **THEN** 每条题目 SHALL 显示标题、难度标签、重要度标签

### Requirement: 排序切换

中栏头部 SHALL 提供排序下拉菜单，支持三种排序模式：
- **默认排序**：按数据文件中题目的排列顺序
- **重要度排序**：按高频 → 中频 → 低频降序排列
- **访问次数排序**：按 localStorage 中记录的点击次数降序排列

#### Scenario: 按重要度排序

- **WHEN** 用户选择排序方式为"重要度"
- **THEN** 列表 SHALL 按高频 > 中频 > 低频降序排列
- **WHEN** 多道题目重要度相同
- **THEN** 它们在组内保持数据文件中的相对顺序

#### Scenario: 按访问次数排序

- **WHEN** 用户选择排序方式为"访问次数"
- **THEN** 列表 SHALL 按点击次数降序排列
- **WHEN** 题目从未被点击过
- **THEN** 访问次数按 0 处理

### Requirement: 列表点击选择

用户点击列表中的某道题目时，该行 SHALL 高亮显示，右侧详情区 SHALL 切换为对应题目的详情内容。

#### Scenario: 选择题目

- **WHEN** 用户点击列表中"static 关键字的作用？"
- **THEN** 该行 SHALL 添加选中态背景色
- **THEN** 右侧详情区 SHALL 显示"static 关键字的作用？"的完整内容

### Requirement: 移动端自适应

当视口宽度 < 768px 时，侧栏 SHALL 折叠为水平滚动条（与类别药丸同行），三栏布局 SHALL 切换为纵向堆叠。当视口宽度 < 480px 时，列表和详情 SHALL 全屏切换，点击题目后列表隐藏、详情全屏显示。

#### Scenario: 窄屏选择题目

- **WHEN** 用户在宽度 < 480px 的设备上点击一道题目
- **THEN** 题目列表 SHALL 隐藏
- **THEN** 题目详情 SHALL 全屏显示
- **WHEN** 用户切换技术栈
- **THEN** 列表 SHALL 重新显示

### Requirement: 初始示例数据

每个技术栈 SHALL 包含至少 1 条示例题目，用于 UI 调试和功能验证。

#### Scenario: 示例数据存在

- **WHEN** 页面首次加载
- **THEN** 每个技术栈的列表 SHALL NOT 为空
- **THEN** 每个题目 SHALL 包含完整的 id、title、difficulty、priority、tags、body 字段
