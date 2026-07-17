# 面试追踪状态体系规范

## ADDED Requirements

### Requirement: 14 种面试状态定义
系统必须定义 14 种面试状态，每种状态包含 ID、标签、图标、颜色和所属阶段分组。

#### Scenario: 状态完整列表
- **WHEN** 系统初始化状态常量
- **THEN** 系统定义以下 14 种状态：意向公司(intention)、沟通中(communicating)、已投递(applied)、简历筛选(resume_screen)、一面(first_interview)、二面(second_interview)、三面(third_interview)、主管面(manager_interview)、HR面(hr_interview)、薪资谈判(salary_negotiation)、背调(background_check)、Offer(offer)、已接受(accepted)、已拒绝(rejected)

#### Scenario: 状态分组
- **WHEN** 系统渲染状态相关 UI
- **THEN** 系统将状态分为 5 个组：前期(pre, 3 种)、筛选(screen, 1 种)、技术面(tech, 3 种)、终面(final, 4 种)、结果(result, 3 种)

### Requirement: 状态颜色体系
每种状态必须有唯一的颜色值，同组状态可共享颜色家族。

#### Scenario: 前期状态颜色
- **WHEN** 渲染前期状态
- **THEN** 意向公司使用灰色(#95a5a6)，沟通中和已投递使用蓝色(#5c9ce6)

#### Scenario: 技术面状态颜色
- **WHEN** 渲染技术面状态
- **THEN** 一面、二面、三面均使用绿色(#27ae60)

#### Scenario: 终面状态颜色
- **WHEN** 渲染终面状态
- **THEN** 主管面使用紫色(#9b59b6)，HR面使用紫色(#9b59b6)，薪资谈判和背调使用红色(#e74c3c)

#### Scenario: 结果状态颜色
- **WHEN** 渲染结果状态
- **THEN** Offer 使用金色(#f39c12)，已接受使用翠绿(#2ecc71)，已拒绝使用浅灰(#bdc3c7)

### Requirement: 状态流转逻辑
系统必须支持状态之间的顺序流转，并提供状态切换 UI。

#### Scenario: 状态按序前进
- **WHEN** 用户将公司从"一面"切换到"二面"
- **THEN** 系统更新公司状态为 second_interview，更新日期为当前时间

#### Scenario: 状态可回退
- **WHEN** 用户将公司从"二面"切换回"一面"
- **THEN** 系统允许回退，更新状态为 first_interview

#### Scenario: 状态变更记录历史
- **WHEN** 用户每次变更公司状态
- **THEN** 系统在 `_meta.md` 的 frontmatter 中追加状态变更历史（含旧状态、新状态、变更日期）

### Requirement: 状态快捷切换面板
在公司详情面板中必须提供状态快捷切换按钮。

#### Scenario: 详情面板显示状态按钮
- **WHEN** 打开公司详情面板
- **THEN** 面板顶部显示 14 个状态按钮，当前状态按钮高亮

#### Scenario: 点击状态按钮切换
- **WHEN** 用户在详情面板中点击某个状态按钮
- **THEN** 系统更新公司状态，刷新卡片视图和统计概览

### Requirement: 状态筛选语义
快捷筛选条中的"流程中"和"待回复"必须有明确的语义定义。

#### Scenario: 流程中定义
- **WHEN** 用户筛选"流程中"
- **THEN** 系统显示状态为 intention 至 background_check 之间（不含 offer/accepted/rejected）的所有公司

#### Scenario: 待回复定义
- **WHEN** 用户筛选"待回复"
- **THEN** 系统显示状态为 applied 或 resume_screen（已投递或简历筛选，等待对方回复）的公司
