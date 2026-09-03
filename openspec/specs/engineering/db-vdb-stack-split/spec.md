# db-vdb-stack-split

## Purpose

将向量数据库从传统 DB 技术栈中拆分出来，作为独立的技术栈进行管理，包括导航栏入口、CATEGORIES 注册、象限标签体系和题库归属。

## Requirements

### Requirement: 新增 vdb 技术栈

在现有 `c/cpp/ds/db/py/linux` 六个技术栈外，新增 `vdb`（向量数据库）技术栈，作为独立的导航项、CATEGORIES 条目、象限标签体系存在。

#### Scenario: CATEGORIES 注册
- **WHEN** 页面加载完成
- **THEN** `CATEGORIES` 对象中包含 `vdb` 键
- **AND** `CATEGORIES.vdb` 包含 `id/label/icon/items/quadrantLabels/storagePrefix` 字段

#### Scenario: 导航栏入口
- **WHEN** 导航栏渲染
- **THEN** 导航栏中出现「向量DB」或等效入口，点击后跳转到 `quiz-system.html?stack=vdb`
- **AND** 其他页面（如 dashboard）的导航也同时更新

#### Scenario: 象限标签
- **WHEN** 用户进入 vdb 技术栈的测验
- **THEN** 象限选择器显示 vdb 专属的象限标签（向量基础/索引算法/量化与压缩/系统与工程）

#### Scenario: 题库归属
- **WHEN** 用户选择 vdb 技术栈进行答题
- **THEN** `loadQuestionBank("vdb")` 返回向量数据库的题目，而非传统 DB 的题目
- **AND** `QUESTION_BANK.vdb` 与 `QUESTION_BANK.db` 互不干扰

### Requirement: 技术栈列表更新

全局技术栈列表（quiz-tech.js、dashboard 的技术栈卡片等）包含 vdb。

#### Scenario: 技术栈元数据
- **WHEN** `quiz-tech.js` 执行
- **THEN** `VDB_TECH_DATA` 数组被创建，内容为 items-registry 中 `stack:"vdb"` 的条目
- **AND** 渲染仪表盘时 vdb 有独立的技术栈卡片
