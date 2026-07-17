# 题库文件按象限拆分

## Purpose

将每个技术栈的题库文件按 4 个象限拆分为独立文件，减少单文件体积，降低 Claude 新增/编辑题目时的 Token 消耗。Linux 技术栈从 day 1 起采用此模式；现有题库（C/CPP/DS/DB/PY）保持单文件，后续按需迁移。

## Requirements

### Requirement: Linux 题库按 4 象限拆分

系统 SHALL 为 Linux 技术栈创建 4 个题库文件，每个对应一个象限：
- `quiz-questions-linux-observability.js`（资源观测与诊断）
- `quiz-questions-linux-os-internals.js`（内核行为理解）
- `quiz-questions-linux-storage.js`（存储与文件系统）
- `quiz-questions-linux-networking.js`（网络与系统编程）

#### Scenario: 4 个文件语法正确

- **WHEN** 对每个文件执行 `node --check`
- **THEN** 所有 4 个文件 SHALL 语法正确，退出码为 0

### Requirement: 安全合并到 QUESTION_BANK.linux

每个象限题库文件 SHALL 使用 `QUESTION_BANK.linux = Object.assign(QUESTION_BANK.linux || {}, {...})` 模式，确保无论文件加载顺序如何，所有象限的题目都能正确合并到 `QUESTION_BANK.linux` 中。

#### Scenario: 文件加载顺序无关

- **WHEN** 以任意顺序加载 4 个 Linux 题库文件
- **THEN** `QUESTION_BANK.linux` SHALL 包含所有 4 个象限的知识点键
- **THEN** `Object.keys(QUESTION_BANK.linux)` SHALL 不包含重复键

### Requirement: quiz-core.js 支持合并后的题库

`loadQuestionBank("linux")` SHALL 正确读取由多个文件合并后的 `QUESTION_BANK.linux` 对象。当所有象限文件均为空框架（仅有知识点键但无题目）时，SHALL 返回 `{ empty: true }`。

#### Scenario: 合并后的空题库处理

- **WHEN** 加载 4 个 Linux 题库文件后调用 `loadQuestionBank("linux")`
- **THEN** 若所有知识点键下均为空数组，SHALL 返回 `{ empty: true }`
- **THEN** SHALL NOT 抛出异常

### Requirement: 现有题库不受影响

现有题库文件（quiz-questions-c.js、quiz-questions-cpp.js、quiz-questions-ds.js、quiz-questions-db.js、quiz-questions-py.js）SHALL 保持单文件结构不变，加载方式不变。

#### Scenario: 现有题库正常加载

- **WHEN** quiz-system.html 加载现有题库文件
- **THEN** `QUESTION_BANK.c`、`QUESTION_BANK.cpp` 等 SHALL 与迁移前数据完全一致
- **THEN** 现有技术栈的测验功能 SHALL 正常运作

### Requirement: 文件命名规范

按象限拆分的题库文件 SHALL 遵循命名规范 `quiz-questions-<stack>-<quadrant>.js`，其中 `<stack>` 为技术栈标识符（小写），`<quadrant>` 为象限标识符（小写，连字符分隔）。

#### Scenario: 文件命名一致

- **WHEN** 查看 data/ 目录
- **THEN** Linux 题库文件 SHALL 命名为 `quiz-questions-linux-observability.js` 等格式
- **THEN** 文件名中的象限标识 SHALL 与 `QUADRANT_LABELS.linux` 中的键一致
