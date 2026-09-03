# 测验系统模块化（部分提取）

## Purpose

从 quiz-system.html（2909 行单体文件）中提取可独立模块化的部分：quiz-core.js 承载 QuizState 状态管理和题库加载，quiz-ui.js 承载 QuizUI 渲染对象。quiz-system.html 保留答题逻辑/计时器/服务器同步等核心功能（因深度耦合待后续迭代）。保持全局状态通过 `window` 对象可访问，确保拆分前后行为一致。

## Requirements

### Requirement: quiz-core.js 状态管理

系统 SHALL 在 `data/quiz-core.js` 中定义 `window.QuizState` 对象，集中管理所有测验状态：`currentCategory`、`currentQuadrant`、`currentRing`、`questions`、`currentIndex`、`answers`、`score`、`mode`。

#### Scenario: QuizState 全局可访问

- **WHEN** quiz-system.html 加载 `quiz-core.js`
- **THEN** `window.QuizState` SHALL 存在且包含上述所有字段
- **THEN** 浏览器控制台中可读写 `QuizState.currentCategory` 等属性

### Requirement: quiz-core.js 题库加载

系统 SHALL 在 quiz-core.js 中提供 `loadQuestionBank(categoryId)` 函数，加载并合并指定技术栈的题库数据。当题库为空对象时，SHALL 返回 `{ empty: true, message: "题库暂未开放" }`。

#### Scenario: 空题库优雅降级

- **WHEN** 调用 `loadQuestionBank("linux")` 且 QUESTION_BANK['linux'] 为空对象
- **THEN** SHALL 返回 `{ empty: true }`，不抛出异常

### Requirement: quiz-ui.js 渲染函数

系统 SHALL 在 `data/quiz-ui.js` 中定义 `window.QuizUI` 对象，提供所有 UI 渲染函数：`renderCategorySelector()`、`renderQuadrantSelector()`、`renderQuestion()`、`renderResult()`、`showEmptyBankNotice()`、`bindEvents()`。

#### Scenario: 空题库提示 UI

- **WHEN** QuizUI.showEmptyBankNotice() 被调用
- **THEN** SHALL 在页面中显示"题库暂未开放，敬请期待"提示信息
- **THEN** SHALL NOT 显示任何报错信息

### Requirement: quiz-system.html 保留内联核心（部分提取）

quiz-system.html 保留计时器/服务器同步/答题判分/项目管理等核心模块的内联实现（因与页面深度耦合），小幅增长至 2922 行（因 nav-component 脚本标签引入）。后续迭代继续将核心逻辑迁移到 quiz-core.js，目标缩减至 ~500 行。

#### Scenario: 测验功能完整

- **WHEN** 用户完成一次完整的测验流程（选择技术栈 → 选择象限 → 答题 → 查看结果）
- **THEN** SHALL 与拆分前的行为完全一致，包括题目渲染、交互反馈、计分和错题回顾

### Requirement: 向后兼容的全局访问

拆分后，浏览器控制台中 SHALL 可通过 `window.QuizState` 和 `window.QuizUI` 访问测验状态和 UI 方法，便于调试。

#### Scenario: 调试接口可用

- **WHEN** 开发者在控制台输入 `QuizState.answers`
- **THEN** SHALL 返回当前答题记录对象
