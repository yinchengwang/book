## ADDED Requirements

### Requirement: 填充 Linux 题库内容

将现有的 4 个 Linux 题库空壳文件（`quiz-questions-linux-observability.js`、`quiz-questions-linux-os-internals.js`、`quiz-questions-linux-storage.js`、`quiz-questions-linux-networking.js`）填充为包含实际题目的有效文件。

#### Scenario: 文件内容
- **WHEN** 填充完成后
- **THEN** 每个 Linux 题库文件不再是空对象，而是包含该象限对应的实际题目对象
- **AND** 每个文件至少包含 5 个知识点、每个知识点至少 2 道题

#### Scenario: 题目来源
- **WHEN** 编写题目时
- **THEN** 题目内容基于 `ITEMS_REGISTRY` 中 `stack:"linux"` 的 56 个知识点条目生成
- **AND** 每个题目的 `quadrant` 字段与其知识点的 `quadrant` 匹配
- **AND** 题目难度与知识点的 `ring` 对应

#### Scenario: 题目类型多样性
- **WHEN** 填充 Linux 题库时
- **THEN** 每象限包含至少 2 种不同的题型（choice / true_false / multi_choice / fill_blank / predict_output 中至少 2 种）

### Requirement: 学习模式兼容

新填充的 Linux 题库在全象限筛选、全部难度、用户学习模式中正常工作。

#### Scenario: 全量选题
- **WHEN** 用户选择 Linux 技术栈并开始答题
- **THEN** `loadQuestionBank("linux")` 返回完整 Linux 题库
- **AND** `selectQuestions("linux", null, null, 10)` 从所有象限和难度随机选题

#### Scenario: 象限筛选
- **WHEN** 用户选择 Linux 技术栈下的特定象限（如「资源观测与诊断」）
- **THEN** `selectQuestions("linux", "language", null, 10)` 只返回 `quadrant:"language"` 的题目
- **AND** 返回结果不为空
