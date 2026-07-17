# 统一数据注册中心

## Purpose

消除 reading-radar 中 `quiz-tech.js`、`radar-tech.js`、`kanban-data.js` 三份数据文件对同一批知识点的重复定义。所有知识点的公共元数据（id/title/stack/quadrant/ring/desc）在 `data/items-registry.js` 中唯一定义，三个消费者文件通过过滤和映射自动派生各自的视图。

## ADDED Requirements

### Requirement: ITEMS_REGISTRY 唯一定义知识点

系统 SHALL 在 `data/items-registry.js` 中定义 `ITEMS_REGISTRY` 对象，以知识点 `id` 为键，包含 `stack`、`quadrant`、`ring`、`title`、`desc` 等公共字段。所有技术栈的所有知识点 SHALL 在此唯一定义。

#### Scenario: Registry 包含所有技术栈

- **WHEN** 页面加载 `items-registry.js`
- **THEN** `ITEMS_REGISTRY` SHALL 包含 C/CPP/DS/DB/PY 五个技术栈的 240 个知识点条目
- **THEN** 每个条目 SHALL 以 `id` 为键，具备 `stack`、`quadrant`、`ring`、`title`、`desc` 字段

### Requirement: quiz-tech.js 从 Registry 派生

`quiz-tech.js` 中的 `C_TECH_DATA`、`CPP_TECH_DATA` 等变量 SHALL 通过过滤 `ITEMS_REGISTRY`（按 `stack` 字段）并映射为 `{id, title, quadrant, ring, desc}` 格式生成。全局变量名和数据结构 SHALL 保持不变。

#### Scenario: C_TECH_DATA 从 Registry 生成

- **WHEN** 页面加载 `items-registry.js` 后加载 `quiz-tech.js`
- **THEN** `C_TECH_DATA.length` SHALL 等于 ITEMS_REGISTRY 中 `stack === "c"` 的条目数
- **THEN** `C_TECH_DATA[0]` SHALL 保持 `{id, title, quadrant, ring, desc}` 格式不变

### Requirement: radar-tech.js 从 Registry 派生

`radar-tech.js` 中的 `C_RADAR_DATA` 等变量 SHALL 通过过滤 `ITEMS_REGISTRY` 并映射为 `{id, title, quadrant, ring, books}` 格式生成。`books` 字段 SHALL 从 ITEMS_REGISTRY 中读取，缺失时默认为空数组 `[]`。

#### Scenario: 数据兼容性

- **WHEN** 页面加载所有数据文件后
- **THEN** `C_RADAR_DATA.length` SHALL 等于 `C_TECH_DATA.length`
- **THEN** 雷达图渲染 SHALL 与迁移前表现一致

### Requirement: kanban-data.js 从 Registry 派生

`kanban-data.js` 中的 `C_KANBAN_DATA` 等变量 SHALL 通过过滤 `ITEMS_REGISTRY` 并映射为 `{id, title, quadrant, ring, tags, status}` 格式生成。`tags` 字段缺失时默认为空数组，`status` 默认为 `"todo"`。

#### Scenario: Kanban 数据向后兼容

- **WHEN** 页面加载所有数据文件后
- **THEN** `C_KANBAN_DATA[0].status` SHALL 为 `"todo"`（默认值）
- **THEN** 看板渲染 SHALL 与迁移前表现一致

### Requirement: 新增技术栈只需修改 Registry

当新增技术栈（如 Linux）时，只需在 `ITEMS_REGISTRY` 中追加条目，`quiz-tech.js`、`radar-tech.js`、`kanban-data.js` SHALL 自动生成对应的全局变量，无需手动编辑。

#### Scenario: 追加 Linux 数据自动生效

- **WHEN** 在 ITEMS_REGISTRY 中追加 `stack: "linux"` 的 56 个条目
- **THEN** `getItemsByStack("linux")` SHALL 返回 56 个条目
- **THEN** `LINUX_TECH_DATA`、`LINUX_RADAR_DATA`、`LINUX_KANBAN_DATA` SHALL 自动生成
