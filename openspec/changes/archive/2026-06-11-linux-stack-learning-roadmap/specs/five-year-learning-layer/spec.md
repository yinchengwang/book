# 五年计划月度学习层

## Purpose

在 five-year-plan.html 中新增月度学习清单区块、弹窗"今日学习"选择区、日历学习标记点，将看板学习进度与五年计划日常打卡整合。不替换现有五大工程打卡系统，作为增量功能叠加。

## ADDED Requirements

### Requirement: 月度学习清单区块

five-year-plan.html SHALL 在 theme-card 下方、stats-row 上方渲染月度学习清单区块 `monthly-learning`。该区块 SHALL 显示当月主题名称、每个知识点的标题及看板状态、完成进度百分比。

知识点看板状态 SHALL 通过 localStorage 中 `learning-kanban-<stack>` key 读取，状态映射为：
- `done` → "✅ 已掌握"
- `doing` → "📖 进行中"
- `review` → "🔄 待复习"
- `todo` 或未设置 → "📥"

#### Scenario: 学习清单随月份切换更新

- **WHEN** 用户在五年计划页面切换月份
- **THEN** 月度学习清单 SHALL 自动切换到对应月份的内容
- **THEN** 各知识点的看板状态 SHALL 反映最新的 localStorage 数据

### Requirement: 弹窗"今日学习"选择区

five-year-plan.html 的打卡弹窗 SHALL 在五大工程勾选框下方新增"📚 今日学习"区域。该区域 SHALL 从当月学习清单中读取知识点，每个知识点显示为一个 toggle chip（可点击切换选中/未选中状态）。

#### Scenario: 勾选学习 chip 联动看板

- **WHEN** 用户在弹窗中勾选某学习 chip 并保存
- **THEN** 对应知识点的 kanban 状态 SHALL 更新为 `doing`（若之前为 `todo`）
- **THEN** 该知识点的测验最高分 SHALL 不影响状态更新

#### Scenario: 空学习清单的优雅处理

- **WHEN** 当前月份的学习清单为空（learn-roadmap.js 未定义该月份）
- **THEN** "📚 今日学习"区域 SHALL 显示"本月暂无学习计划"提示
- **THEN** 系统 SHALL NOT 抛出 JavaScript 异常

### Requirement: 日历学习标记点

每个 day-cell 的 dot-row 中 SHALL 增加一个学习标记点（蓝色 dot），区别于五大工程点（绿/橙/灰）。该标记点 SHALL 根据当天是否有学习活动记录显示不同状态：
- 蓝色实心：当天有学习活动记录
- 浅灰空心：当天无学习活动记录

#### Scenario: 日历正确显示学习标记

- **WHEN** 用户在某天的弹窗中勾选了至少一个学习 chip 并保存
- **THEN** 该天日历格子的学习标记点 SHALL 显示为蓝色实心

### Requirement: 学习块完成度视觉指标

月度学习清单中每个知识点行 SHALL 显示测验平均分（从 `quiz_<stack>_hist_<itemId>` 读取最近 3 次平均分），以及看板状态标签。

#### Scenario: 完成百分比计算

- **WHEN** 当月学习清单中有 8 个知识点，其中 3 个状态为 done
- **THEN** 月度学习清单头部 SHALL 显示 "3/8 完成 · 平均分 85"

### Requirement: 与现有打卡系统互不干扰

新增的学习层 SHALL 不修改五大工程的打卡逻辑。用户选择不使用学习路线图时，现有打卡系统 SHALL 照常运行。`monthly-learning` 区块在 `learn-roadmap.js` 加载失败时 SHALL 不渲染，但不影响其他区块。

#### Scenario: 学习层独立降级

- **WHEN** `data/learn-roadmap.js` 加载失败（文件不存在或语法错误）
- **THEN** 五年计划页面 SHALL 正常显示五大工程打卡系统
- **THEN** `monthly-learning` 区块 SHALL 被隐藏
- **THEN** 弹窗中"📚 今日学习"区域 SHALL 被隐藏
