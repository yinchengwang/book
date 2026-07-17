# README 文档更新

## Purpose

为 reading-radar 项目提供完整的 README 文档，覆盖四层联动架构说明、文件结构、数据规范和各技术栈知识点覆盖统计。

## Requirements

### Requirement: README 覆盖四层联动架构说明

README.md SHALL 包含四层联动架构（Layer 1-4）的说明表格，描述每一层的页面、作用及联动触发条件。

#### Scenario: 架构表格存在

- **WHEN** 用户查看 README.md
- **THEN** 文档 SHALL 包含 Layer 1/2/3/4 的架构说明，含页面名称和功能描述

### Requirement: README 包含完整文件结构说明

README.md SHALL 列出 `project/reading-radar/` 下每个文件和 `data/` 下每个数据文件的路径与用途说明。

#### Scenario: 文件路径可追溯

- **WHEN** 新开发者阅读 README
- **THEN** 每个 `.html` 文件和 `data/*.js` 文件 SHALL 有一行简短说明，描述其作用

### Requirement: README 包含数据结构规范

README.md SHALL 说明题库文件的双层嵌套结构规范（`QUESTION_BANK[catId][catId][itemId]`）、题目对象字段定义、题型枚举。

#### Scenario: 数据规范章节存在

- **WHEN** 开发者需要新增题目
- **THEN** README SHALL 提供完整的题目格式说明和示例代码片段

### Requirement: README 包含各技术栈知识点数量说明

README.md SHALL 列出当前各技术栈（C/C++/DS/DB/Python）的知识点总数和题库覆盖情况。

#### Scenario: 知识点清单存在

- **WHEN** 用户查看 README
- **THEN** 文档 SHALL 包含一个汇总表，显示各技术栈知识点数量和题库状态（已完成/进行中）
