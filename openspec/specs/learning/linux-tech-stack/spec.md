# Linux 技术栈

## Purpose

为 reading-radar 新增 Linux 技术栈，包含 56 个面向数据库内核开发者的知识点，覆盖资源观测、内核行为、存储文件系统、网络编程四大象限。得益于 Phase 0 的 `items-registry.js`（统一数据源），Linux 知识点只需在 registry 中定义一次，quiz-tech.js、radar-tech.js、kanban-data.js 自动派生各自的视图。题库按 4 象限拆分为 4 个文件（Phase 0 P4 策略）。

## Requirements

### Requirement: Linux 知识点在 Registry 中唯一定义

系统 SHALL 在 `items-registry.js` 的 `ITEMS_REGISTRY` 对象中新增 56 个 `stack: "linux"` 的知识点条目。每个条目 SHALL 具备 `id`、`title`、`stack`、`quadrant`、`ring`、`desc`、`tags` 字段。

知识点 SHALL 按四大象限组织：
- `observability`（资源观测与诊断）：16 个知识点
- `os-internals`（内核行为理解）：16 个知识点
- `storage`（存储与文件系统）：12 个知识点
- `networking`（网络与系统编程）：12 个知识点

每个象限 SHALL 包含 basic/intermediate/advanced 三个 ring 层级。

#### Scenario: Registry 包含 Linux 数据

- **WHEN** 页面加载 `items-registry.js`
- **THEN** `getItemsByStack("linux")` SHALL 返回 56 个知识点对象
- **THEN** 每个知识点对象的 `id` 字段 SHALL 以 `linux-` 为前缀

### Requirement: Linux 知识点描述面向数据库场景

每个 Linux 知识点的 `desc` 字段 SHALL 明确描述该知识点与数据库内核开发的关联，而非提供通用的 Linux 知识介绍。

#### Scenario: 知识点描述含数据库关联

- **WHEN** 用户查看任意 Linux 知识点的描述
- **THEN** 描述 SHALL 解释该知识点如何影响数据库的性能诊断、存储设计、并发控制或网络通信

### Requirement: TECH_DATA / RADAR_DATA / KANBAN_DATA 自动派生

`LINUX_TECH_DATA`、`LINUX_RADAR_DATA`、`LINUX_KANBAN_DATA` SHALL 由各自的数据文件通过过滤 `ITEMS_REGISTRY`（`stack === "linux"`）自动生成，无需手动定义。

#### Scenario: 三视图自动生成

- **WHEN** 页面加载 items-registry.js → quiz-tech.js → radar-tech.js → kanban-data.js
- **THEN** `LINUX_TECH_DATA.length` SHALL 为 56
- **THEN** `LINUX_RADAR_DATA` 中每条的 `id` 与 `LINUX_TECH_DATA` 一一对应
- **THEN** `LINUX_KANBAN_DATA` 中每条的 `id` 与 `LINUX_TECH_DATA` 一一对应

### Requirement: Linux 题库按象限拆分为 4 个文件

系统 SHALL 创建 4 个 Linux 题库文件（`quiz-questions-linux-observability.js` 等），每个对应一个象限。初期题库内容为空框架（知识点键存在但题目数组为空）。文件 SHALL 使用 `Object.assign(QUESTION_BANK.linux || {}, {...})` 安全合并。

#### Scenario: 4 个文件语法正确且可合并

- **WHEN** 执行 `node --check` 检查每个文件
- **THEN** 退出码 SHALL 为 0
- **WHEN** 4 个文件全部加载
- **THEN** `QUESTION_BANK.linux` SHALL 包含所有 4 个象限的知识点键

### Requirement: Linux 分类注册

系统 SHALL 在 `quiz-static.js` 的 `CATEGORIES` 对象中新增 `linux` 条目，配置 `id`、`label`（"Linux 技术栈"）、`icon`（"🐧"）、`items`（引用 `LINUX_TECH_DATA`）、`storagePrefix`（"quiz_linux_"）。

系统 SHALL 在 `QUADRANT_LABELS` 中新增 `linux` 条目，定义四大象限的中文标签映射。

#### Scenario: CATEGORIES 包含 Linux

- **WHEN** quiz-system.html 或 dashboard.html 遍历 `CATEGORIES` 生成技术栈按钮或进度卡片
- **THEN** Linux 技术栈 SHALL 出现在列表中，label 为 "Linux 技术栈"，icon 为 "🐧"

### Requirement: Linux 题库建设中提示

quiz-core.js 的 `loadQuestionBank("linux")` 在处理空题库时 SHALL 返回 `{ empty: true, message: "题库暂未开放" }`，quiz-ui.js 的 `showEmptyBankNotice()` SHALL 显示友好提示而非报错。

#### Scenario: 选择 Linux 技术栈进行测验

- **WHEN** 用户在 quiz-system.html 中选择 Linux 技术栈并尝试开始测验
- **THEN** 系统 SHALL 显示"题库暂未开放，敬请期待"提示信息
- **THEN** 系统 SHALL NOT 抛出 JavaScript 异常
