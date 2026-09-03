# 题库扩充

## Purpose

将 Python、C、C++、数据结构各技术栈的每个知识点题目数量扩充至不少于 10 道，覆盖多种难度和题型，分批执行逐批验证。

## Requirements

### Requirement: 每个知识点题目不少于 10 道

Python、C、C++、数据结构各技术栈的每个知识点 SHALL 拥有至少 10 道题目，覆盖 basic/intermediate/advanced 难度及 choice/predict_output/true_false/fill_blank/multi_choice 多种题型。

#### Scenario: 扩充后题目数量验证

- **WHEN** 对题库文件执行 `Select-String -Pattern 'id:"<catId>-q\d+"'` 统计
- **THEN** 每个知识点的匹配数量 SHALL 大于等于 10

#### Scenario: 语法正确性验证

- **WHEN** 执行 `node --check <题库文件>`
- **THEN** 退出码 SHALL 为 0（无语法错误）

### Requirement: 题目格式规范一致

新增题目 SHALL 遵循统一的 JS 对象格式：`{ id, type, difficulty, scenario, stem, options, answer, explanation, code? }`。

#### Scenario: 题目字段完整

- **WHEN** 任意一道新题被加入题库
- **THEN** 该题 SHALL 包含 id、type、difficulty、scenario、stem、options、answer、explanation 全部必填字段

#### Scenario: ID 命名规范

- **WHEN** 新增题目时分配 ID
- **THEN** ID SHALL 遵循 `<catId>-q<序号>` 格式（如 py-stdlib-q6），序号从已有最大值 +1 开始

### Requirement: 新题与现有题风格一致

新增题目 SHALL 采用工程实践导向的 scenario（情景化描述），避免纯理论。

#### Scenario: 情景化描述

- **WHEN** 编写新题的 scenario 字段
- **THEN** scenario SHALL 描述具体工作场景（如"你在排查内存泄漏"、"你要在 CI 中加速构建"）

### Requirement: 分批执行、逐批验证

题库扩充 SHALL 按批次执行（每批 4 个知识点），每批执行后必须通过 node --check 验证才能继续。

#### Scenario: 批次边界清晰

- **WHEN** 完成一个批次（A3/A4/...）的插入
- **THEN** 必须先验证语法和题目数量，验证通过后才执行下一批
