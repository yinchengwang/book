# 自适应学习路线图

## Purpose

为 2026 筑基年提供 12 个月的月度学习计划数据，支持按难度分级的编排和提前完成后的自适应调整（pull-forward），让学习节奏与用户实际进度匹配。

## ADDED Requirements

### Requirement: 月度学习路线图数据

系统 SHALL 新建 `data/learn-roadmap.js` 文件，定义 `LEARNING_ROADMAP` 对象。该对象以年份为 key（如 `"2026"`），每个年份包含 `theme`（年主题）和 `months`（12 个月的月度计划）。

每个月份 SHALL 包含 `label`（月度主题名称）和 `items`（知识点清单）。每个知识点 SHALL 标注：
- `id`：知识点 ID，与看板/题库 ID 一致
- `stack`：所属技术栈（linux/db/c/ds/cpp/py）
- `difficulty`：难度等级（easy/medium/hard）
- `estDays`：预估学习天数

#### Scenario: 路线图数据语法正确

- **WHEN** 执行 `node --check learn-roadmap.js`
- **THEN** 退出码 SHALL 为 0

#### Scenario: 路线图 ID 与看板 ID 匹配

- **WHEN** 用 `learn-roadmap.js` 中的知识点 ID 查找 `kanban-data.js` 或 `quiz-tech.js` 中对应条目
- **THEN** 每个 ID 均 SHALL 能找到匹配的知识点

### Requirement: 优先级驱动的月度编排

2026 年 12 个月的月度计划 SHALL 按以下优先级排列学习内容：
1. DB（数据库深化）— 25%
2. Linux（新学）— 35%
3. C（深化）— 15%
4. DS（补理论）— 15%
5. C++（现代特性）— 5%
6. Python（工具链）— 5%

每月 SHALL 包含 7-10 个知识点，easy 知识点每周 3-4 个，medium 每周 2 个，hard 每周 1 个。

#### Scenario: 各优先级均有覆盖

- **WHEN** 查看 2026 年全年学习计划
- **THEN** 六个技术栈的占比 SHALL 大致符合优先级排列

### Requirement: 自适应计划检查

five-year-plan.html SHALL 在渲染当月学习清单时，检查当月所有 item 的 kanban 状态是否均为 `done`。若全部完成，SHALL 弹出确认对话框。

状态检查 SHALL 复用 localStorage 中已有的看板数据（`learning-kanban-<stack>` key），不创建新的数据存储。

#### Scenario: 当月全部完成时弹出确认

- **WHEN** 用户在当月将所有学习计划知识点都标记为 done
- **THEN** 页面 SHALL 显示弹窗："🎉 本月计划提前完成！"
- **THEN** 弹窗 SHALL 提供三个选项：提前开始下月内容 / 复习本月薄弱点 / 不再提醒

### Requirement: Pull-Forward 前移机制

用户选择"提前开始下月内容"后，系统 SHALL 将下月所有 item 的 plannedMonth 改为当前月，后续月份依次前移。前移状态 SHALL 持久化到 localStorage（`learning-roadmap-state` key）。

#### Scenario: Pull-Forward 持久化

- **WHEN** 用户选择提前开始下月内容并刷新页面
- **THEN** 前移后的月度计划 SHALL 保持不变，不会回退到原始计划

### Requirement: 难度分级标准

每个知识点的 `difficulty` 字段 SHALL 遵循以下标准：
- `easy`：纯理论学习，2 天内可掌握（如 /proc 文件系统阅读、基础概念理解）
- `medium`：需要动手实验或代码练习，3-5 天可掌握（如 perf 使用、fio 基准测试）
- `hard`：需要深入源码级理解或多知识点串联，6 天以上（如 io_uring 深度、eBPF 开发）

#### Scenario: 难度标记一致

- **WHEN** 比较同类知识点的难度标记
- **THEN** 同类型、同深度的知识点 SHALL 具有相同的 difficulty 值

### Requirement: 年末弹性缓冲月

12 月 SHALL 设置为弹性缓冲月，包含 Python 工具链补缺内容（4-5 个 easy 知识点），不安排 hard 知识点。若全年计划提前完成，缓冲月被压缩或前移至更早月份。

#### Scenario: 12月为缓冲月

- **WHEN** 查看 LEARING_ROADMAP 中 2026 年 12 月的学习计划
- **THEN** 12 月 SHALL 仅包含 easy 难度知识点，数量不超过 5 个
