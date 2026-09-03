# kanban-product-grouping

## Purpose

在看板中增加按产品分组的查看模式，知识点先按产品（MySQL、PostgreSQL、Redis、Faiss、Milvus 等）分组，组内再按象限排列。用户可在象限分组和产品分组间自由切换。

## Requirements

### Requirement: 知识点增加 product 字段

`ITEMS_REGISTRY` 中的知识点条目增加可选的 `product` 字段，标明该知识点归属的数据库产品（仅对 db 和 vdb 技术栈）。

#### Scenario: product 字段赋值
- **WHEN** items-registry 初始化完成
- **THEN** 传统 DB 条目（stack:"db"）中，专属某产品的条目（如 `db-redis-event`）的 `product` 字段值为该产品的 ID（如 `"redis"`）
- **AND** 跨产品通用条目（如 `db-acid`）的 `product` 字段值为 `"general"` 或不设置

### Requirement: 看板支持按产品分组

`kanban-render.js` 支持 `groupBy: "product"` 模式，知识点先按产品分组，组内再按象限排列。

#### Scenario: 产品分组视图
- **WHEN** 看板切换到产品分组模式
- **THEN** 看板按产品（MySQL、PostgreSQL、Redis、Faiss、Milvus 等）分为多个区域
- **AND** 每个产品区域内，知识点按象限（language/systems/algorithms/engineering）排列
- **AND** 每个产品区域显示该产品下知识点的总进度

#### Scenario: 默认分组不变
- **WHEN** 用户首次打开看板
- **THEN** 默认仍按象限分组（`groupBy: "quadrant"`）
- **AND** 用户可通过下拉或切换按钮切换到产品分组

### Requirement: 产品分组与象限分组的切换

看板提供分组模式切换器，用户在「象限分组」和「产品分组」间自由切换。

#### Scenario: 分组切换器
- **WHEN** 看板渲染
- **THEN** 分组切换器显示在当前看板顶部
- **AND** 切换器包含至少两个选项：象限分组、产品分组
- **AND** 切换时知识点状态（已学/学习中/待复习）不丢失
