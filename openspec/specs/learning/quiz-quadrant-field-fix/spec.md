# quiz-quadrant-field-fix

## Purpose

在所有迁移后的题目对象中补充 `quadrant` 和 `ring` 字段，确保象限筛选和联合筛选（象限+难度）功能正确工作。

## Requirements

### Requirement: 题目对象补充 quadrant 字段

所有迁移到新文件中的题目对象显式携带 `quadrant` 字段，字段值从原文件中的注释分组位置或 items-registry 推导。

#### Scenario: 按注释分区推导
- **WHEN** 拆分 `quiz-questions-c/cpp/ds/py.js` 时
- **THEN** 原来在 `语言核心` 注释组下的题目获得 `quadrant: "language"`
- **AND** 原来在 `系统编程` / `内存与并发` 等系统类注释组下的题目获得 `quadrant: "systems"`
- **AND** 原来在 `算法` / `数据结构与算法` 等注释组下的题目获得 `quadrant: "algorithms"`
- **AND** 原来在 `工程` 等注释组下的题目获得 `quadrant: "engineering"`

#### Scenario: 按 items-registry 推导
- **WHEN** 拆分 `quiz-questions-db.js` 时
- **THEN** 每个知识点 ID 对应的题目从 `ITEMS_REGISTRY[<id>].quadrant` 读取象限值
- **AND** 如果没有匹配的 registry 条目，手动审查并标注

### Requirement: 题目对象补充 ring 字段

所有迁移题目对象补充 `ring` 字段，值等于已有的 `difficulty` 字段。

#### Scenario: ring 与 difficulty 一致
- **WHEN** 迁移完成后
- **THEN** 每个题目的 `ring` 值等于其 `difficulty` 值
- **AND** `difficulty` 字段保留不动（向后兼容）

### Requirement: 象限筛选功能验证

修复后的象限筛选能正确按象限和难度筛选题目。

#### Scenario: 象限筛选可用
- **WHEN** 用户选择一个具体象限（如 C 技术栈→语言核心）
- **THEN** `selectQuestions("c", "language", null, 10)` 返回的题目全部属于 `quadrant: "language"`
- **AND** 结果数量不为 0（假设该象限有足够题目）

#### Scenario: 象限+难度联合筛选
- **WHEN** 用户选择象限+难度（如 C 技术栈→语言核心→进阶）
- **THEN** `selectQuestions("c", "language", "advanced", 10)` 返回的题目全部属于 `quadrant:"language"` 且 `ring:"advanced"`
