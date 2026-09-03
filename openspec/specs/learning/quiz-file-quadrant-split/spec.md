# quiz-file-quadrant-split

## Purpose

将现有的 5 个大题库文件（`quiz-questions-c/cpp/ds/db/py.js`）按象限拆分为 4 个独立小文件，每文件 ~300-600 行，通过 `Object.assign` 合并回 `QUESTION_BANK`。同步更新 HTML 加载标签，并删除旧的大文件。

## Requirements

### Requirement: 按象限拆分题库文件

现有 5 个大题库文件（`quiz-questions-c/cpp/ds/db/py.js`）每个按象限拆分为 4 个独立小文件，每文件 ~300-600 行。拆分后文件用 `Object.assign` 合并回 `QUESTION_BANK`。

#### Scenario: 文件拆分粒度
- **WHEN** 完成拆分后
- **THEN** 每个技术栈的题库分布在 4 个象限文件中，文件路径为 `data/quiz-questions-<stack>-<quadrant>.js`
- **AND** 单文件行数不超过 800 行

#### Scenario: QUESTION_BANK 合并
- **WHEN** 页面加载所有 `<script>` 标签后
- **THEN** `QUESTION_BANK.<stack>` 应包含该技术栈完整的题目集合
- **AND** 象限筛选 `selectQuestions(categoryId, quadrant, ring, count)` 能正确过滤出对应象限的题目

#### Scenario: 原有题目数据不丢失
- **WHEN** 拆分迁移完成后
- **THEN** `QUESTION_BANK` 中各技术栈的题目总数与拆分前完全一致

### Requirement: HTML 加载标签更新

`quiz-system.html` 的 `<script>` 加载标签随文件拆分同步更新。

#### Scenario: 全量加载
- **WHEN** 用户打开 quiz-system.html 页面
- **THEN** 页面的 `<script>` 标签包含所有技术栈、所有象限的题库文件
- **AND** 标签加载顺序为先加载 `quiz-core.js` + `quiz-static.js` + `quiz-tech.js` + `quiz-ui.js`，再加载各题库文件

### Requirement: 删除旧文件

拆分完成后，原有的 5 个大文件被删除。

#### Scenario: 文件清理
- **WHEN** 拆分验证通过后
- **THEN** `data/quiz-questions-c.js`、`data/quiz-questions-cpp.js`、`data/quiz-questions-ds.js`、`data/quiz-questions-db.js`、`data/quiz-questions-py.js` 被删除
- **AND** git 历史可追溯旧文件内容
